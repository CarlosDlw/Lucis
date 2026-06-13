#pragma once

#include "cli/Command.h"
#include "config/LucisConfig.h"

#include <string>

class BuildCommand : public Command {
public:
    std::string name() const override { return "build"; }
    std::string description() const override {
        return "Compile a Lucis project into a native binary";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;

private:
    // Resolve the input file from lucis.yaml when no explicit path is given.
    // Returns empty string if neither file nor config can be found.
    std::string resolveInputFile(const ArgParser& parser,
                                  LucisConfig* outConfig = nullptr) const;
};
