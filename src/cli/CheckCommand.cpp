#include "cli/CheckCommand.h"
#include "cli/ArgParser.h"
#include "cli/LucisPipeline.h"

#include <iostream>

void CheckCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
}

int CheckCommand::run(const ArgParser& parser) {
    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile    = parser.get("file");
    pipeOpts.quiet        = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [check] no errors found\n";
    return 0;
}
