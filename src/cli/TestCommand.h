#pragma once

#include "cli/Command.h"

class TestCommand : public Command {
public:
    std::string name() const override { return "test"; }
    std::string description() const override {
        return "Run the Lucis test suite";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;
};
