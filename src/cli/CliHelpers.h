#pragma once

#include "config/LucisConfig.h"
#include "LLVM_Optimizer/Optimizer.h"

#include <string>
#include <optional>

struct ResolvedInput {
    std::string                filePath;
    std::optional<LucisConfig> config;
    bool                       useConfig = false;
};

ResolvedInput resolveInputFile(const std::string& explicitFile,
                               const std::string& configPath = "");
OptimizationLevel parseOptimizationLevel(const std::string& s);
