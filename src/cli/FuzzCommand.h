#pragma once

#include "cli/Command.h"

class FuzzCommand : public Command {
public:
    std::string name() const override { return "fuzz"; }
    std::string description() const override {
        return "Fuzz-test a Lucis program (#[fuzz] targets), cargo-fuzz style";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;
};