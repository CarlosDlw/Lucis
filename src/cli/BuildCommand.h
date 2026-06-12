#pragma once

#include "cli/Command.h"

class BuildCommand : public Command {
public:
    std::string name() const override { return "build"; }
    std::string description() const override {
        return "Compile a Lucis project into a native binary";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;
};
