#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

struct LucisConfig {
    std::string name;
    std::string version;
    std::string binary;
    std::string outDir;

    struct Tools {
        std::string nasm;
        std::string ld;
        std::string objcopy;
    } tools;

    struct AssemblySettings {
        std::vector<std::string> files;
        std::string assembler;
        std::vector<std::string> flags;
    } assembly;

    std::vector<std::string> sourcePaths;

    struct BuildSettings {
        std::string target;
        std::string optLevel;
        bool noStd;
        bool lto;
        bool staticLink;
        bool shared;
        bool fpic;
        std::string codeModel;
        std::vector<std::string> includePaths;
        std::map<std::string, std::string> defines;
    } build;

    struct InputSettings {
        std::vector<std::string> objects;
        std::vector<std::string> staticLibs;
        std::vector<std::string> sharedLibs;
    } inputs;

    struct LinkerSettings {
        std::string program;
        std::string script;
        std::string entry;
        std::vector<std::string> libs;
        std::vector<std::string> libPaths;
        std::vector<std::string> flags;
        std::vector<std::string> args;
    } linker;

    struct OutputSettings {
        std::string path;
        bool strip;
        bool emitBin;
        bool emitLlvm;
        bool emitAsm;
        bool emitBc;
        bool emitObj;
    } output;

    struct ScriptsConfig {
        std::map<std::string, std::string> env;
        std::vector<std::string> pre;
        std::vector<std::string> pos;
    } scripts;

    struct RunSettings {
        std::string optLevel;
        bool lto;
        std::vector<std::string> args;
    } run;

    static std::optional<LucisConfig> load(const std::string& yamlPath);
    static std::optional<LucisConfig> findInDir(const std::string& dir);
    static std::string findConfigPath(const std::string& dir);
    static bool createDefault(const std::string& dir, const std::string& name);

    struct ValidationMsg { std::string path; std::string message; };
    static std::vector<ValidationMsg> validate(const std::string& yamlPath);

    bool save(const std::string& yamlPath) const;
};
