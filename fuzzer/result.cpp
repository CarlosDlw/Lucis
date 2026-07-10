#include "result.hpp"

FuzzResult FuzzOutcome::classify(std::string_view stderr_, int exitCode) {
    if (exitCode == 0) return FuzzResult::Ok;

    // IR verifier errors are the main target — they indicate the checker
    // let something through that IRGen couldn't handle.
    if (stderr_.find("LLVM IR") != std::string_view::npos ||
        stderr_.find("verification") != std::string_view::npos ||
        stderr_.find("Assertion") != std::string_view::npos)
        return FuzzResult::IRVerifierError;

    // Checker errors are expected for invalid code, but still worth logging
    // to detect new false positives.
    if (stderr_.find("semantic") != std::string_view::npos ||
        stderr_.find("type mismatch") != std::string_view::npos ||
        stderr_.find("cannot") != std::string_view::npos ||
        stderr_.find("expected") != std::string_view::npos ||
        stderr_.find("does not allow") != std::string_view::npos)
        return FuzzResult::CheckerError;

    if (exitCode < 0 || exitCode >= 128)
        return FuzzResult::Crash;

    return FuzzResult::BuildError;
}

std::string_view FuzzOutcome::describe(FuzzResult r) {
    switch (r) {
    case FuzzResult::Ok:            return "ok";
    case FuzzResult::BuildError:    return "build_error";
    case FuzzResult::CheckerError:  return "checker_error";
    case FuzzResult::IRVerifierError: return "ir_verifier_error";
    case FuzzResult::Crash:         return "crash";
    case FuzzResult::Timeout:       return "timeout";
    }
    return "unknown";
}
