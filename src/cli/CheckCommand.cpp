#include "cli/CheckCommand.h"
#include "cli/ArgParser.h"
#include "cli/LucisPipeline.h"
#include "config/LucisConfig.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void CheckCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
}

int CheckCommand::run(const ArgParser& parser) {
    auto config = LucisConfig::findInDir(fs::current_path().string());

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile    = parser.get("file");
    pipeOpts.quiet        = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (config) pipeOpts.sourcePaths = config->sourcePaths;

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [check] no errors found\n";
    return 0;
}
