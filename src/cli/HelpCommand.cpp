#include "cli/HelpCommand.h"
#include "cli/ArgParser.h"

#include <iostream>
#include <string>

void HelpCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("command", "Command to get help for (optional)", false);
}

int HelpCommand::run(const ArgParser& parser) {
    std::string cmd = parser.get("command");
    if (cmd.empty()) {
        printGeneralHelp();
        return 0;
    }

    // Show help for a specific command
    for (auto* c : commands_) {
        if (c->name() == cmd) {
            ArgParser cmdParser("lucis " + c->name(), c->description());
            c->buildArgs(cmdParser);
            cmdParser.printHelp();
            return 0;
        }
    }

    std::cerr << "lucis: unknown command '" << cmd << "'\n";
    std::cerr << "Run 'lucis help' for usage.\n";
    return 1;
}

void HelpCommand::printGeneralHelp() const {
    std::cout << "lucis — The Lucis compiler\n\n"
              << "Usage:\n"
              << "  lucis <command> [args...]\n\n"
              << "Commands:\n";

    for (auto* c : commands_) {
        if (c->name() == "help" || c->name() == "version") continue;
        std::cout << "  " << c->name();
        size_t pad = c->name().size() < 12 ? 12 - c->name().size() : 1;
        std::cout << std::string(pad, ' ') << c->description() << "\n";
    }

    std::cout << "\nExamples:\n"
              << "  lucis build main.lc -o ./main -O2\n"
              << "  lucis build main.lc --emit-llvm\n"
              << "  lucis run main.lc\n"
              << "  lucis check main.lc\n"
              << "  lucis test\n"
              << "  lucis help build\n"
              << "  lucis helpc raylib InitWindow\n";
}
