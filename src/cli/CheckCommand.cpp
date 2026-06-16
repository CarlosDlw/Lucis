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
    pipeOpts.inputFile = parser.get("file");
    if (!pipeOpts.inputFile.empty()) {
        if (fs::is_directory(pipeOpts.inputFile))
            pipeOpts.inputFile.clear();
    }
    if (pipeOpts.inputFile.empty()) {
        if (!config) {
            std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
            std::cerr << "usage: lucis check <file>   or   lucis check  (from a project with lucis.yaml)\n";
            return 1;
        }
        for (auto& candidate : { "src/main.lc", "main.lc" }) {
            auto path = fs::path(candidate);
            if (fs::exists(path)) {
                pipeOpts.inputFile = fs::canonical(path).string();
                break;
            }
        }
    }
    pipeOpts.quiet        = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (config) pipeOpts.sourcePaths = config->sourcePaths;

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [check] no errors found\n";
    return 0;
}
