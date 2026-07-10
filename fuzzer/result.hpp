#pragma once

#include <string>
#include <string_view>

enum class FuzzResult {
    Ok,
    BuildError,
    CheckerError,
    IRVerifierError,
    Crash,
    Timeout,
};

struct FuzzOutcome {
    FuzzResult kind;
    int exitCode = 0;
    std::string stderr;
    std::string stdout;
    bool timedOut = false;

    static FuzzResult classify(std::string_view stderr_, int exitCode);
    static std::string_view describe(FuzzResult r);
};
