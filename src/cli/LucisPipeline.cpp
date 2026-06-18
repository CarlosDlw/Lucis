#include "cli/LucisPipeline.h"
#include "parser/Parser.h"
#include "checkers/Checker.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CHeaderResolver.h"
#include "imports/ImportResolver.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <deque>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

bool useAnsiStderr() {
    static const bool enabled = (::isatty(STDERR_FILENO) == 1);
    return enabled;
}

void printProgressLine(const char* stage, int current, int total, const std::string& msg) {
    int pct = (total > 0) ? (current * 100 / total) : 100;
    std::cerr << "lucis: ";
    if (useAnsiStderr()) std::cerr << "\033[1;36m";
    std::cerr << "[" << stage << " " << std::setw(3) << pct << "%]";
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << " " << msg << "\n";
}

void printUnitLine(const char* stage, const char* phase,
                    size_t idx, size_t total, const std::string& path) {
    std::cerr << "lucis: ";
    if (useAnsiStderr()) std::cerr << "\033[1;33m";
    std::cerr << "[" << stage << " " << phase << " " << idx << "/" << total << "]";
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << " " << path << "\n";
}

void printErrorLine(const std::string& msg) {
    std::cerr << "lucis: ";
    if (useAnsiStderr()) std::cerr << "\033[1;31m";
    std::cerr << msg;
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << "\n";
}

} // namespace

// ── Static helpers ────────────────────────────────────────────────────────────

std::string LucisPipeline::getProjectRoot(const std::string& inputFile) {
    auto dir = fs::path(inputFile).parent_path();
    if (dir.empty()) dir = ".";
    dir = fs::canonical(dir);

    for (auto ancestor = dir; ; ancestor = ancestor.parent_path()) {
        if (fs::exists(ancestor / "lucis.yaml"))
            return ancestor.string();
        if (fs::exists(ancestor / ".git"))
            return ancestor.string();
        if (ancestor == ancestor.parent_path()) break;
    }
    return dir.string();
}

std::string LucisPipeline::filePathToModulePath(const std::string& filePath,
                                                  const std::string& projectRoot) {
    fs::path absPath = fs::canonical(filePath);
    fs::path rel = fs::relative(absPath, projectRoot);
    std::string result = rel.replace_extension("").string();
    // Normalize backslashes to forward slashes
    for (auto& c : result) {
        if (c == '\\') c = '/';
    }
    return result;
}

std::string LucisPipeline::resolveUseToFile(const std::string& useIdent,
                                             const std::string& projectRoot,
                                             const std::vector<std::string>& searchDirs,
                                             const std::vector<std::string>& sourcePaths) {
    // First convert "lib::math" → "lib/math" (module path)
    std::string modPath;
    {
        std::string tmp = useIdent;
        for (auto& c : tmp) if (c == ':') c = '/';
        for (size_t i = 0; i < tmp.size(); i++) {
            if (tmp[i] == '/' && i + 1 < tmp.size() && tmp[i+1] == '/') {
                modPath += '/'; // normalize "::" → single "/"
                i++; // skip the doubled slash
                continue;
            }
            modPath += tmp[i];
        }
    }

    // Build search directories: project root, then source paths, then stdlib
    std::vector<fs::path> searchPaths;
    searchPaths.push_back(fs::path(projectRoot));
    for (auto& sp : sourcePaths)
        searchPaths.push_back(fs::path(projectRoot) / sp);
    for (auto& sp : searchDirs)
        searchPaths.push_back(fs::path(sp));

    // Try to locate modPath + ".lc" in each search directory
    for (auto& base : searchPaths) {
        auto candidate = base / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec)
            return fs::canonical(candidate, ec).string();
    }
    return {};
}

std::vector<std::string> LucisPipeline::extractUseModulePaths(
        LucisParser::ProgramContext* tree) {

    std::vector<std::string> result;
    // Check both preamble and top-level use declarations
    auto extract = [&](auto* decl) {
        if (auto* use = decl->useDecl()) {
            if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(use)) {
                result.push_back(root->IDENTIFIER()->getText());
            } else if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(use)) {
                // Reconstruct module path from the modulePath rule
                std::string path;
                for (auto* id : item->modulePath()->IDENTIFIER())
                    path += (path.empty() ? "" : "::") + id->getText();
                result.push_back(path);
            } else if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(use)) {
                std::string path;
                for (auto* id : group->modulePath()->IDENTIFIER())
                    path += (path.empty() ? "" : "::") + id->getText();
                result.push_back(path);
            } else if (auto* wild = dynamic_cast<LucisParser::UseEnumWildcardContext*>(use)) {
                // use Type::* — module path comes from the type spec
                // For now, just skip wildcard imports in module resolution
                (void)wild;
            }
        }
    };

    for (auto* pre : tree->preambleDecl()) extract(pre);
    for (auto* top : tree->topLevelDecl()) {
        if (auto* use = top->useDecl()) {
            // Top-level use declarations (not using preamble)
            if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(use)) {
                result.push_back(root->IDENTIFIER()->getText());
            } else if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(use)) {
                std::string path;
                for (auto* id : item->modulePath()->IDENTIFIER())
                    path += (path.empty() ? "" : "::") + id->getText();
                result.push_back(path);
            } else if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(use)) {
                std::string path;
                for (auto* id : group->modulePath()->IDENTIFIER())
                    path += (path.empty() ? "" : "::") + id->getText();
                result.push_back(path);
            }
        }
    }

    return result;
}

// ── Pipeline runner ───────────────────────────────────────────────────────────

std::unique_ptr<PipelineResult> LucisPipeline::run(const Options& opts) {
    auto result = std::make_unique<PipelineResult>();
    const char* stage = "build";

    auto progress = [&](int step, int total, const std::string& msg) {
        if (opts.quiet) return;
        printProgressLine(stage, step, total, msg);
    };

    // ── Step 1: project root & build dir ────────────────────────────────────
    progress(1, 5, "resolving project root");
    result->projectRoot = getProjectRoot(opts.inputFile);
    result->buildDir = result->projectRoot + "/.lucis/build";
    fs::create_directories(result->buildDir);

        // Build search directories: stdlib paths + import resolver fallbacks
    std::vector<std::string> searchDirs = opts.stdlibPaths;
    for (auto& p : ImportResolver::stdlibSearchPaths())
        searchDirs.push_back(p);

    // ── Step 2: resolve imports transitively (BFS) ─────────────────────────
    progress(2, 5, "resolving import tree");
    std::unordered_set<std::string> visited;
    // Queue stores (filePath, logicalModulePath, importChain)
    // For entry point: modulePath derived from file path
    // For use-resolved files: modulePath from usePath converted to "/" format
    // importChain tracks the path from entry to current module for cycle detection
    auto entryModPath = filePathToModulePath(opts.inputFile, result->projectRoot);
    std::deque<std::tuple<std::string, std::string, std::vector<std::string>>> queue;
    queue.emplace_back(opts.inputFile, entryModPath,
        std::vector<std::string>{entryModPath});
    visited.insert(opts.inputFile);

    bool anyParseError = false;
    bool anyCircularImport = false;
    size_t totalUnits = 0;

    while (!queue.empty()) {
        auto [filePath, modulePath, importChain] = std::move(queue.front());
        queue.pop_front();

        SourceUnit unit;
        unit.filePath    = filePath;
        unit.modulePath  = modulePath;
        unit.parseResult = Parser::parse(filePath);

        if (unit.parseResult.hasErrors) {
            printErrorLine("parse errors in '" + filePath + "'");
            anyParseError = true;
            continue;
        }

        result->units.push_back(std::move(unit));
        totalUnits = result->units.size();
        if (!opts.quiet) {
            printUnitLine(stage, "parse", totalUnits, 0, filePath);
        }

        // Extract use declarations and enqueue their module files
        auto usePaths = extractUseModulePaths(unit.parseResult.tree);
        for (auto& usePath : usePaths) {
            auto modFile = resolveUseToFile(usePath, result->projectRoot, searchDirs, opts.sourcePaths);
            if (modFile.empty()) continue;

            auto logicalPath = ModuleRegistry::usePathToModulePath(usePath);

            // Circular import detection: check if this module is already in the current chain
            if (std::find(importChain.begin(), importChain.end(), logicalPath) != importChain.end()) {
                std::string chain;
                for (auto& m : importChain) chain += m + " → ";
                chain += logicalPath;
                printErrorLine("circular import detected: " + chain);
                anyCircularImport = true;
                continue;
            }

            if (visited.find(modFile) != visited.end()) continue;

            visited.insert(modFile);
            auto childChain = importChain;
            childChain.push_back(logicalPath);
            queue.emplace_back(modFile, logicalPath, std::move(childChain));
        }
    }

    if (anyCircularImport) {
        result->hasErrors = true;
        return result;
    }

    if (result->units.empty()) {
        printErrorLine("no .lc files found or imported from '" + opts.inputFile + "'");
        result->hasErrors = true;
        return result;
    }

    // ── Step 3: module registry ──────────────────────────────────────────
    progress(3, 5, "building module registry");
    result->registry = std::make_unique<ModuleRegistry>();
    for (auto& unit : result->units)
        result->registry->registerFile(unit.modulePath, unit.filePath, unit.parseResult.tree);
    auto dupErrors = result->registry->validate();
    if (!dupErrors.empty()) {
        for (auto& err : dupErrors) printErrorLine(err);
        result->hasErrors = true;
        return result;
    }

    // ── Step 4: resolve C headers & compile C sources ─────────────────────
    progress(4, 5, "resolving C includes and auto-link");
    result->cBindings = std::make_unique<CBindings>();
    result->cTypeReg  = std::make_unique<TypeRegistry>();
    {
        CHeaderResolver resolver(*result->cTypeReg, *result->cBindings, opts.includePaths);
        for (auto& unit : result->units) {
            auto* tree = unit.parseResult.tree;
            for (auto* pre : tree->preambleDecl()) {
                auto* incl = pre->includeDecl();
                if (!incl) continue;
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(incl->getText());
                    if (!header.empty())
                        resolver.resolveSystemHeader(header);
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(incl->getText());
                    if (!header.empty()) {
                        resolver.resolveLocalHeader(header, result->projectRoot);
                        auto hPath = fs::path(result->projectRoot) / header;
                        auto cPath = fs::path(hPath).replace_extension(".c");
                        if (fs::exists(cPath)) {
                            auto canonical = fs::canonical(cPath).string();
                            if (std::find(result->cSourceFiles.begin(),
                                          result->cSourceFiles.end(),
                                          canonical) == result->cSourceFiles.end())
                                result->cSourceFiles.push_back(canonical);
                        }
                    }
                }
            }
        }
    }

    for (auto& [flag, header] : result->cBindings->requiredLibs()) {
        bool alreadyProvided = false;
        for (auto& lf : opts.userLinkerFlags) {
            if (("-l" + lf) == flag) { alreadyProvided = true; break; }
        }
        if (!alreadyProvided) {
            if (!opts.quiet)
                std::cerr << "lucis: auto-linking '" << flag
                          << "' (required by <" << header << ">)\n";
            result->linkerFlags.push_back(flag);
        }
    }
    for (auto& lf : opts.userLinkerFlags)
        result->linkerFlags.push_back("-l" + lf);

    // ── Step 5: semantic check ──────────────────────────────────────────────
    progress(5, 5, "running semantic checker");

    // Phase 1: create shared SemanticDB
    result->semanticDB = std::make_unique<semantic::SemanticDB>();

    bool anyCheckError = false;
    size_t checkIdx = 0;
    for (auto& unit : result->units) {
        ++checkIdx;
        if (!opts.quiet)
            printUnitLine(stage, "check", checkIdx, result->units.size(), unit.filePath);
        Checker checker;
        checker.setModuleContext(result->registry.get(), unit.modulePath, unit.filePath);
        checker.setCBindings(result->cBindings.get());
        checker.setSemanticDB(result->semanticDB.get());
        bool passed = checker.check(unit.parseResult.tree);
        for (auto& err : checker.errors())
            printErrorLine(unit.filePath + ": " + err);
        if (!passed) anyCheckError = true;
    }
    if (anyCheckError) {
        result->hasErrors = true;
        return result;
    }

    // Phase 5: save SemanticDB cache for incremental builds
    if (result->semanticDB) {
        std::string cacheDir = result->buildDir + "/cache";
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (!ec) {
            result->semanticDB->save(cacheDir + "/semantic.db");
        }
    }

    return result;
}
