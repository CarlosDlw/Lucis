#include "cli/CheckCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "config/LucisConfig.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

void CheckCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file (auto-resolved from lucis.yaml if omitted)", false);
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    parser.addFlag("ignore-config", '\0', "Ignore lucis.yaml, use CLI flags only");
}

int CheckCommand::run(const ArgParser& parser) {
    auto resolved = resolveInputFile(parser.get("file"), "",
                                     parser.has("ignore-config"));
    if (resolved.filePath.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis check <file>   or   lucis check  (from a project with lucis.yaml)\n";
        return 1;
    }

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = resolved.filePath;
    pipeOpts.quiet     = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (resolved.useConfig) {
        if (pipeOpts.includePaths.empty())
            pipeOpts.includePaths = resolved.config->build.includePaths;
    }

    pipeOpts.sourcePaths = resolved.useConfig ? resolved.config->sourcePaths : std::vector<std::string>{"src/"};

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [check] no errors found\n";
    return 0;
}
