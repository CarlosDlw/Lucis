#pragma once

#include "cli/Command.h"

class VersionCommand : public Command {
public:
    std::string name() const override { return "version"; }
    std::string description() const override {
        return "Show the compiler version";
    }
    void buildArgs(ArgParser& parser) const override {
        parser.addFlag("version", 'v', "Show version");
    }
    int run(const ArgParser& parser) override {
        std::cout << "lucis v0.0.1 beta\n";
        return 0;
    }
};
