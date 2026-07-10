#pragma once

#include <string>
#include "result.hpp"

class Runner {
public:
    Runner(std::string lucisPath, unsigned timeoutMs = 10000);

    FuzzOutcome run(std::string_view sourcePath) const;

private:
    std::string lucisPath_;
    unsigned timeoutMs_;

    static constexpr int SignalCrash = -2;
    static constexpr int SignalTimeout = -3;
};
