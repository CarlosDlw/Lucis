#include "mutator.hpp"
#include "runner.hpp"
#include "result.hpp"
#include "corpus.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

static std::atomic<bool> g_running = true;
static void handleSignal(int) { g_running = false; }

static bool writeTempSource(const std::string& path, const std::string& source) {
    std::ofstream f(path);
    if (!f) return false;
    f << source;
    return f.good();
}

static void saveResult(const std::string& subdir, const std::string& prefix,
                       unsigned index, const std::string& source,
                       const std::string& log) {
    auto dir = std::filesystem::path(subdir) / (prefix + "_" + std::to_string(index));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return;

    {
        std::ofstream f(dir / "source.lc");
        if (f) f << source;
    }
    {
        std::ofstream f(dir / "log.txt");
        if (f) f << log;
    }
}

int main(int argc, char** argv) {
    // ── Config ──────────────────────────────────────────────────────
    std::string lucisBin = argc > 1 ? argv[1] : "lucis";
    std::string corpusDir = argc > 2 ? argv[2] : "tests";
    unsigned timeoutMs = 10000;
    unsigned maxIters = 0; // 0 = infinite
    bool keepFails = false; // only save interesting (bugs + checker errors)
    bool bugsOnly = false;  // only save REAL bugs (crash, IR err, checker crash)

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--timeout" && i + 1 < argc)
            timeoutMs = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (arg == "--iters" && i + 1 < argc)
            maxIters = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (arg == "--keep-fails")
            keepFails = true;
        else if (arg == "--bugs-only")
            bugsOnly = true;
        else if (arg == "--lucis" && i + 1 < argc)
            lucisBin = argv[++i];
        else if (arg == "--corpus" && i + 1 < argc)
            corpusDir = argv[++i];
    }

    std::string crashDir = "fuzzer/crashes";

    // ── Init ────────────────────────────────────────────────────────
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    Corpus corpus;
    corpus.loadDir(corpusDir);

    Mutator mutator;
    mutator.setCorpus(&corpus);

    Runner runner(lucisBin, timeoutMs);

    std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
    std::string tmpFile = (tmpDir / "lucis_fuzz_tmp.lc").string();

    constexpr unsigned kMaxPerKind = 5;

    // ── Stats ───────────────────────────────────────────────────────
    struct {
        std::atomic<unsigned> total{0};
        std::atomic<unsigned> ir{0};
        std::atomic<unsigned> crash{0};
        std::atomic<unsigned> checker{0};
        std::atomic<unsigned> timeout{0};
        std::atomic<unsigned> build{0};
        std::atomic<unsigned> ckCrash{0};
        std::atomic<unsigned> ok{0};
    } stats;

    std::map<GenStrategy, unsigned> strategyCounts;
    std::map<GenStrategy, unsigned> strategyBugs;

    auto printStats = [&]() {
        std::cerr << "\r[fuzzer] runs " << stats.total.load()
                  << " | ok " << stats.ok.load()
                  << " | ir_err " << stats.ir.load()
                  << " | crash " << stats.crash.load()
                  << " | ck_crash " << stats.ckCrash.load()
                  << " | chk_err " << stats.checker.load()
                  << " | timeout " << stats.timeout.load()
                  << " | build_err " << stats.build.load()
                  << "   " << std::flush;
    };

    // ── Fuzz loop ───────────────────────────────────────────────────
    std::cerr << "[fuzzer] starting...\n";

    for (unsigned iter = 0; g_running && (maxIters == 0 || iter < maxIters); iter++) {
        // 1. Generate code using a random strategy
        auto mutant = mutator.generate();
        GenStrategy strat = mutator.lastStrategy();
        strategyCounts[strat]++;

        // 2. Write to temp file
        if (!writeTempSource(tmpFile, mutant)) {
            std::cerr << "[fuzzer] failed to write temp file\n";
            continue;
        }

        // 3. Run two-phase (check → build)
        auto outcome = runner.run(tmpFile);
        stats.total++;

        // 4. Count and save interesting results
        bool isBug = false;
        switch (outcome.kind) {
        case FuzzResult::IRVerifierError: {
            unsigned idx = stats.ir.fetch_add(1);
            if (idx < kMaxPerKind) {
                saveResult(crashDir, "ir", idx + 1, mutant, outcome.stderr);
                std::cerr << "\n[!] IR verifier error #" << (idx + 1) << "\n";
            }
            isBug = true;
            break;
        }
        case FuzzResult::CompilerCrash: {
            unsigned idx = stats.crash.fetch_add(1);
            if (idx < kMaxPerKind) {
                saveResult(crashDir, "crash", idx + 1, mutant, outcome.stderr);
                std::cerr << "\n[!] Compiler crash #" << (idx + 1) << "\n";
            }
            isBug = true;
            break;
        }
        case FuzzResult::CheckerCrash: {
            unsigned idx = stats.ckCrash.fetch_add(1);
            if (idx < kMaxPerKind) {
                saveResult(crashDir, "ck_crash", idx + 1, mutant, outcome.stderr);
                std::cerr << "\n[!] Checker crash #" << (idx + 1) << "\n";
            }
            isBug = true;
            break;
        }
        case FuzzResult::CheckerError: {
            unsigned idx = stats.checker.fetch_add(1);
            if (!bugsOnly && idx < kMaxPerKind) {
                saveResult(crashDir, "checker", idx + 1, mutant, outcome.stderr);
            }
            break;
        }
        case FuzzResult::Timeout: {
            unsigned idx = stats.timeout.fetch_add(1);
            if (!bugsOnly && idx < kMaxPerKind)
                saveResult(crashDir, "timeout", idx + 1, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::BuildError: {
            unsigned idx = stats.build.fetch_add(1);
            if (!bugsOnly && keepFails && idx < kMaxPerKind)
                saveResult(crashDir, "build", idx + 1, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::Ok: {
            stats.ok++;
            break;
        }
        }

        if (isBug)
            strategyBugs[strat]++;

        if (iter % 100 == 0)
            printStats();
    }

    printStats();
    std::cerr << "\n[fuzzer] done.\n";

    // ── Per-strategy summary ────────────────────────────────────────
    std::cerr << "\n[summary] Per-strategy stats:\n";
    for (int i = 0; i < static_cast<int>(GenStrategy::Count); i++) {
        auto s = static_cast<GenStrategy>(i);
        auto total = strategyCounts[s];
        auto bugs = strategyBugs[s];
        if (total > 0) {
            std::cerr << "  " << mutator.strategyName(i)
                      << ": " << total << " runs, " << bugs << " bugs"
                      << " (" << (bugs * 100 / total) << "%)\n";
        }
    }

    // ── Best strategy ───────────────────────────────────────────────
    GenStrategy best = GenStrategy::GrammarSimple;
    unsigned bestRate = 0;
    for (int i = 0; i < static_cast<int>(GenStrategy::Count); i++) {
        auto s = static_cast<GenStrategy>(i);
        auto total = strategyCounts[s];
        auto bugs = strategyBugs[s];
        if (total > 0) {
            unsigned rate = bugs * 10000 / total;
            if (rate > bestRate) {
                bestRate = rate;
                best = s;
            }
        }
    }
    std::cerr << "[summary] Best strategy: " << mutator.strategyName(static_cast<size_t>(best))
              << " (" << (bestRate / 100) << "." << (bestRate % 100) << "% bug rate)\n";

    return 0;
}
