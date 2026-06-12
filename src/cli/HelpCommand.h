#pragma once

#include "cli/Command.h"
#include <vector>

class HelpCommand : public Command {
public:
    void setCommands(const std::vector<Command*>& cmds) { commands_ = cmds; }

    std::string name() const override { return "help"; }
    std::string description() const override {
        return "Show help information for lucis or a specific command";
    }
    void buildArgs(ArgParser& parser) const override;
    int run(const ArgParser& parser) override;

    void printGeneralHelp() const;

private:
    std::vector<Command*> commands_;
};
