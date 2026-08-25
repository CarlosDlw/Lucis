#include "cli/TestCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "cli/BinaryBuilder.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>

void TestCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint (auto-resolved from lucis.yaml if omitted)", false);

    parser.addSection("Selection");
    parser.addOption("test", 't', "SUBSTR", "Run only tests matching substring (repeatable)", true);
    parser.addOption("skip", '\0', "SUBSTR", "Skip tests matching substring (repeatable)", true);
    parser.addFlag("exact", '\0', "Filters match the exact name instead of substring");
    parser.addFlag("list", '\0', "List discovered tests without building");

    parser.addSection("Execution");
    parser.addOption("timeout", '\0', "MS", "Global default timeout in milliseconds per test (0 = none)");
    parser.addOption("jobs", 'j', "N", "Parallel test children (default 1)");
    parser.addFlag("verbose", 'v', "Echo captured output of every test");

    parser.addSection("Compilation");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, fast (default: 0)");
    parser.addOption("target", '\0', "TRIPLE", "Target triple (default: host)");
    parser.addOption("sysroot", '\0', "PATH", "System root for cross-compilation headers/libs");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    parser.addFlag("ignore-config", '\0', "Ignore lucis.yaml, use CLI flags only");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
}

namespace {

// Selection predicate: substring or exact match against the display name and
// the qualified "module::function" name.
bool testMatches(const TestCase& tc, const std::vector<std::string>& patterns,
                 bool exact) {
    if (patterns.empty()) return true;
    const std::string& display = tc.spec.displayName.empty()
        ? tc.funcName : tc.spec.displayName;
    const std::string qualified = tc.modulePath + "::" + tc.funcName;
    for (auto& pat : patterns) {
        bool hit = exact
            ? (display == pat || qualified == pat)
            : (display.find(pat) != std::string::npos ||
               qualified.find(pat) != std::string::npos);
        if (hit) return true;
    }
    return false;
}

void printList(const std::vector<TestCase>& tests) {
    for (auto& tc : tests) {
        std::cout << tc.modulePath << "::" << tc.funcName;
        if (!tc.spec.displayName.empty())
            std::cout << "  \"" << tc.spec.displayName << "\"";
        if (tc.spec.skip)
            std::cout << "  [skip"
                      << (tc.spec.skipReason.empty() ? "" : ": " + tc.spec.skipReason)
                      << "]";
        if (tc.spec.expectedFail)
            std::cout << "  [expected_fail]";
        if (tc.spec.timeoutMs > 0)
            std::cout << "  [timeout " << tc.spec.timeoutMs << "ms]";
        std::cout << "\n";
    }
}

}  // namespace

int TestCommand::run(const ArgParser& parser) {
    auto resolved = resolveInputFile(parser.get("file"), parser.get("config"),
                                     parser.has("ignore-config"));
    if (resolved.filePath.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis test <file>   or   lucis test  (from a project with lucis.yaml)\n";
        return 2;
    }

    bool useConfig = resolved.useConfig;
    auto& cfg = resolved.config;

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = resolved.filePath;
    pipeOpts.quiet     = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (useConfig && pipeOpts.includePaths.empty())
        pipeOpts.includePaths = cfg->build.includePaths;
    pipeOpts.sourcePaths = useConfig ? cfg->sourcePaths
                                     : std::vector<std::string>{"src/"};
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
        optLevel = parseOptimizationLevel(cfg->build.optLevel);

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    // ── Discovery & filtering (before any IR generation) ─────────────────
    bool exact = parser.has("exact");
    auto includePatterns = parser.getAll("test");
    auto skipPatterns    = parser.getAll("skip");

    std::vector<TestCase> tests;
    for (auto& tc : pipeline->testCases) {
        if (!testMatches(tc, includePatterns, exact)) continue;
        if (!skipPatterns.empty() && testMatches(tc, skipPatterns, exact)) continue;
        tests.push_back(tc);
    }

    if (parser.has("list")) {
        printList(tests);
        return 0;
    }

    if (tests.empty()) {
        if (includePatterns.empty())
            std::cerr << "lucis: no tests found\n";
        else
            std::cerr << "lucis: no tests matched the given filters\n";
        return 0;
    }

    // ── Build the test binary (harness main is synthesized on the entry) ─
    int jobs = parser.getInt("jobs", 1);
    if (jobs < 1) jobs = 1;
    int64_t timeoutMs = parser.getInt("timeout", 0);
    if (timeoutMs < 0) timeoutMs = 0;

    BinaryBuildOptions buildOpts;
    buildOpts.optLevel = optLevel;
    buildOpts.quiet = pipeOpts.quiet;
    buildOpts.emitDebugInfo = pipeOpts.emitDebugInfo;
    buildOpts.noStd = pipeOpts.noStd;
    buildOpts.targetTriple = pipeOpts.targetTriple;
    buildOpts.entryFilePath = resolved.filePath;
    buildOpts.cachePrefix = "test";
    buildOpts.progressStage = "test";

    auto customizeIrGen = [&](IRGen& irGen, const SourceUnit&, bool isEntry) {
        irGen.setTestMode(true);
        if (isEntry) {
            irGen.setTestCases(tests);
            irGen.setTestRunnerOptions(jobs, timeoutMs, parser.has("verbose"));
        }
    };

    std::string binPath;
    if (!BinaryBuilder::build(*pipeline, buildOpts, customizeIrGen, binPath))
        return 1;

    // ── Execute the test binary ──────────────────────────────────────────
    if (!pipeOpts.quiet)
        std::cerr << "\nlucis: [test] --- test output ---\n\n";

    pid_t pid = ::fork();
    if (pid < 0) {
        std::cerr << "lucis: failed to execute test binary: "
                  << std::strerror(errno) << "\n";
        return 1;
    }
    if (pid == 0) {
        std::vector<char*> execArgv;
        execArgv.push_back(const_cast<char*>(binPath.c_str()));
        execArgv.push_back(nullptr);
        ::execv(binPath.c_str(), execArgv.data());
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