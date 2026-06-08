#include "cli/CLI.h"
#include "cli/Command.h"
#include "cli/ArgParser.h"
#include "cli/BuildCommand.h"
#include "cli/RunCommand.h"
#include "cli/CheckCommand.h"
#include "cli/TestCommand.h"
#include "cli/HelpCommand.h"
#include "cli/VersionCommand.h"
#include "helpc/HelpC.h"
#include "lsp/LspServer.h"
#include "machine_code/CodeGen.h"

#include <iostream>
#include <algorithm>

CLI::~CLI() = default;

CLI::CLI(int argc, char* argv[])
    : argc_(argc), argv_(argv)
{
    // Register all commands
    commands_.push_back(std::make_unique<BuildCommand>());
    commands_.push_back(std::make_unique<RunCommand>());
    commands_.push_back(std::make_unique<CheckCommand>());
    commands_.push_back(std::make_unique<TestCommand>());
    commands_.push_back(std::make_unique<VersionCommand>());

    auto helpCmd = std::make_unique<HelpCommand>();
    {
        std::vector<Command*> cmdPtrs;
        for (auto& c : commands_)
            cmdPtrs.push_back(c.get());
        cmdPtrs.push_back(helpCmd.get());
        helpCmd->setCommands(cmdPtrs);
    }
    commands_.push_back(std::move(helpCmd));
}

Command* CLI::findCommand(const std::string& name) const {
    for (auto& c : commands_) {
        if (c->name() == name) return c.get();
    }
    return nullptr;
}

int CLI::run() {
    if (argc_ < 2) {
        // No args → show help
        auto* help = findCommand("help");
        if (help) {
            ArgParser parser("lux", "The Lux compiler");
            help->buildArgs(parser);
            return help->run(parser);
        }
        return 1;
    }

    std::string first = argv_[1];

    // Early dispatch for special legacy commands
    if (first == "lsp") {
        LspServer server;
        return server.run();
    }

    if (first == "helpc") {
        HelpCCommand cmd;
        if (!HelpC::parseArgs(argc_, argv_, cmd)) {
            HelpC::printUsage();
            return 1;
        }
        return HelpC::run(cmd);
    }

    if (first == "--version" || first == "-v") {
        auto* ver = findCommand("version");
        if (ver) {
            ArgParser parser("lux version", ver->description());
            ver->buildArgs(parser);
            return ver->run(parser);
        }
        return 1;
    }

    if (first == "--print-builtins-path") {
        std::cout << CodeGen::builtinsLibraryPath() << std::endl;
        return 0;
    }

    // Check if first arg is a registered command
    if (auto* cmd = findCommand(first)) {
        // Build a stripped argv with program name + remaining args
        std::vector<const char*> stripped;
        stripped.push_back(argv_[0]);
        for (int i = 2; i < argc_; i++)
            stripped.push_back(argv_[i]);

        ArgParser parser("lux " + cmd->name(), cmd->description());
        cmd->buildArgs(parser);

        if (!parser.parse(stripped.size(), const_cast<char**>(stripped.data()))) {
            if (parser.wantsHelp()) return 0;
            return 1;
        }

        return cmd->run(parser);
    }

    // Legacy: handle positional args (lux <file.lx> ...) — deprecated
    bool legacyResult = parseLegacy(argc_, argv_);
    if (legacyResult) {
        if (legacyOpts_.showHelp) {
            auto* help = findCommand("help");
            if (help) {
                ArgParser parser("lux", "The Lux compiler");
                help->buildArgs(parser);
                return help->run(parser);
            }
            return 0;
        }

        // Legacy compile mode: delegate to BuildCommand
        if (auto* build = findCommand("build")) {
            // Build args from legacy options
            ArgParser parser("lux build", build->description());
            build->buildArgs(parser);

            // Simulate parse with legacy options
            // We need to reconstruct argv for the build parser
            std::vector<std::string> buildArgs;
            buildArgs.push_back("lux");
            buildArgs.push_back("build");
            buildArgs.push_back(legacyOpts_.inputFile);
            if (!legacyOpts_.outputFile.empty()) {
                buildArgs.push_back("-o");
                buildArgs.push_back(legacyOpts_.outputFile);
            }
            if (legacyOpts_.optimizationLevel > 0)
                buildArgs.push_back("-O" + std::to_string(legacyOpts_.optimizationLevel));
            if (legacyOpts_.quiet)
                buildArgs.push_back("-q");
            if (legacyOpts_.showIR)
                buildArgs.push_back("--emit-llvm");
            for (auto& lf : legacyOpts_.linkerFlags)
                buildArgs.push_back(lf);
            for (auto& lp : legacyOpts_.libPaths)
                buildArgs.push_back("-L" + lp);
            for (auto& ip : legacyOpts_.includePaths)
                buildArgs.push_back("-I" + ip);

            std::vector<char*> buildArgv;
            for (auto& a : buildArgs)
                buildArgv.push_back(&a[0]);

            parser.parse(buildArgv.size(), buildArgv.data());
            return build->run(parser);
        }
    }

    std::cerr << "lux: unknown command '" << first << "'\n";
    std::cerr << "Run 'lux help' for usage.\n";
    return 1;
}

bool CLI::parseLegacy(int argc, char* argv[]) {
    if (argc < 2) return false;

    std::string first = argv[1];

    if (first == "help" || first == "--help" || first == "-h") {
        legacyOpts_.showHelp = true;
        return true;
    }

    if (first == "version" || first == "--version" || first == "-v") {
        legacyOpts_.showVersion = true;
        return true;
    }

    // Treat as legacy compile mode: first arg = input file
    legacyOpts_.inputFile = first;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-o1") {
            legacyOpts_.optimizationLevel = 1;
        } else if (arg == "-o2") {
            legacyOpts_.optimizationLevel = 2;
        } else if (arg == "-o3") {
            legacyOpts_.optimizationLevel = 3;
        } else if (arg == "-q" || arg == "--quiet") {
            legacyOpts_.quiet = true;
        } else if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
            legacyOpts_.linkerFlags.push_back(arg);
        } else if (arg.rfind("-L", 0) == 0 && arg.size() > 2) {
            legacyOpts_.libPaths.push_back(arg);
        } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            legacyOpts_.includePaths.push_back(arg);
        } else if (arg[0] != '-') {
            legacyOpts_.outputFile = arg;
        } else {
            std::cerr << "\033[1;31mlux:\033[0m unknown flag '" << arg << "'\n";
            std::cerr << "Run 'lux help' for usage.\n";
            return false;
        }
    }

    legacyOpts_.showIR = legacyOpts_.outputFile.empty();
    return true;
}
