#pragma once

#include "result.hpp"

#include <string>
#include <string_view>

class Runner {
public:
    Runner(std::string lucisPath, unsigned timeoutMs);

    /// Two-phase run: check → (if pass) build
    FuzzOutcome run(std::string_view sourcePath) const;

private:
    struct PhaseResult {
        bool ok = false;
        bool timedOut = false;
        int exitCode = 0;
        int termSig = 0;
        std::string stdout;
        std::string stderr;
    };

    PhaseResult runPhase(const std::string& cmd, unsigned timeoutMs) const;

    std::string lucisPath_;
    unsigned timeoutMs_;
};
