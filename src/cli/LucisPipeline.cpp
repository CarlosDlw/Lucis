#include "cli/LucisPipeline.h"
#include "parser/Parser.h"
#include "checkers/Checker.h"
#include "namespace/ProjectScanner.h"
#include "namespace/NamespaceRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CHeaderResolver.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
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
    auto parent = fs::path(inputFile).parent_path();
    if (parent.empty()) parent = ".";
    return fs::canonical(parent).string();
}

std::string LucisPipeline::extractNamespace(LucisParser::ProgramContext* tree) {
    if (auto* nsDecl = tree->namespaceDecl())
        return nsDecl->IDENTIFIER()->getText();
    return {};
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
    result->buildDir    = result->projectRoot + "/.lucis/build";
    fs::create_directories(result->buildDir);
    auto buildProgress = [&, step = 1]() mutable { progress(++step, 5, "scanning .lc files"); };
    buildProgress();
    // We can't use a lambda chain here cleanly, so let's use a step counter
    int step = 1;

    // ── Step 2: scan ────────────────────────────────────────────────────────
    progress(++step, 5, "scanning .lc files");
    auto allFiles = ProjectScanner::scan(result->projectRoot);
    if (allFiles.empty()) {
        printErrorLine("no .lc files found in '" + result->projectRoot + "'");
        result->hasErrors = true;
        return result;
    }

    // ── Step 3: parse ───────────────────────────────────────────────────────
    progress(++step, 5, "parsing source files");
    bool anyParseError = false;
    const size_t parseTotal = allFiles.size();
    size_t parseIdx = 0;
    for (auto& filePath : allFiles) {
        ++parseIdx;
        if (!opts.quiet)
            printUnitLine(stage, "parse", parseIdx, parseTotal, filePath);
        SourceUnit unit;
        unit.filePath    = filePath;
        unit.parseResult = Parser::parse(filePath);
        if (unit.parseResult.hasErrors) {
            printErrorLine("parse errors in '" + filePath + "'");
            anyParseError = true;
            continue;
        }
        unit.namespaceName = extractNamespace(unit.parseResult.tree);
        if (unit.namespaceName.empty()) {
            printErrorLine("file '" + filePath + "' is missing a 'namespace' declaration");
            anyParseError = true;
            continue;
        }
        result->units.push_back(std::move(unit));
    }
    if (anyParseError) {
        result->hasErrors = true;
        return result;
    }

    // ── Step 4: namespace registry ──────────────────────────────────────────
    progress(++step, 5, "building namespace registry");
    result->registry = std::make_unique<NamespaceRegistry>();
    for (auto& unit : result->units)
        result->registry->registerFile(unit.namespaceName, unit.filePath, unit.parseResult.tree);
    auto dupErrors = result->registry->validate();
    if (!dupErrors.empty()) {
        for (auto& err : dupErrors) printErrorLine(err);
        result->hasErrors = true;
        return result;
    }

    // ── Step 4.5: resolve C headers & compile C sources ─────────────────────
    progress(step, 5, "resolving C includes and auto-link");
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
                        auto base = fs::path(unit.filePath).parent_path().string();
                        resolver.resolveLocalHeader(header, base);
                        auto hPath = fs::path(base) / header;
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

    // Auto-detect required linker libraries from C headers
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
    // Also include user flags (add -l prefix to match linker convention)
    for (auto& lf : opts.userLinkerFlags)
        result->linkerFlags.push_back("-l" + lf);

    // ── Step 5: semantic check ──────────────────────────────────────────────
    progress(++step, 5, "running semantic checker");
    bool anyCheckError = false;
    size_t checkIdx = 0;
    for (auto& unit : result->units) {
        ++checkIdx;
        if (!opts.quiet)
            printUnitLine(stage, "check", checkIdx, result->units.size(), unit.filePath);
        Checker checker;
        checker.setNamespaceContext(result->registry.get(), unit.namespaceName, unit.filePath);
        checker.setCBindings(result->cBindings.get());
        bool passed = checker.check(unit.parseResult.tree);
        for (auto& err : checker.errors())
            printErrorLine(unit.filePath + ": " + err);
        if (!passed) anyCheckError = true;
    }
    if (anyCheckError) {
        result->hasErrors = true;
        return result;
    }

    return result;
}
