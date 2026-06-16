#pragma once

#include <string>
#include <vector>
#include <memory>

#include "parser/Parser.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "types/TypeRegistry.h"
#include "semantic/SemanticDB.h"

struct SourceUnit {
    std::string filePath;
    std::string modulePath;
    ParseResult parseResult;
};

struct PipelineResult {
    std::string projectRoot;
    std::string buildDir;
    std::vector<SourceUnit> units;

    std::unique_ptr<ModuleRegistry> registry;
    std::unique_ptr<CBindings> cBindings;
    std::unique_ptr<TypeRegistry> cTypeReg;
    std::unique_ptr<semantic::SemanticDB> semanticDB;
    std::vector<std::string> cSourceFiles;

    std::vector<std::string> linkerFlags;

    bool hasErrors = false;
};

class LucisPipeline {
public:
    struct Options {
        std::string inputFile;
        std::vector<std::string> includePaths;
        std::vector<std::string> userLinkerFlags;
        std::string binaryName;
        std::string outDir;
        std::vector<std::string> stdlibPaths;
        std::vector<std::string> sourcePaths;
        bool quiet = false;
    };

    static std::unique_ptr<PipelineResult> run(const Options& opts);

private:
    static std::string getProjectRoot(const std::string& inputFile);
    static std::string filePathToModulePath(const std::string& filePath,
                                             const std::string& projectRoot);
    static std::string resolveUseToFile(const std::string& useIdent,
                                         const std::string& projectRoot,
                                         const std::vector<std::string>& searchDirs,
                                         const std::vector<std::string>& sourcePaths);
    static std::vector<std::string> extractUseModulePaths(LucisParser::ProgramContext* tree);
};
