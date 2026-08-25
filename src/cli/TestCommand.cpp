#include "cli/TestCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "cli/BinaryBuilder.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "attributes/TargetInfo.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

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

// ── Project-wide test discovery ────────────────────────────────────────
// Conventional test directories (tests/, test/, specs/, spec/, unit/) —
// any .lc inside them is a candidate; elsewhere only *.test.lc / *_test.lc
// files are. A candidate must actually contain a #[test] attribute.

bool isExcludedScanDir(const std::string& name) {
    if (!name.empty() && name[0] == '.') return true;
    static const char* kExcluded[] = {
        "node_modules", "target", "build", "dist", "out", "bin", "obj",
    };
    for (auto e : kExcluded)
        if (name == e) return true;
    return false;
}

bool isConventionalTestDir(const std::string& name) {
    return name == "tests" || name == "test" ||
           name == "specs" || name == "spec" || name == "unit";
}

bool isConventionalTestFile(const std::string& name) {
    const std::string dotTail = name.size() >= 9 ? name.substr(name.size() - 9) : "";
    const std::string underTail = name.size() >= 8 ? name.substr(name.size() - 8) : "";
    return dotTail == ".test.lc" || underTail == "_test.lc";
}

bool fileHasTestAttribute(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("#[test") != std::string::npos)
            return true;
    }
    return false;
}

void walkTestCandidates(const fs::path& dir, std::vector<std::string>& out) {
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        auto p = entry.path();
        auto name = p.filename().string();
        if (entry.is_directory()) {
            if (isExcludedScanDir(name)) continue;
            walkTestCandidates(p, out);
        } else if (entry.is_regular_file()) {
            bool byDir  = isConventionalTestDir(p.parent_path().filename().string());
            bool byName = isConventionalTestFile(name);
            if ((byDir || byName) && fileHasTestAttribute(p.string()))
                out.push_back(fs::canonical(p).string());
        }
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

    // ── Project-wide discovery (`lucis test` without an explicit file) ───
    // Tests outside the entry's import graph are found through conventional
    // test directories and *.test.lc / *_test.lc file names.
    //
    // Every independent root (the entry and each discovered candidate) is a
    // separate test GROUP with its own binary: merging unrelated files into
    // one binary would collide the plain (non-mangled) user symbols.
    struct TestGroup {
        std::unique_ptr<PipelineResult> pipeline;
        std::string inputPath;
        std::vector<TestCase> tests;   // filtered + deduplicated
    };
    std::vector<TestGroup> groups;
    groups.push_back({ std::move(pipeline), resolved.filePath, {} });

    bool explicitEntry = !parser.get("file").empty();
    bool discoverErrors = false;
    if (!explicitEntry && useConfig) {
        std::error_code ec;
        auto root = LucisPipeline::getProjectRoot(pipeOpts.inputFile);
        std::vector<std::string> candidates;
        walkTestCandidates(root, candidates);

        // Skip files already covered by the entry's import graph.
        std::unordered_set<std::string> graphed;
        for (auto& u : groups[0].pipeline->units) {
            auto cu = fs::canonical(u.filePath, ec);
            auto key = (ec || cu.string().empty()) ? u.filePath : cu.string();
            graphed.insert(key);
        }

        std::sort(candidates.begin(), candidates.end());
        for (auto& cand : candidates) {
            if (graphed.count(cand)) continue;
            auto extraOpts = pipeOpts;
            extraOpts.inputFile = cand;
            auto extra = LucisPipeline::run(extraOpts);
            if (!extra || extra->hasErrors) {
                discoverErrors = true;
                continue;
            }
            groups.push_back({ std::move(extra), cand, {} });
        }
    }
    if (discoverErrors) return 1;

    // ── Filtering + dedup; a test runs in the first group that defines it ─
    bool exact = parser.has("exact");
    auto includePatterns = parser.getAll("test");
    auto skipPatterns    = parser.getAll("skip");

    std::vector<TestCase> tests;
    std::unordered_set<std::string> seenTests;
    for (auto& g : groups) {
        for (auto& tc : g.pipeline->testCases) {
            std::string key = tc.modulePath + "::" + tc.funcName;
            if (!seenTests.insert(key).second) continue;
            if (!testMatches(tc, includePatterns, exact)) continue;
            if (!skipPatterns.empty() && testMatches(tc, skipPatterns, exact)) continue;
            g.tests.push_back(tc);
            tests.push_back(tc);
        }
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

    // ── Build and run one binary per test group ────────────────────────────
    int jobs = parser.getInt("jobs", 1);
    if (jobs < 1) jobs = 1;
    int64_t timeoutMs = parser.getInt("timeout", 0);
    if (timeoutMs < 0) timeoutMs = 0;
    bool verbose = parser.has("verbose");

    int worst = 0;
    for (auto& g : groups) {
        if (g.tests.empty()) continue;

        if (groups.size() > 1 && !pipeOpts.quiet)
            std::cerr << "\nlucis: [test] group " << g.inputPath << "\n";

        BinaryBuildOptions buildOpts;
        buildOpts.optLevel = optLevel;
        buildOpts.quiet = pipeOpts.quiet;
        buildOpts.emitDebugInfo = pipeOpts.emitDebugInfo;
        buildOpts.noStd = pipeOpts.noStd;
        buildOpts.targetTriple = pipeOpts.targetTriple;
        buildOpts.entryFilePath = g.inputPath;
        buildOpts.cachePrefix = "test";
        buildOpts.progressStage = "test";

        auto customizeIrGen = [&](IRGen& irGen, const SourceUnit&, bool isEntry) {
            irGen.setTestMode(true);
            if (isEntry) {
                irGen.setTestCases(g.tests);
                irGen.setTestRunnerOptions(jobs, timeoutMs, verbose);
            }
        };

        std::string binPath;
        if (!BinaryBuilder::build(*g.pipeline, buildOpts, customizeIrGen, binPath))
            return 1;

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

        int code = WIFEXITED(status) ? WEXITSTATUS(status)
                                    : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
        if (code != 0) worst = code;
    }
    return worst;
}