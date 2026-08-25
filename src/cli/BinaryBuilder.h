#pragma once

#include <functional>
#include <string>

#include "LLVM_Optimizer/Optimizer.h"

class PipelineResult;
class IRGen;
struct SourceUnit;

struct BinaryBuildOptions {
    OptimizationLevel optLevel = OptimizationLevel::O0;
    bool useLTO = false;
    bool useClean = false;
    bool quiet = false;
    bool emitDebugInfo = false;
    bool noStd = false;
    std::string targetTriple;
    std::string entryFilePath;    // canonical path of the entry unit
    const char* cachePrefix = "run";   // cache file name prefix (e.g. "run", "test")
    const char* progressStage = "run"; // [stage] label in progress logs
};

// Shared pipeline→IR→link core used by `lucis run` and `lucis test`.
//
// Takes an already-executed LucisPipeline, generates IR for every unit links
// everything into a single LLVM module, optimizes it and produces a cached
// native binary (cache key: MD5 of IR + toolchain inputs, under
// <projectRoot>/.lucis/cache/). `customizeIrGen` lets callers inject extra
// IRGen configuration per unit (e.g. test mode) before generation.
class BinaryBuilder {
public:
    static bool build(PipelineResult& pipeline,
                      const BinaryBuildOptions& opts,
                      const std::function<void(IRGen&, const SourceUnit&, bool isEntry)>& customizeIrGen,
                      std::string& outBinaryPath);
};