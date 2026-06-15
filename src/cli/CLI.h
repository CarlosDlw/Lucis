#pragma once

#include <string>
#include <vector>
#include <memory>

#include "parser/Parser.h"
#include "namespace/ModuleRegistry.h"

class Command;

struct CLIOptions {
    bool isLSP = false;
    std::string inputFile;
    std::string outputFile;
    int         optimizationLevel = 0;
    bool        quiet       = false;
    bool        showIR      = false;
    bool        showHelp    = false;
    bool        showVersion = false;
    bool        runJIT      = false;   // lucis run <file.lc>

    std::vector<std::string> linkerFlags;
    std::vector<std::string> libPaths;
    std::vector<std::string> includePaths;
    std::vector<std::string> runArgs;
};

class Command;

class CLI {
public:
    CLI(int argc, char* argv[]);
    ~CLI();
    int run();

private:
    int    argc_ = 0;
    char** argv_ = nullptr;

    std::vector<std::unique_ptr<Command>> commands_;
    Command* findCommand(const std::string& name) const;

    CLIOptions legacyOpts_;

    // Parse legacy positional mode (no subcommand given)
    bool parseLegacy(int argc, char* argv[]);
};
