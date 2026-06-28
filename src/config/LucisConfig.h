#pragma once

#include <string>
#include <vector>
#include <optional>

struct LucisConfig {
    std::string name;
    std::string version;
    std::string binary;
    std::string outDir;

    std::vector<std::string> sourcePaths;

    // Explicit inputs (mirror --asm, --obj, --lib)
    std::vector<std::string> assemblyFiles;
    std::vector<std::string> objects;
    std::vector<std::string> staticLibs;
    std::vector<std::string> sharedLibs;

    struct BuildSettings {
        std::string optLevel;
        bool lto;
        bool staticLink;
        bool shared;
        bool fpic;
        bool noStd;
        std::string target;
        std::string codeModel;
        std::string entry;
        std::string assembler;          // nasm | as
        std::vector<std::string> assemblerFlags;
    } build;

    struct EmitSettings {
        bool llvm = false;
        bool asmFile  = false;
        bool bc   = false;
        bool obj  = false;
        bool bin  = false;
    } emit;

    struct RunSettings {
        std::string optLevel;
        bool lto;
        std::vector<std::string> args;
    } run;

    struct LinkerSettings {
        std::vector<std::string> libs;
        std::vector<std::string> libPaths;
        std::string program;            // ld | gcc | clang
        std::string script;             // linker script path
        std::string entry;              // entry point
        std::vector<std::string> flags; // -nmagic, -N, --gc-sections
        std::vector<std::string> args;  // raw args (--linker-arg)
    } linker;

    struct ScriptsConfig {
        std::vector<std::string> pre;
        std::vector<std::string> pos;
    } scripts;

    std::vector<std::string> includes;

    static std::optional<LucisConfig> load(const std::string& yamlPath);
    static std::optional<LucisConfig> findInDir(const std::string& dir);
    static std::string findConfigPath(const std::string& dir);
    static bool createDefault(const std::string& dir, const std::string& name);

    struct ValidationMsg { std::string path; std::string message; };
    static std::vector<ValidationMsg> validate(const std::string& yamlPath);

    bool save(const std::string& yamlPath) const;
};
