#include "corpus.hpp"
#include "mutator.hpp"
#include "runner.hpp"
#include "result.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

static std::atomic<bool> g_running = true;

static void handleSignal(int) { g_running = false; }

static bool writeTempSource(const std::string& path, const std::string& source) {
    std::ofstream f(path);
    if (!f) return false;
    f << source;
    return f.good();
}

/// Save an interesting result to fuzzer/crashes/<prefix_NNN>/ for later inspection.
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

    // Saved inside fuzzer/ so .gitignore can cover it easily
    std::string crashDir = "fuzzer/crashes";

    // ── Init ────────────────────────────────────────────────────────
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    Corpus corpus;
    corpus.loadDir(corpusDir);

    if (corpus.empty()) {
        std::cerr << "[fuzzer] no corpus found in '" << corpusDir << "'\n";
        return 1;
    }

    Mutator mutator;
    Runner runner(lucisBin, timeoutMs);

    std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
    std::string tmpFile = (tmpDir / "lucis_fuzz_tmp.lc").string();

    // ── Stats ───────────────────────────────────────────────────────
    struct {
        std::atomic<size_t> total{0};
        std::atomic<size_t> ir{0};
        std::atomic<size_t> crash{0};
        std::atomic<size_t> checker{0};
        std::atomic<size_t> timeout{0};
        std::atomic<size_t> build{0};
    } stats;

    auto printStats = [&]() {
        std::cerr << "\r[fuzzer] runs " << stats.total.load()
                  << " | ir_err " << stats.ir.load()
                  << " | crash " << stats.crash.load()
                  << " | chk_err " << stats.checker.load()
                  << " | timeout " << stats.timeout.load()
                  << " | build_err " << stats.build.load()
                  << "   " << std::flush;
    };

    // ── Fuzz loop ───────────────────────────────────────────────────
    std::cerr << "[fuzzer] starting...\n";
    std::error_code ec;

    for (unsigned iter = 0; g_running && (maxIters == 0 || iter < maxIters); iter++) {
        // 1. Pick a seed from the corpus
        auto& seed = corpus.randomEntry();

        // 2. Mutate it
        auto mutant = mutator.mutate(seed.source);

        // 3. Write to temp file
        if (!writeTempSource(tmpFile, mutant)) {
            std::cerr << "[fuzzer] failed to write temp file\n";
            continue;
        }

        // 4. Run lucis
        auto outcome = runner.run(tmpFile);
        stats.total++;

        // 5. Save interesting results
        switch (outcome.kind) {
        case FuzzResult::IRVerifierError: {
            stats.ir++;
            unsigned idx = stats.ir.load();
            saveResult(crashDir, "ir", idx, mutant, outcome.stderr);
            std::cerr << "\n[!] IR verifier error #" << idx
                      << " — saved to " << crashDir << "/ir_" << idx << "/\n"
                      << outcome.stderr << "\n";
            break;
        }
        case FuzzResult::Crash: {
            stats.crash++;
            unsigned idx = stats.crash.load();
            saveResult(crashDir, "crash", idx, mutant, outcome.stderr);
            std::cerr << "\n[!] Crash #" << idx
                      << " — saved to " << crashDir << "/crash_" << idx << "/\n";
            if (!outcome.stderr.empty())
                std::cerr << outcome.stderr.substr(0, 500) << "\n";
            break;
        }
        case FuzzResult::CheckerError: {
            stats.checker++;
            unsigned idx = stats.checker.load();
            saveResult(crashDir, "checker", idx, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::Timeout: {
            stats.timeout++;
            unsigned idx = stats.timeout.load();
            saveResult(crashDir, "timeout", idx, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::BuildError: {
            stats.build++;
            unsigned idx = stats.build.load();
            saveResult(crashDir, "build", idx, mutant, outcome.stderr);
            break;
        }
        default:
            break;
        }

        if (iter % 100 == 0)
            printStats();
    }

    printStats();
    std::cerr << "\n[fuzzer] done.\n";
    return 0;
}
