#include "cli/CliHelpers.h"
#include "config/LucisConfig.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static const std::vector<std::string> kDefaultSourcePaths = {"src/"};

ResolvedInput resolveInputFile(const std::string& explicitFile,
                               const std::string& configPath,
                               bool ignoreConfig) {
    ResolvedInput result;

    // --config and --ignore-config are mutually exclusive
    if (ignoreConfig && !configPath.empty()) {
        std::cerr << "lucis: error: --ignore-config and --config are mutually exclusive\n";
        return result;
    }

    // If --config was given, load that specific config file
    if (!configPath.empty()) {
        auto cfg = LucisConfig::load(fs::absolute(configPath).string());
        if (!cfg) return result;
        result.config = std::move(*cfg);

        std::string searchDir = fs::path(fs::absolute(configPath)).parent_path().string();
        for (auto& candidate : {"src/main.lc", "main.lc"}) {
            auto full = fs::path(searchDir) / candidate;
            if (fs::exists(full)) {
                result.filePath = fs::canonical(full).string();
                result.useConfig = true;
                return result;
            }
        }

        result.config.reset();
        return result;
    }

    if (!explicitFile.empty()) {
        if (!fs::is_directory(explicitFile)) {
            std::error_code ec;
            result.filePath = fs::canonical(explicitFile, ec).string();
            result.useConfig = false;
            return result;
        }
        // Directory was passed — fall through to config-based search
    }

    // --ignore-config skips auto-detection entirely
    if (ignoreConfig) {
        if (!explicitFile.empty()) {
            // explicitFile is a directory; we need a .lc file from args
        }
        return result;
    }

    auto config = LucisConfig::findInDir(fs::current_path().string());
    if (!config) return result;

    result.config = std::move(*config);

    for (auto& candidate : {"src/main.lc", "main.lc"}) {
        if (fs::exists(candidate)) {
            std::error_code ec;
            result.filePath = fs::canonical(candidate, ec).string();
            result.useConfig = true;
            return result;
        }
    }

    result.config.reset();
    return result;
}

OptimizationLevel parseOptimizationLevel(const std::string& s) {
    if (s == "0")    return OptimizationLevel::O0;
    if (s == "1")    return OptimizationLevel::O1;
    if (s == "2")    return OptimizationLevel::O2;
    if (s == "3")    return OptimizationLevel::O3;
    if (s == "s")    return OptimizationLevel::Os;
    if (s == "z")    return OptimizationLevel::Oz;
    if (s == "fast") return OptimizationLevel::Ofast;
    return OptimizationLevel::O0;
}
