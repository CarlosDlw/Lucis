#pragma once

#include "cli/Command.h"

class CheckCommand : public Command {
public:
    std::string name() const override { return "check"; }
    std::string description() const override {
        return "Type-check a Lucis project without generating code";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;
};
