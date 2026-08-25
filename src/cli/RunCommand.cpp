#include "cli/RunCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "cli/BinaryBuilder.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"
#include "comptime/ComptimeEngine.h"
#include "generated/LucisParser.h"

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <llvm/Support/MD5.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>

namespace fs = std::filesystem;

void RunCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file (auto-resolved from lucis.yaml if omitted)", false);
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, or fast (default: 0)");
    parser.addOption("target", '\0', "TRIPLE", "Target triple (default: host)");
    parser.addOption("sysroot", '\0', "PATH", "System root for cross-compilation headers/libs");
    parser.addFlag("lto", '\0', "Enable Link Time Optimization");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addFlag("clean", 'c', "Clear cache before compiling");
    parser.addOption("link", 'l', "LIB", "Link against a library (repeatable)", true);
    parser.addOption("lib-path", 'L', "DIR", "Add library search path (repeatable)", true);
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    parser.addFlag("ignore-config", '\0', "Ignore lucis.yaml, use CLI flags only");
}

int RunCommand::run(const ArgParser& parser) {
    auto resolved = resolveInputFile(parser.get("file"), "",
                                     parser.has("ignore-config"));
    if (resolved.filePath.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis run <file>   or   lucis run  (from a project with lucis.yaml)\n";
        return 1;
    }

    bool useConfig = resolved.useConfig;
    auto& cfg = resolved.config;

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = resolved.filePath;
    pipeOpts.quiet     = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (useConfig) {
        if (pipeOpts.includePaths.empty())
            pipeOpts.includePaths = cfg->build.includePaths;
    }

    pipeOpts.sourcePaths = useConfig ? cfg->sourcePaths : std::vector<std::string>{"src/"};

    pipeOpts.targetTriple = parser.get("target");
    if (pipeOpts.targetTriple.empty() && useConfig)
        pipeOpts.targetTriple = cfg->build.target;
    pipeOpts.sysroot = parser.get("sysroot");
    if (pipeOpts.sysroot.empty() && useConfig)
        pipeOpts.sysroot = cfg->build.sysroot;

    pipeOpts.userLinkerFlags = parser.getAll("link");
    if (useConfig && pipeOpts.userLinkerFlags.empty())
        pipeOpts.userLinkerFlags = cfg->linker.libs;

    pipeOpts.binaryName = useConfig ? cfg->binary : "";
    pipeOpts.outDir     = useConfig ? cfg->outDir : "";

    OptimizationLevel optLevel = OptimizationLevel::O0;
    if (parser.has("opt"))
        optLevel = parseOptimizationLevel(parser.get("opt"));
    else if (useConfig)
        optLevel = parseOptimizationLevel(cfg->run.optLevel);

    bool useLTO   = parser.has("lto")   ? true : (useConfig ? cfg->run.lto : false);
    bool useClean = parser.has("clean");

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    // ── Build the binary through the shared builder ────────────────────────
    BinaryBuildOptions buildOpts;
    buildOpts.optLevel = optLevel;
    buildOpts.useLTO = useLTO;
    buildOpts.useClean = useClean;
    buildOpts.quiet = pipeOpts.quiet;
    buildOpts.emitDebugInfo = pipeOpts.emitDebugInfo;
    buildOpts.noStd = pipeOpts.noStd;
    buildOpts.targetTriple = pipeOpts.targetTriple;
    buildOpts.entryFilePath = resolved.filePath;
    buildOpts.cachePrefix = "run";
    buildOpts.progressStage = "run";

    std::string runBinPath;
    if (!BinaryBuilder::build(*pipeline, buildOpts, nullptr, runBinPath))
        return 1;

    // ── Execute ────────────────────────────────────────────────────────────
    if (!pipeOpts.quiet)
        std::cerr << "\nlucis: [run] --- program output ---\n\n";

    pid_t pid = ::fork();
    if (pid < 0) {
        std::cerr << "lucis: failed to execute run binary: " << std::strerror(errno) << "\n";
        return 1;
    }

    if (pid == 0) {
        std::vector<char*> execArgv;
        execArgv.push_back(const_cast<char*>(runBinPath.c_str()));
        // Config run.args first (lower priority)
        if (useConfig) {
            for (auto& arg : cfg->run.args)
                execArgv.push_back(const_cast<char*>(arg.c_str()));
        }
        // CLI remaining args override (higher priority)
        for (auto& arg : parser.remaining())
            execArgv.push_back(const_cast<char*>(arg.c_str()));
        execArgv.push_back(nullptr);
        ::execv(runBinPath.c_str(), execArgv.data());
        ::_exit(127);
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            std::cerr << "lucis: waitpid failed: " << std::strerror(errno) << "\n";
            return 1;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
