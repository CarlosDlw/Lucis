#pragma once

#include <string>
#include <vector>
#include <optional>

struct LucisConfig {
    std::string name;
    std::string version;
    std::string description;

    std::string binary;
    std::string outDir;

    std::vector<std::string> sourcePaths;
    std::vector<std::string> excludePatterns;

    struct BuildSettings {
        std::string optLevel;
        bool lto;
        bool staticLink;
        bool shared;
        bool fpic;
        bool quiet;
        bool emitLlvm;
        bool emitAsm;
        bool emitBc;
        bool emitObj;
    } build;

    struct RunSettings {
        std::string optLevel;
        bool lto;
        bool quiet;
        bool clean;
        std::vector<std::string> args;
    } run;

    struct LinkerSettings {
        std::vector<std::string> flags;
        std::string rpath;
        std::vector<std::string> libs;
        std::vector<std::string> libPaths;
    } linker;

    std::vector<std::string> includes;

    static std::optional<LucisConfig> load(const std::string& yamlPath);
    static std::optional<LucisConfig> findInDir(const std::string& dir);
    static std::string findConfigPath(const std::string& dir);
    static bool createDefault(const std::string& dir, const std::string& name);

    bool save(const std::string& yamlPath) const;
};
