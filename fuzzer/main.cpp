#include "corpus.hpp"
#include "mutator.hpp"
#include "runner.hpp"
#include "result.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <regex>
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

/// Extract the `fn main` block from source (including its body).
static std::string extractMain(std::string_view src) {
    // Find the last `fn main` — it's typically at the end of the file.
    auto pos = src.rfind("fn main");
    if (pos == std::string::npos) return {};
    return std::string(src.substr(pos));
}

/// Replace any existing `fn main` block with the given replacement,
/// so the mutated source keeps calling the original test functions.
static std::string replaceMain(std::string src, const std::string& newMain) {
    // Strip any existing fn main block (relies on it being at the end).
    auto pos = src.rfind("fn main");
    if (pos != std::string::npos)
        src.resize(pos);

    src += "\n" + newMain + "\n";
    return src;
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

    // ── Config limits ───────────────────────────────────────────────
    constexpr unsigned kMaxPerKind = 5;

    // ── Stats ───────────────────────────────────────────────────────
    struct {
        std::atomic<unsigned> total{0};
        std::atomic<unsigned> ir{0};
        std::atomic<unsigned> crash{0};
        std::atomic<unsigned> checker{0};
        std::atomic<unsigned> timeout{0};
        std::atomic<unsigned> build{0};
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

        // 2. Save the original main so it still calls the test functions
        auto savedMain = extractMain(seed.source);

        // 3. Mutate everything EXCEPT the main block
        auto mutant = mutator.mutate(seed.source);

        // 4. Re-attach the saved main (mutations may have broken it)
        if (!savedMain.empty())
            mutant = replaceMain(std::move(mutant), savedMain);
        else
            mutant += "\nfn main() int32 { return 0; }\n";

        // 5. Write to temp file
        if (!writeTempSource(tmpFile, mutant)) {
            std::cerr << "[fuzzer] failed to write temp file\n";
            continue;
        }

        // 6. Run lucis
        auto outcome = runner.run(tmpFile);
        stats.total++;

        // 5. Save interesting results (max kMaxPerKind per type)
        switch (outcome.kind) {
        case FuzzResult::IRVerifierError: {
            unsigned idx = stats.ir.fetch_add(1);
            if (idx < kMaxPerKind) {
                saveResult(crashDir, "ir", idx + 1, mutant, outcome.stderr);
                std::cerr << "\n[!] IR verifier error #" << (idx + 1)
                          << " — saved to " << crashDir << "/ir_" << (idx + 1) << "/\n"
                          << outcome.stderr << "\n";
            }
            break;
        }
        case FuzzResult::Crash: {
            unsigned idx = stats.crash.fetch_add(1);
            if (idx < kMaxPerKind) {
                saveResult(crashDir, "crash", idx + 1, mutant, outcome.stderr);
                std::cerr << "\n[!] Crash #" << (idx + 1)
                          << " — saved to " << crashDir << "/crash_" << (idx + 1) << "/\n"
                          << outcome.stderr << "\n";
            }
            break;
        }
        case FuzzResult::CheckerError: {
            unsigned idx = stats.checker.fetch_add(1);
            if (idx < kMaxPerKind)
                saveResult(crashDir, "checker", idx + 1, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::Timeout: {
            unsigned idx = stats.timeout.fetch_add(1);
            if (idx < kMaxPerKind)
                saveResult(crashDir, "timeout", idx + 1, mutant, outcome.stderr);
            break;
        }
        case FuzzResult::BuildError: {
            unsigned idx = stats.build.fetch_add(1);
            if (idx < kMaxPerKind)
                saveResult(crashDir, "build", idx + 1, mutant, outcome.stderr);
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
