#include "lsp/DiagnosticEngine.h"
#include "lsp/ProjectContext.h"
#include "parser/Parser.h"
#include "checkers/Checker.h"
#include "ffi/CBindings.h"
#include "ffi/CHeaderResolver.h"

#include <filesystem>

namespace fs = std::filesystem;

// Parse checker error string "line:col: message" into a Diagnostic.
static Diagnostic parseCheckerError(const std::string& err) {
    Diagnostic d;
    d.severity = Diagnostic::Error;

    // Format: "line:col: message" (1-based)
    size_t firstColon = err.find(':');
    if (firstColon == std::string::npos) {
        d.message = err;
        return d;
    }

    size_t secondColon = err.find(':', firstColon + 1);
    if (secondColon == std::string::npos) {
        d.message = err;
        return d;
    }

    int line = std::atoi(err.substr(0, firstColon).c_str());
    int col  = std::atoi(err.substr(firstColon + 1, secondColon - firstColon - 1).c_str());

    // Skip ": " after second colon
    size_t msgStart = secondColon + 1;
    if (msgStart < err.size() && err[msgStart] == ' ') {
        msgStart++;
    }

    d.line    = (line > 0) ? static_cast<size_t>(line - 1) : 0;  // to 0-based
    d.col     = (col > 0) ? static_cast<size_t>(col - 1) : 0;    // to 0-based
    d.endLine = d.line;
    d.endCol  = d.col + 1;
    d.message = err.substr(msgStart);

    return d;
}

std::vector<Diagnostic> DiagnosticEngine::run(const std::string& source,
                                               const std::string& filePath,
                                               ParseResult* preParsed) {
    std::vector<Diagnostic> result;

    // Step 1: Parse
    ParseResult localParseStorage;
    auto& parsed = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);

    // Collect parse errors (syntax errors are always valid without project context)
    for (auto& d : parsed.diagnostics) {
        result.push_back(std::move(d));
    }

    // Step 2: Semantic checking
    if (parsed.tree && !parsed.hasErrors) {
        Checker checker;

        // Resolve C header includes so FFI functions are known
        CBindings cBindings;
        TypeRegistry cTypeReg;
        std::vector<LucisParser::IncludeDeclContext*> includes;
        for (auto* pre : parsed.tree->preambleDecl())
            if (auto* inc = pre->includeDecl()) includes.push_back(inc);
        if (!includes.empty()) {
            CHeaderResolver resolver(cTypeReg, cBindings);
            for (auto* incl : includes) {
                auto text = incl->getText();
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(text);
                    if (!header.empty())
                        resolver.resolveSystemHeader(header);
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(text);
                    if (!header.empty())
                        resolver.resolveLocalHeader(header, fs::path(filePath).parent_path().string());
                }
            }
            checker.setCBindings(&cBindings);
        }

        checker.check(parsed.tree);
        for (auto& d : checker.diagnostics()) {
            result.push_back(d);
        }
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  Project-aware diagnostics
// ═══════════════════════════════════════════════════════════════════════

std::vector<Diagnostic> DiagnosticEngine::run(const std::string& source,
                                               const std::string& filePath,
                                               const ProjectContext* project,
                                               ParseResult* preParsed) {
    if (!project || !project->isValid())
        return run(source, filePath, preParsed);

    std::vector<Diagnostic> result;

    // Step 1: Parse
    ParseResult localParseStorage;
    auto& parsed = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);

    for (auto& d : parsed.diagnostics) {
        result.push_back(std::move(d));
    }

    // Step 2: Semantic checking with full project context
    if (parsed.tree && !parsed.hasErrors) {
        Checker checker;

        // Set module context from the project registry.
        std::string modPath = project->modulePathFor(filePath);
        checker.setModuleContext(&project->registry(), modPath, filePath);

        // Pass project paths for filesystem-based module fallback.
        checker.setProjectPaths(project->projectRoot(), project->sourcePaths());

        // Use project-wide C bindings.
        checker.setCBindings(&project->cBindings());

        // Only resolve C headers locally if the project doesn't already have bindings.
        // This avoids re-parsing C headers on every keystroke when the project
        // context already scanned them.
        CBindings localBindings;
        TypeRegistry localTypeReg;
        if (project->cBindings().functions().empty()) {
            std::vector<LucisParser::IncludeDeclContext*> includes;
            for (auto* pre : parsed.tree->preambleDecl())
                if (auto* inc = pre->includeDecl()) includes.push_back(inc);
            if (!includes.empty()) {
                CHeaderResolver resolver(localTypeReg, localBindings);
                for (auto* incl : includes) {
                    auto text = incl->getText();
                    if (incl->INCLUDE_SYS()) {
                        auto header = CHeaderResolver::extractSystemHeader(text);
                        if (!header.empty())
                            resolver.resolveSystemHeader(header);
                    } else if (incl->INCLUDE_LOCAL()) {
                        auto header = CHeaderResolver::extractLocalHeader(text);
                        if (!header.empty())
                            resolver.resolveLocalHeader(header, project->projectRoot());
                    }
                }
            }
        }
        if (!localBindings.functions().empty() || !localBindings.structs().empty())
            checker.setCBindings(&localBindings);
        else
            checker.setCBindings(&project->cBindings());

        checker.check(parsed.tree);
        for (auto& d : checker.diagnostics()) {
            result.push_back(d);
        }
    }

    // Step 3: Project-level errors (e.g. circular imports)
    for (auto& ie : project->importErrors()) {
        if (ie.filePath == filePath) {
            Diagnostic d;
            d.severity = Diagnostic::Error;
            d.source = "lucis";
            d.code = "circular-import";
            d.message = ie.message;
            d.line    = ie.line;
            d.col     = ie.col;
            d.endLine = ie.line;
            d.endCol  = ie.col + 1;
            result.push_back(std::move(d));
        }
    }

    return result;
}
