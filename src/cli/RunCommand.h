#pragma once

#include "cli/Command.h"
#include "config/LucisConfig.h"

#include <string>

class RunCommand : public Command {
public:
    std::string name() const override { return "run"; }
    std::string description() const override {
        return "Compile and run a Lucis program in one step";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;

private:
    std::string resolveInputFile(const ArgParser& parser,
                                  LucisConfig* outConfig = nullptr) const;
};
