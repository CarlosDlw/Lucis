#include "cli/FuzzCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "cli/BinaryBuilder.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "attributes/FuzzAttribute.h"

#include <iostream>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

void FuzzCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("action", "init | run | list | repro", false);

    parser.addSection("Target & run");
    parser.addOption("corpus", '\0', "DIR", "Corpus directory (default: fuzz/corpus)");
    parser.addOption("crash-dir", '\0', "DIR", "Crash output directory (default: fuzz/crashes)");
    parser.addOption("iters", '\0', "N", "Max executions (0 = until interrupted)");
    parser.addOption("timeout", '\0', "MS", "Per-execution timeout in ms (default: 1000)");
    parser.addOption("seed", '\0', "N", "RNG seed (deterministic runs)");
    parser.addOption("jobs", 'j', "N", "Parallel fuzz workers (default 1)");
    parser.addFlag("verbose", 'v', "Verbose runner output");

    parser.addSection("Compilation");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, fast (default: 0)");
    parser.addOption("target", '\0', "TRIPLE", "Target triple (default: host)");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    parser.addFlag("ignore-config", '\0', "Ignore lucis.yaml, use CLI flags only");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
}

namespace {

const char* kExampleTarget = R"lucis(
// Example fuzz target: inputs whose first byte is 'C' (0x43) hit a seeded
// bug (null dereference) so you can watch the fuzzer find and shrink it.
use std::log::println;

#[fuzz]
fn parseBytes([]uint8 data) void {
    if data.len() < 1 {
        ret;
    }
    if data[0] == 67 {
        println("magic byte detected");
        *int32 p = null;   // seeded bug: crash
        *p = 1;
    }
}
)lucis";

const char* kReadme = R"(# fuzz/

Fuzz targets for this project, run with:

    lucis fuzz run <target>
    lucis fuzz list
    lucis fuzz repro <fuzz/crashes/crash_0_0/input.bin>

A target is a plain function marked #[fuzz] taking one []u8 parameter.
Corpus files live in corpus/ (new interesting inputs are added there);
crashes are saved under crashes/ with a minimized reproducer.
)";

// Empty corpus would not be tracked by git; provide one seed byte.
const char* kSeed = "\x00";

std::string defaultProjectRoot() {
    static const auto cwd = fs::current_path();
    return cwd.string();
}

std::string resolveTargetFile(const std::string& target, const std::string& projectRoot) {
    if (target.empty()) return {};
    if (target.size() > 3 && target.compare(target.size() - 3, 3, ".lc") == 0) {
        if (fs::exists(target)) return fs::canonical(target).string();
        return {};
    }
    // bare name -> <project>/fuzz/<name>.lc
    auto candidate = fs::path(projectRoot) / "fuzz" / (target + ".lc");
    if (fs::exists(candidate)) return fs::canonical(candidate).string();
    return {};
}

void printUsage() {
    std::cerr << "usage: lucis fuzz <action> [target] [flags]\n"
              << "       lucis fuzz init                          create fuzz/ with an example target\n"
              << "       lucis fuzz run <target> [flags]          fuzz a #[fuzz] target\n"
              << "       lucis fuzz list                          list targets in fuzz/\n"
              << "       lucis fuzz repro <file> [target]         reproduce a saved crash\n";
}

}  // namespace

int FuzzCommand::run(const ArgParser& parser) {
    std::string action = parser.get("action");
    auto extra = parser.remaining();
    const std::string projectRoot = defaultProjectRoot();

    if (action == "init") {
        fs::path fuzzDir = fs::path(projectRoot) / "fuzz";
        if (fs::exists(fuzzDir)) {
            std::cerr << "lucis: 'fuzz/' already exists in " << projectRoot << "\n";
            return 1;
        }
        fs::create_directories(fuzzDir / "corpus");
        {
            std::ofstream f(fuzzDir / "example.lc");
            f << kExampleTarget;
        }
        {
            std::ofstream f(fuzzDir / "corpus" / "seed.bin", std::ios::binary);
            f.write(kSeed, 1);
        }
        {
            std::ofstream f(fuzzDir / "README.md");
            f << kReadme;
        }
        std::cout << "created " << (fuzzDir / "example.lc").string() << "\n"
                  << "run it with: lucis fuzz run example\n";
        return 0;
    }

    if (action == "list") {
        fs::path fuzzDir = fs::path(projectRoot) / "fuzz";
        std::error_code ec;
        if (!fs::exists(fuzzDir, ec)) {
            std::cout << "no fuzz/ directory in " << projectRoot
                      << " (run `lucis fuzz init`)\n";
            return 0;
        }
        for (auto& entry : fs::directory_iterator(fuzzDir, ec)) {
            if (entry.path().extension() == ".lc")
                std::cout << entry.path().stem().string() << "\n";
        }
        return 0;
    }

    std::string reproFile;
    std::string targetArg;
    if (action == "repro") {
        if (extra.empty()) {
            printUsage();
            return 2;
        }
        reproFile = extra[0];
        if (extra.size() > 1) targetArg = extra[1];
    } else if (action == "run") {
        if (extra.empty()) {
            printUsage();
            return 2;
        }
        targetArg = extra[0];
    } else {
        std::cerr << "lucis: unknown fuzz action '" << action
                  << "' (expected init | run | list | repro)\n";
        printUsage();
        return 2;
    }

    // ── Resolve the target file and run the pipeline ─────────────────────
    std::string targetFile = resolveTargetFile(targetArg, projectRoot);
    if (targetFile.empty()) {
        std::cerr << "lucis: fuzz target not found: '" << targetArg << "'\n";
        std::cerr << "hint: it must exist as 'fuzz/<name>.lc' or be a path to a .lc file\n";
        return 2;
    }

    // The fuzz/ layout belongs to the target's project root (lucis.yaml or
    // nearest VCS root), not to the current working directory.
    const std::string root = LucisPipeline::getProjectRoot(targetFile);

    ResolvedInput resolved;
    resolved.filePath = targetFile;
    // Honor lucis.yaml when the target lives inside a project.
    std::optional<LucisConfig> cfg;
    if (fs::exists(fs::path(root) / "lucis.yaml")) {
        auto parsed = LucisConfig::load(fs::path(root) / "lucis.yaml");
        if (parsed) {
            resolved.config = *parsed;
            resolved.useConfig = true;
            cfg = *parsed;
        }
    }

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = targetFile;
    pipeOpts.quiet = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (resolved.useConfig) {
        if (pipeOpts.includePaths.empty())
            pipeOpts.includePaths = cfg->build.includePaths;
        pipeOpts.sourcePaths = cfg->sourcePaths;
    } else {
        pipeOpts.sourcePaths = {"src/"};
    }
    pipeOpts.targetTriple = parser.get("target");
    if (pipeOpts.targetTriple.empty() && resolved.useConfig)
        pipeOpts.targetTriple = cfg->build.target;
    pipeOpts.userLinkerFlags = resolved.useConfig ? cfg->linker.libs
                                                  : std::vector<std::string>{};
    pipeOpts.binaryName = resolved.useConfig ? cfg->binary : "";
    pipeOpts.outDir = resolved.useConfig ? cfg->outDir : "";

    OptimizationLevel optLevel = OptimizationLevel::O0;
    if (parser.has("opt"))
        optLevel = parseOptimizationLevel(parser.get("opt"));
    else if (resolved.useConfig)
        optLevel = parseOptimizationLevel(cfg->build.optLevel);

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    if (pipeline->fuzzTargets.empty()) {
        std::cerr << "lucis: no #[fuzz] target found in '" << targetFile << "'\n";
        return 1;
    }
    if (pipeline->fuzzTargets.size() > 1) {
        std::cerr << "lucis: only one #[fuzz] target is allowed per program (found "
                  << pipeline->fuzzTargets.size() << ")\n";
        return 1;
    }

    // ── Build the fuzz binary ────────────────────────────────────────────
    BinaryBuildOptions buildOpts;
    buildOpts.optLevel = optLevel;
    buildOpts.quiet = pipeOpts.quiet;
    buildOpts.emitDebugInfo = pipeOpts.emitDebugInfo;
    buildOpts.noStd = pipeOpts.noStd;
    buildOpts.targetTriple = pipeOpts.targetTriple;
    buildOpts.entryFilePath = targetFile;
    buildOpts.cachePrefix = "fuzz";
    buildOpts.progressStage = "fuzz";

    auto customizeIrGen = [&](IRGen& irGen, const SourceUnit&, bool isEntry) {
        irGen.setFuzzMode(true);
        if (isEntry)
            irGen.setFuzzTarget(pipeline->fuzzTargets[0]);
    };

    std::string binPath;
    if (!BinaryBuilder::build(*pipeline, buildOpts, customizeIrGen, binPath))
        return 1;

    // ── Runner argv (shared by all workers) ──────────────────────────────
    std::string corpusDir = parser.get("corpus");
    if (corpusDir.empty()) corpusDir = (fs::path(root) / "fuzz" / "corpus").string();
    std::string crashDir = parser.get("crash-dir");
    if (crashDir.empty()) crashDir = (fs::path(root) / "fuzz" / "crashes").string();

    std::vector<std::string> baseArgs;
    baseArgs.push_back("--corpus"); baseArgs.push_back(corpusDir);
    baseArgs.push_back("--crash-dir"); baseArgs.push_back(crashDir);
    if (parser.has("iters")) { baseArgs.push_back("--iters"); baseArgs.push_back(parser.get("iters")); }
    if (parser.has("timeout")) { baseArgs.push_back("--timeout"); baseArgs.push_back(parser.get("timeout")); }
    unsigned long long seedBase = 0;
    if (parser.has("seed")) seedBase = std::stoull(parser.get("seed"));
    if (parser.has("verbose")) baseArgs.push_back("--verbose");

    int jobs = 1;
    if (parser.has("jobs")) jobs = parser.getInt("jobs", 1);
    if (jobs < 1) jobs = 1;

    if (!reproFile.empty()) {
        baseArgs.push_back("--repro"); baseArgs.push_back(reproFile);
        jobs = 1;
    }

    // ── Spawn workers (or a single repro run) ────────────────────────────
    std::vector<pid_t> workers;
    for (int w = 0; w < jobs; ++w) {
        pid_t pid = ::fork();
        if (pid < 0) {
            std::cerr << "lucis: fork failed: " << std::strerror(errno) << "\n";
            return 1;
        }
        if (pid == 0) {
            std::vector<const char*> argv;
            argv.push_back(binPath.c_str());
            if (jobs > 1) {
                argv.push_back("--worker-id");
                std::string wid = std::to_string(w);
                argv.push_back(wid.c_str());
                std::string seed = std::to_string(seedBase + (unsigned long long)w * 100003ull);
                argv.push_back("--seed"); argv.push_back(seed.c_str());
            } else if (parser.has("seed")) {
                argv.push_back("--seed"); argv.push_back(parser.get("seed").c_str());
            }
            for (auto& a : baseArgs)
                argv.push_back(a.c_str());
            argv.push_back(nullptr);
            ::execv(binPath.c_str(), const_cast<char**>(argv.data()));
            ::_exit(127);
        }
        workers.push_back(pid);
    }

    // ── Wait for workers; report exit code ───────────────────────────────
    int worst = 0;
    for (auto wp : workers) {
        int status = 0;
        pid_t r;
        do {
            r = ::waitpid(wp, &status, 0);
        } while (r < 0 && errno == EINTR);
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code != 0) worst = code;
        } else if (WIFSIGNALED(status)) {
            worst = 128 + WTERMSIG(status);
        }
    }
    return worst;
}