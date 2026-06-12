#pragma once

#include "cli/Command.h"

class RunCommand : public Command {
public:
    std::string name() const override { return "run"; }
    std::string description() const override {
        return "Compile and execute a Lucis program via JIT";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;
};
