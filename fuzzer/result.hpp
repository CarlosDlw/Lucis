#pragma once

#include <string>
#include <string_view>
#include <vector>

// ── Outcome from running the compiler ─────────────────────────────
enum class FuzzResult {
    Ok,                // checker passed + build succeeded
    BuildError,        // build system error (infrastructure issue)
    CheckerError,      // checker rejected code with a KNOWN diagnostic
    CheckerCrash,      // checker itself crashed (SIGSEGV, SIGABRT, etc.) — REAL BUG
    IRVerifierError,   // checker passed, but IRGen crashed/asserted — REAL BUG
    CompilerCrash,     // full compile crashed after checker passed — REAL BUG
    LinkerError,       // checker passed, but linker failed (missing symbols, etc.) — REAL BUG
    Timeout,           // timed out
};

struct FuzzOutcome {
    FuzzResult kind = FuzzResult::Ok;

    // Phase information
    bool checkerPassed = false;
    bool checkerTimedOut = false;
    int  checkerExitCode = 0;
    int  checkerTermSig = 0;
    std::string checkerStdout;
    std::string checkerStderr;

    bool builderTimedOut = false;
    int  builderExitCode = 0;
    int  builderTermSig = 0;
    std::string builderStdout;
    std::string builderStderr;

    // Combined stderr (for logging)
    std::string stderr;
};

// ── Classifier: anti-false-positive ───────────────────────────────
class ResultClassifier {
public:
    /// Given a FuzzOutcome from a two-phase run, classify definitively.
    /// Phase1 = checker, Phase2 = build (only runs if checker passed).
    static FuzzOutcome classify(FuzzOutcome raw);

private:
    /// Checker-stderr patterns that indicate EXPECTED rejections (not bugs).
    static bool isExpectedCheckerError(const std::string& stderr);

    /// Shell/stderr infrastructure errors (e.g. "sh: --: invalid option") —
    /// never a Lucis bug, always an infrastructure issue.
    static bool isShellError(const std::string& stderr);

    /// Checker-stderr patterns that indicate a CHECKER CRASH (SIGABRT, assertion
    /// failure, or internal error that is NOT a normal diagnostic).
    static bool isCheckerCrash(const std::string& stderr);

    /// Builder-stderr patterns for IR verifier errors
    static bool isIRVerifierError(const std::string& stderr);

    /// Builder-stderr patterns for linker errors (missing symbols)
    static bool isLinkerError(const std::string& stderr);

    static const std::vector<std::string> kCheckerErrors;
};

// ── Signal decoding ───────────────────────────────────────────────
constexpr int SignalTimeout = -1;
constexpr int SignalCrash   = -2;
