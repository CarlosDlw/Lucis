#include "cli/CLI.h"
#include "parser/Parser.h"
#include "checkers/Checker.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"
#include "namespace/ProjectScanner.h"
#include "namespace/NamespaceRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CHeaderResolver.h"
#include "helpc/HelpC.h"
#include "lsp/LspServer.h"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/MD5.h>
#include <llvm/Support/Error.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef LUX_RUNTIME_DIAGNOSTICS
#include <array>
#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace {

bool useAnsiStderr() {
    static const bool enabled = (::isatty(STDERR_FILENO) == 1);
    return enabled;
}

void printProgressLine(const char* stage,
                       int current,
                       int total,
                       const std::string& msg) {
    int pct = (total > 0) ? (current * 100 / total) : 100;
    std::cerr << "lux: ";
    if (useAnsiStderr()) std::cerr << "\033[1;36m";
    std::cerr << "[" << stage << " " << std::setw(3) << pct << "%]";
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << " " << msg << "\n";
}

void printUnitLine(const char* stage,
                   const char* phase,
                   size_t idx,
                   size_t total,
                   const std::string& path) {
    std::cerr << "lux: ";
    if (useAnsiStderr()) std::cerr << "\033[1;33m";
    std::cerr << "[" << stage << " " << phase << " " << idx << "/" << total << "]";
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << " " << path << "\n";
}

void printOutputDivider(const char* stage, const std::string& label) {
    std::cerr << "\n";
    std::cerr << "lux: ";
    if (useAnsiStderr()) std::cerr << "\033[90m";
    std::cerr << "[" << stage << "] --- " << label << " ---";
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << "\n\n";
}

void printErrorLine(const std::string& msg) {
    std::cerr << "lux: ";
    if (useAnsiStderr()) std::cerr << "\033[1;31m";
    std::cerr << msg;
    if (useAnsiStderr()) std::cerr << "\033[0m";
    std::cerr << "\n";
}

}

namespace fs = std::filesystem;

#ifdef LUX_RUNTIME_DIAGNOSTICS
namespace {

volatile sig_atomic_t g_inCrashHandler = 0;

const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
#ifdef SIGBUS
        case SIGBUS:  return "SIGBUS";
#endif
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        default:      return "SIGNAL";
    }
}

const char* probableCause(int sig) {
    switch (sig) {
        case SIGSEGV:
#ifdef SIGBUS
        case SIGBUS:
#endif
            return "invalid memory access (overflow/use-after-free/null dereference)";
        case SIGABRT:
            return "detected memory corruption or explicit abort()";
        case SIGFPE:
            return "invalid arithmetic operation (division by zero or overflow trap)";
        case SIGILL:
            return "invalid instruction or corrupted control flow";
        default:
            return "runtime fatal condition";
    }
}

void runtimeCrashHandler(int sig, siginfo_t* info, void*) {
    if (g_inCrashHandler) _Exit(128 + sig);
    g_inCrashHandler = 1;

    std::string msg;
    msg += "\nlux: runtime crash detected in JIT execution\n";
    msg += "lux: signal: ";
    msg += signalName(sig);
    msg += " (" + std::to_string(sig) + ")\n";

    if (info) {
        msg += "lux: fault address: ";
        msg += std::to_string(reinterpret_cast<uintptr_t>(info->si_addr));
        msg += "\n";
    }

    msg += "lux: probable cause: ";
    msg += probableCause(sig);
    msg += "\n";
    msg += "lux: stack trace:\n";
    (void)!write(STDERR_FILENO, msg.c_str(), msg.size());

    void* frames[64];
    int n = backtrace(frames, 64);
    if (n > 0)
        backtrace_symbols_fd(frames, n, STDERR_FILENO);

    const char* hint =
        "lux: hint: rebuild with -DCMAKE_BUILD_TYPE=Debug to maximize diagnostics\n";
    (void)!write(STDERR_FILENO, hint, std::strlen(hint));

    _Exit(128 + sig);
}

class RuntimeSignalGuard {
public:
    RuntimeSignalGuard() {
        install(SIGSEGV, idx_++);
        install(SIGABRT, idx_++);
#ifdef SIGBUS
        install(SIGBUS, idx_++);
#endif
        install(SIGFPE, idx_++);
        install(SIGILL, idx_++);
    }

    ~RuntimeSignalGuard() {
        for (int i = 0; i < idx_; i++)
            sigaction(installed_[i], &oldActions_[i], nullptr);
    }

private:
    void install(int sig, int slot) {
        installed_[slot] = sig;
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = runtimeCrashHandler;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigemptyset(&sa.sa_mask);
        sigaction(sig, &sa, &oldActions_[slot]);
    }

    std::array<struct sigaction, 8> oldActions_{};
    std::array<int, 8> installed_{};
    int idx_ = 0;
};

}
#endif

CLI::CLI(int argc, char* argv[])
    : argc_(argc), argv_(argv)
{
    // Check for subcommands early
    if (argc >= 2 && std::string(argv[1]) == "helpc") {
        isHelpC_ = true;
        return;
    }
    if (argc >= 2 && std::string(argv[1]) == "lsp") {
        isLSP_ = true;
        return;
    }
    if (argc >= 3 && std::string(argv[1]) == "run") {
        isRun_ = true;
        options_.runJIT   = true;
        options_.inputFile = argv[2];
        // Collect -l/-L/-I/-q flags; everything after -- goes to runArgs
        bool doubleDash = false;
        for (int i = 3; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--") { doubleDash = true; continue; }
            if (doubleDash) {
                options_.runArgs.push_back(arg);
            } else if (arg == "-q" || arg == "--quiet") {
                options_.quiet = true;
            } else if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
                options_.linkerFlags.push_back(arg);
            } else if (arg.rfind("-L", 0) == 0 && arg.size() > 2) {
                options_.libPaths.push_back(arg);
            } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
                options_.includePaths.push_back(arg);
            }
        }
        return;
    }
    if (!parse(argc, argv)) {
        options_.showHelp = true;
    }
}

bool CLI::parse(int argc, char* argv[]) {
    if (argc < 2) {
        return false;
    }

    std::string first = argv[1];

    if (first == "help" || first == "--help" || first == "-h") {
        options_.showHelp = true;
        return true;
    }

    if (first == "version" || first == "--version" || first == "-v") {
        options_.showVersion = true;
        return true;
    }

    options_.inputFile = first;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-o1") {
            options_.optimizationLevel = 1;
        } else if (arg == "-o2") {
            options_.optimizationLevel = 2;
        } else if (arg == "-o3") {
            options_.optimizationLevel = 3;
        } else if (arg == "-q" || arg == "--quiet") {
            options_.quiet = true;
        } else if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
            options_.linkerFlags.push_back(arg);
        } else if (arg.rfind("-L", 0) == 0 && arg.size() > 2) {
            options_.libPaths.push_back(arg);
        } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            options_.includePaths.push_back(arg);
        } else if (arg[0] != '-') {
            options_.outputFile = arg;
        } else {
            printErrorLine("unknown flag '" + arg + "'");
            std::cerr << "Run 'lux help' for usage.\n";
            return false;
        }
    }

    options_.showIR = options_.outputFile.empty();
    return true;
}

void CLI::printHelp() const {
    std::cout
        << "lux - Lux Compiler  v0.1.0\n\n"
        << "Usage:\n"
        << "  lux <file.lx>              Compile and print LLVM IR to stdout\n"
        << "  lux <file.lx> <output>     Compile and emit native binary\n"
        << "  lux <file.lx> <output> -oN Compile with optimization level N (1, 2, or 3)\n"
        << "  lux <file.lx> <output> -q  Compile with minimal output\n"
        << "  lux run <file.lx>          JIT-execute via LLVM IR (no binary emitted)\n"
        << "  lux run <file.lx> -q       JIT-execute with minimal output\n"
        << "  lux run <file.lx> -- ...   JIT-execute, forwarding args to main\n"
        << "  lux helpc <lib> [symbol]   C library reference helper\n"
        << "  lux lsp                    Start LSP server (for editor integration)\n"
        << "  lux help                   Show this help message\n"
        << "  lux version                Show compiler version\n\n"
        << "Linker flags:\n"
        << "  -lname       Link against library 'name' (e.g. -lSDL2)\n"
        << "  -Lpath       Add 'path' to the library search path\n"
        << "  -Ipath       Add 'path' to the include search path (reserved)\n"
        << "  -q, --quiet  Hide pipeline logs and show only final output\n\n"
        << "Examples:\n"
        << "  lux main.lx\n"
        << "  lux main.lx ./main\n"
        << "  lux main.lx ./main -o2\n"
        << "  lux run main.lx\n"
        << "  lux run main.lx -lraylib\n";
}

void CLI::printVersion() const {
    std::cout << "lux v0.1.0\n";
}

int CLI::run() {
    if (isLSP_) {
        LspServer server;
        return server.run();
    }
    if (isHelpC_) {
        HelpCCommand cmd;
        if (!HelpC::parseArgs(argc_, argv_, cmd)) {
            HelpC::printUsage();
            return 1;
        }
        return HelpC::run(cmd);
    }
    if (isRun_) {
        return jitRun();
    }
    if (options_.showHelp) {
        printHelp();
        return 0;
    }
    if (options_.showVersion) {
        printVersion();
        return 0;
    }
    return compile();
}

// ── Helpers ──────────────────────────────────────────────────────────────────

std::string CLI::getProjectRoot() const {
    auto parent = fs::path(options_.inputFile).parent_path();
    if (parent.empty()) parent = ".";
    return fs::canonical(parent).string();
}

std::string CLI::ensureBuildDir(const std::string& projectRoot) const {
    auto buildDir = projectRoot + "/.luxbuild";
    fs::create_directories(buildDir);
    return buildDir;
}

std::string CLI::extractNamespace(LuxParser::ProgramContext* tree) {
    if (auto* nsDecl = tree->namespaceDecl()) {
        return nsDecl->IDENTIFIER()->getText();
    }
    return {};
}

std::string CLI::makeObjectName(const SourceUnit& unit) {
    // Use namespace + sanitized filename stem for uniqueness.
    auto stem = fs::path(unit.filePath).stem().string();
    return unit.namespaceName + "__" + stem;
}

// ── Main compilation pipeline ────────────────────────────────────────────────

int CLI::compile() {
    auto progress = [&](int current, int total, const std::string& msg) {
        if (options_.quiet) return;
        printProgressLine("build", current, total, msg);
    };
    constexpr int totalSteps = 7;
    int step = 0;

    // ═════════════════════════════════════════════════════════════════════
    // STEP 1: DETERMINE PROJECT ROOT & BUILD DIRECTORY
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "resolving project root");
    auto projectRoot = getProjectRoot();
    auto buildDir    = ensureBuildDir(projectRoot);

    // ═════════════════════════════════════════════════════════════════════
    // STEP 2: SCAN ALL .lx FILES IN THE PROJECT
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "scanning .lx files");
    auto allFiles = ProjectScanner::scan(projectRoot);
    if (allFiles.empty()) {
        printErrorLine("no .lx files found in '" + projectRoot + "'");
        return 1;
    }

    // ═════════════════════════════════════════════════════════════════════
    // STEP 3: PARSE ALL FILES
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "parsing source files");
    std::vector<SourceUnit> units;
    bool anyParseError = false;

    const size_t parseTotal = allFiles.size();
    size_t parseIdx = 0;
    for (auto& filePath : allFiles) {
        ++parseIdx;
        if (!options_.quiet)
            printUnitLine("build", "parse", parseIdx, parseTotal, filePath);
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

        units.push_back(std::move(unit));
    }

    if (anyParseError) return 1;

    // ═════════════════════════════════════════════════════════════════════
    // STEP 4: BUILD NAMESPACE REGISTRY
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "building namespace registry");
    NamespaceRegistry registry;
    for (auto& unit : units) {
        registry.registerFile(unit.namespaceName, unit.filePath,
                              unit.parseResult.tree);
    }

    // Check for duplicate symbols within namespaces
    auto dupErrors = registry.validate();
    if (!dupErrors.empty()) {
        for (auto& err : dupErrors) {
            printErrorLine(err);
        }
        return 1;
    }

    // ═════════════════════════════════════════════════════════════════════
    // STEP 4.5: RESOLVE C HEADER INCLUDES
    // ═════════════════════════════════════════════════════════════════════
    progress(step, totalSteps, "resolving C includes and auto-link");
    CBindings cBindings;
    TypeRegistry cTypeReg;
    std::vector<std::string> cSourceFiles;  // C sources to compile
    {
        CHeaderResolver resolver(cTypeReg, cBindings, options_.includePaths);

        for (auto& unit : units) {
            auto* tree = unit.parseResult.tree;
            for (auto* pre : tree->preambleDecl()) {
                auto* incl = pre->includeDecl();
                if (!incl) continue;
                auto text = incl->getText();
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(text);
                    if (header.empty()) {
                        std::cerr << "lux: invalid system include in '"
                                  << unit.filePath << "'\n";
                        continue;
                    }
                    if (!resolver.resolveSystemHeader(header)) {
                        std::cerr << "lux: cannot find header '<" << header
                                  << ">'. Check '-I' include paths\n";
                        return 1;
                    }
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(text);
                    if (header.empty()) {
                        std::cerr << "lux: invalid local include in '"
                                  << unit.filePath << "'\n";
                        continue;
                    }
                    auto basePath = fs::path(unit.filePath).parent_path().string();
                    if (!resolver.resolveLocalHeader(header, basePath)) {
                        std::cerr << "lux: cannot find header '\"" << header
                                  << "\"'. Check '-I' include paths\n";
                        return 1;
                    }

                    // Check for a corresponding .c source file
                    auto headerPath = fs::path(basePath) / header;
                    auto cPath = fs::path(headerPath).replace_extension(".c");
                    if (fs::exists(cPath)) {
                        auto canonical = fs::canonical(cPath).string();
                        if (std::find(cSourceFiles.begin(), cSourceFiles.end(),
                                      canonical) == cSourceFiles.end()) {
                            cSourceFiles.push_back(canonical);
                        }
                    }
                }
            }
        }
    }

    // ═════════════════════════════════════════════════════════════════════
    // STEP 4.5b: AUTO-DETECT REQUIRED LINKER LIBRARIES
    // ═════════════════════════════════════════════════════════════════════
    for (auto& [flag, header] : cBindings.requiredLibs()) {
        // Check if the user already provided this flag
        bool alreadyProvided = false;
        for (auto& lf : options_.linkerFlags) {
            if (lf == flag) { alreadyProvided = true; break; }
        }
        if (!alreadyProvided) {
            if (!options_.quiet) {
                std::cerr << "lux: auto-linking '" << flag
                          << "' (required by <" << header << ">)\n";
            }
            options_.linkerFlags.push_back(flag);
        }
    }

    // ═════════════════════════════════════════════════════════════════════
    // STEP 4.6: COMPILE LOCAL C SOURCES
    // ═════════════════════════════════════════════════════════════════════
    std::vector<std::string> cObjectFiles;
    if (!cSourceFiles.empty()) {
        if (!options_.quiet)
            printOutputDivider("build", "compiler output");
    }
    for (auto& cSrc : cSourceFiles) {
        auto stem = fs::path(cSrc).stem().string();
        auto objPath = buildDir + "/c__" + stem + ".o";

        // Build -I flags for the C compiler
        std::vector<std::string> cIncFlags;
        cIncFlags.push_back("-I" + fs::path(cSrc).parent_path().string());
        for (auto& ip : options_.includePaths) {
            if (ip.rfind("-I", 0) == 0)
                cIncFlags.push_back(ip);
            else
                cIncFlags.push_back("-I" + ip);
        }

        if (!CodeGen::compileCSource(cSrc, objPath, cIncFlags, options_.quiet)) {
            std::cerr << "lux: failed to compile C source '"
                      << cSrc << "'\n";
            return 1;
        }
        cObjectFiles.push_back(objPath);
    }

    // ═════════════════════════════════════════════════════════════════════
    // STEP 5: SEMANTIC CHECK ALL FILES
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "running semantic checker");
    bool anyCheckError = false;
    const size_t checkTotal = units.size();
    size_t checkIdx = 0;
    for (auto& unit : units) {
        ++checkIdx;
        if (!options_.quiet)
            printUnitLine("build", "check", checkIdx, checkTotal, unit.filePath);
        Checker checker;
        checker.setNamespaceContext(&registry, unit.namespaceName, unit.filePath);
        checker.setCBindings(&cBindings);
        bool passed = checker.check(unit.parseResult.tree);
        // Always print diagnostics (errors and warnings)
        for (auto& err : checker.errors()) {
            printErrorLine(unit.filePath + ": " + err);
        }
        if (!passed)
            anyCheckError = true;
    }
    if (anyCheckError) return 1;

    // ═════════════════════════════════════════════════════════════════════
    // STEP 6: GENERATE IR, OPTIMIZE, AND EMIT OBJECT FILES
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "generating IR and object files");
    std::vector<std::string> objectFiles;
    bool anyIRError = false;

    const size_t irTotal = units.size();
    size_t irIdx = 0;
    for (auto& unit : units) {
        ++irIdx;
        if (!options_.quiet)
            printUnitLine("build", "ir", irIdx, irTotal, unit.filePath);
        IRGen irGen;
        irGen.setNamespaceContext(&registry, unit.namespaceName, unit.filePath);
        irGen.setCBindings(&cBindings);
        auto irModule = irGen.generate(unit.parseResult.tree, unit.filePath);
        if (!irModule) {
            printErrorLine("IR generation failed for '" + unit.filePath + "'");
            anyIRError = true;
            continue;
        }

        // Show IR mode: only print the main file's IR
        if (options_.showIR) {
            auto mainPath = fs::canonical(fs::path(options_.inputFile)).string();
            if (unit.filePath == mainPath) {
                irModule->print();
            }
            continue;
        }

        // Optimize
        if (options_.optimizationLevel > 0) {
            auto level = static_cast<OptimizationLevel>(options_.optimizationLevel);
            Optimizer::optimize(*irModule, level);
        }

        // Emit object file to .luxbuild/
        auto objName = makeObjectName(unit);
        auto objPath = buildDir + "/" + objName + ".o";
        if (!CodeGen::emitObjectFile(irModule->module(), objPath)) {
            printErrorLine("failed to emit object for '" + unit.filePath + "'");
            anyIRError = true;
            continue;
        }
        objectFiles.push_back(objPath);
    }

    if (anyIRError) return 1;
    if (options_.showIR) return 0;

    // Append compiled C object files
    objectFiles.insert(objectFiles.end(),
                       cObjectFiles.begin(), cObjectFiles.end());

    // ═════════════════════════════════════════════════════════════════════
    // STEP 7: LINK ALL OBJECT FILES
    // ═════════════════════════════════════════════════════════════════════
    progress(++step, totalSteps, "linking final binary");
    if (!options_.quiet)
        printOutputDivider("build", "linker output");
    if (!CodeGen::linkObjectFiles(objectFiles, options_.outputFile,
                                   options_.linkerFlags, options_.libPaths,
                                   true, options_.quiet)) {
        printErrorLine("failed to link binary '" + options_.outputFile + "'");
        return 1;
    }

    std::cout << "lux: binary written to '" << options_.outputFile << "'\n";
    progress(totalSteps, totalSteps, "build completed");
    return 0;
}

// ── JIT Runner ───────────────────────────────────────────────────────────────
//
// Runs the full compile pipeline (parse → check → IRGen for all project files),
// links all generated modules into one via bitcode serialization (so they can
// cross-context merge), then executes main() in-process via LLVM MCJIT.
//
// External symbols (libc, etc.) are resolved from the host process image.
// Shared libraries specified with -l flags are dlopen'd before JIT finalization.

int CLI::jitRun() {
#ifdef LUX_RUNTIME_DIAGNOSTICS
    RuntimeSignalGuard signalGuard;
#endif

    auto progress = [&](int current, int total, const std::string& msg) {
        if (options_.quiet) return;
        printProgressLine("run", current, total, msg);
    };
    constexpr int totalSteps = 8;
    int step = 0;

    // ─── Steps 1-5: same as compile() ────────────────────────────────────
    progress(++step, totalSteps, "resolving project root");
    auto projectRoot = getProjectRoot();

    progress(++step, totalSteps, "scanning .lx files");
    auto allFiles = ProjectScanner::scan(projectRoot);
    if (allFiles.empty()) {
        printErrorLine("no .lx files found in '" + projectRoot + "'");
        return 1;
    }

    progress(++step, totalSteps, "parsing source files");
    std::vector<SourceUnit> units;
    bool anyParseError = false;
    const size_t parseTotal = allFiles.size();
    size_t parseIdx = 0;
    for (auto& filePath : allFiles) {
        ++parseIdx;
        if (!options_.quiet)
            printUnitLine("run", "parse", parseIdx, parseTotal, filePath);
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
        units.push_back(std::move(unit));
    }
    if (anyParseError) return 1;

    progress(++step, totalSteps, "building namespace registry");
    NamespaceRegistry registry;
    for (auto& unit : units)
        registry.registerFile(unit.namespaceName, unit.filePath,
                              unit.parseResult.tree);

    auto dupErrors = registry.validate();
    if (!dupErrors.empty()) {
        for (auto& err : dupErrors) printErrorLine(err);
        return 1;
    }

    progress(step, totalSteps, "resolving C includes and auto-link");
    CBindings cBindings;
    TypeRegistry cTypeReg;
    {
        CHeaderResolver resolver(cTypeReg, cBindings, options_.includePaths);
        for (auto& unit : units) {
            for (auto* pre : unit.parseResult.tree->preambleDecl()) {
                auto* incl = pre->includeDecl();
                if (!incl) continue;
                auto text = incl->getText();
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(text);
                    if (!header.empty())
                        resolver.resolveSystemHeader(header);
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(text);
                    if (!header.empty()) {
                        auto base = fs::path(unit.filePath).parent_path().string();
                        resolver.resolveLocalHeader(header, base);
                    }
                }
            }
        }
    }

    // Auto-link libraries required by C headers
    for (auto& [flag, header] : cBindings.requiredLibs()) {
        bool already = false;
        for (auto& lf : options_.linkerFlags) if (lf == flag) { already = true; break; }
        if (!already) {
            if (!options_.quiet) {
                std::cerr << "lux: auto-linking '" << flag
                          << "' (required by <" << header << ">)\n";
            }
            options_.linkerFlags.push_back(flag);
        }
    }

    progress(++step, totalSteps, "running semantic checker");
    bool anyCheckError = false;
    const size_t checkTotal = units.size();
    size_t checkIdx = 0;
    for (auto& unit : units) {
        ++checkIdx;
        if (!options_.quiet)
            printUnitLine("run", "check", checkIdx, checkTotal, unit.filePath);
        Checker checker;
        checker.setNamespaceContext(&registry, unit.namespaceName, unit.filePath);
        checker.setCBindings(&cBindings);
        bool passed = checker.check(unit.parseResult.tree);
        for (auto& err : checker.errors())
            printErrorLine(unit.filePath + ": " + err);
        if (!passed) anyCheckError = true;
    }
    if (anyCheckError) return 1;

    progress(++step, totalSteps, "generating and linking project IR");
    // ─── Step 6: Generate IR for each unit, link into one module ─────────

    // We use the first unit's context as the master context.
    // Subsequent modules are serialized to bitcode and re-parsed into it,
    // allowing llvm::Linker to merge modules from different IRGen contexts.

    std::unique_ptr<llvm::LLVMContext> masterCtx;
    std::unique_ptr<llvm::Module>      masterMod;

    const size_t irTotal = units.size();
    size_t irIdx = 0;
    for (auto& unit : units) {
        ++irIdx;
        if (!options_.quiet)
            printUnitLine("run", "ir", irIdx, irTotal, unit.filePath);
        IRGen irGen;
        irGen.setNamespaceContext(&registry, unit.namespaceName, unit.filePath);
        irGen.setCBindings(&cBindings);
        auto irMod = irGen.generate(unit.parseResult.tree, unit.filePath);
        if (!irMod) {
            printErrorLine("IR generation failed for '" + unit.filePath + "'");
            return 1;
        }

        if (!masterMod) {
            // First module: take ownership directly
            masterCtx = std::make_unique<llvm::LLVMContext>();

            // Serialize the generated module to bitcode, then re-parse into
            // masterCtx so every module lives in the same context.
            llvm::SmallVector<char, 0> buf;
            {
                llvm::raw_svector_ostream os(buf);
                llvm::WriteBitcodeToFile(*irMod->module(), os);
            }
            auto memBuf = llvm::MemoryBuffer::getMemBufferCopy(
                llvm::StringRef(buf.data(), buf.size()), unit.filePath);
            auto parsed = llvm::parseBitcodeFile(memBuf->getMemBufferRef(),
                                                  *masterCtx);
            if (!parsed) {
                printErrorLine("bitcode re-parse failed for '" + unit.filePath + "'");
                printErrorLine(llvm::toString(parsed.takeError()));
                return 1;
            }
            masterMod = std::move(parsed.get());
        } else {
            // Subsequent modules: serialize and merge into masterMod
            llvm::SmallVector<char, 0> buf;
            {
                llvm::raw_svector_ostream os(buf);
                llvm::WriteBitcodeToFile(*irMod->module(), os);
            }
            auto memBuf = llvm::MemoryBuffer::getMemBufferCopy(
                llvm::StringRef(buf.data(), buf.size()), unit.filePath);
            auto parsed = llvm::parseBitcodeFile(memBuf->getMemBufferRef(),
                                                  *masterCtx);
            if (!parsed) {
                printErrorLine("bitcode re-parse failed for '" + unit.filePath + "'");
                printErrorLine(llvm::toString(parsed.takeError()));
                return 1;
            }
            if (llvm::Linker::linkModules(*masterMod,
                                           std::move(parsed.get()))) {
                printErrorLine("failed to link module '" + unit.filePath + "'");
                return 1;
            }
        }
    }

    if (!masterMod) {
        printErrorLine("no IR generated");
        return 1;
    }
    masterMod->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
    progress(++step, totalSteps, "preparing cache and compilation");

    // ─── Step 7: Native-run path (emit object, link temp binary, execute) ─
    // MCJIT can become very slow on larger aggregate payload layouts.
    // Running a temporary native binary keeps `lux run` responsive and stable.
    const std::string tempDir = fs::temp_directory_path().string();
    const std::string cacheDir = tempDir + "/lux-run-cache";
    std::error_code cacheEc;
    fs::create_directories(cacheDir, cacheEc);

    std::string irText;
    {
        llvm::raw_string_ostream os(irText);
        masterMod->print(os, nullptr);
    }

    std::string cacheKeyData = irText;
    cacheKeyData += "\n#builtins=" + CodeGen::builtinsLibraryPath();
    for (auto& lp : options_.libPaths) cacheKeyData += "\n#L=" + lp;
    for (auto& lf : options_.linkerFlags) cacheKeyData += "\n#l=" + lf;

    llvm::MD5 md5;
    md5.update(cacheKeyData);
    llvm::MD5::MD5Result md5Result;
    md5.final(md5Result);
    llvm::SmallString<32> digest;
    llvm::MD5::stringifyResult(md5Result, digest);
    const std::string runBinPath = cacheDir + "/run-" + std::string(digest.str()) + ".bin";

    bool needBuild = true;
    if (fs::exists(runBinPath)) {
        std::error_code permsEc;
        auto perms = fs::status(runBinPath, permsEc).permissions();
        if (!permsEc && (perms & fs::perms::owner_exec) != fs::perms::none) {
            needBuild = false;
        }
    }

    auto linkWithCompiler = [&](const char* cc,
                                const std::string& irPath,
                                const std::string& outBinPath) {
        pid_t lpid = ::fork();
        if (lpid < 0) return false;
        if (lpid == 0) {
            if (options_.quiet) {
                int devNull = ::open("/dev/null", O_WRONLY);
                if (devNull >= 0) {
                    ::dup2(devNull, STDOUT_FILENO);
                    ::dup2(devNull, STDERR_FILENO);
                    if (devNull > STDERR_FILENO) ::close(devNull);
                }
            }
            std::vector<const char*> argv;
            argv.push_back(cc);
            argv.push_back(irPath.c_str());
            auto builtinsPath = CodeGen::builtinsLibraryPath();
            argv.push_back(builtinsPath.c_str());
#ifdef LUX_RUNTIME_DIAGNOSTICS
            argv.push_back("-fsanitize=address,undefined");
            argv.push_back("-fno-omit-frame-pointer");
#endif
            for (auto& lp : options_.libPaths)
                argv.push_back(lp.c_str());
            argv.push_back("-lm");
            argv.push_back("-lz");
            argv.push_back("-lpthread");
            for (auto& lf : options_.linkerFlags)
                argv.push_back(lf.c_str());
            argv.push_back("-o");
            argv.push_back(outBinPath.c_str());
            argv.push_back(nullptr);
            ::execvp(cc, const_cast<char**>(argv.data()));
            ::_exit(127);
        }
        int lstatus = 0;
        while (::waitpid(lpid, &lstatus, 0) < 0) {
            if (errno != EINTR) return false;
        }
        return WIFEXITED(lstatus) && WEXITSTATUS(lstatus) == 0;
    };

    if (needBuild) {
        progress(step, totalSteps, "cache miss: building run artifact");
        if (!options_.quiet)
            printOutputDivider("run", "compiler/linker output");
        std::string runLLTemplate = tempDir + "/lux-run-ir-XXXXXX.ll";
        std::vector<char> llTmpl(runLLTemplate.begin(), runLLTemplate.end());
        llTmpl.push_back('\0');
        int llfd = ::mkstemps(llTmpl.data(), 3);
        if (llfd < 0) {
            printErrorLine(std::string("could not create temporary IR path: ") +
                           std::strerror(errno));
            return 1;
        }
        ::close(llfd);
        const std::string runLLPath(llTmpl.data());
        {
            std::ofstream out(runLLPath, std::ios::binary | std::ios::trunc);
            out << irText;
        }

        const std::string runBinTempPath = runBinPath + ".tmp-" + std::to_string(::getpid());
        fs::remove(runBinTempPath);
        if (!linkWithCompiler("clang", runLLPath, runBinTempPath)
            && !linkWithCompiler("gcc", runLLPath, runBinTempPath)) {
            printErrorLine("linking failed — ensure clang or gcc is installed");
            fs::remove(runLLPath);
            fs::remove(runBinTempPath);
            return 1;
        }
        fs::remove(runLLPath);

        std::error_code renameEc;
        fs::rename(runBinTempPath, runBinPath, renameEc);
        if (renameEc) {
            // Another process may have produced the same cache artifact first.
            fs::remove(runBinTempPath);
            if (!fs::exists(runBinPath)) {
                printErrorLine("failed to finalize cached run binary: " + renameEc.message());
                return 1;
            }
        }
    } else {
        progress(step, totalSteps, "cache hit: reusing run binary");
    }

    progress(++step, totalSteps, "executing program");
    if (!options_.quiet)
        printOutputDivider("run", "program output");
    pid_t pid = ::fork();
    if (pid < 0) {
        printErrorLine(std::string("failed to execute run binary: ") + std::strerror(errno));
        fs::remove(runBinPath);
        return 1;
    }

    if (pid == 0) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(runBinPath.c_str()));
        for (auto& arg : options_.runArgs)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        ::execv(runBinPath.c_str(), argv.data());
        ::_exit(127);
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            printErrorLine(std::string("waitpid failed while running program: ") +
                           std::strerror(errno));
            fs::remove(runBinPath);
            return 1;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
