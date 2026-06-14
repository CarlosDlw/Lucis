#pragma once

#include <string>
#include <vector>
#include <memory>

#include "parser/Parser.h"
#include "namespace/NamespaceRegistry.h"
#include "ffi/CBindings.h"
#include "types/TypeRegistry.h"

struct SourceUnit {
    std::string filePath;
    std::string namespaceName;
    ParseResult parseResult;
};

// Result of the shared pipeline (scan → parse → namespace → C headers → check).
struct PipelineResult {
    std::string projectRoot;
    std::string buildDir;
    std::vector<SourceUnit> units;

    std::unique_ptr<NamespaceRegistry> registry;
    std::unique_ptr<CBindings> cBindings;
    std::unique_ptr<TypeRegistry> cTypeReg;
    std::vector<std::string> cSourceFiles;

    // Auto-detected linker flags from C header resolution
    std::vector<std::string> linkerFlags;

    bool hasErrors = false;
};

// Shared compilation pipeline steps (scan → parse → namespace → C → check).
class LucisPipeline {
public:
    struct Options {
        std::string inputFile;
        std::vector<std::string> includePaths;
        std::vector<std::string> userLinkerFlags;
        std::string binaryName;   // from lucis.yaml, used as default output name
        std::string outDir;       // from lucis.yaml out_dir (default: build)
        std::vector<std::string> stdlibPaths; // Additional stdlib search paths
        bool quiet = false;
    };

    static std::unique_ptr<PipelineResult> run(const Options& opts);

private:
    static std::string getProjectRoot(const std::string& inputFile);
    static std::string extractNamespace(LucisParser::ProgramContext* tree);
};
