#include "config/LucisConfig.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

static std::vector<std::string> toStringVec(const YAML::Node& node) {
    std::vector<std::string> result;
    if (!node.IsDefined() || !node.IsSequence()) return result;
    for (const auto& item : node)
        result.push_back(item.Scalar());
    return result;
}

static std::string optOrDefault(const YAML::Node& node, const std::string& key,
                                 const std::string& def) {
    if (!node.IsDefined() || !node[key].IsDefined()) return def;
    return node[key].Scalar();
}

static bool boolOrDefault(const YAML::Node& node, const std::string& key,
                           bool def) {
    if (!node.IsDefined() || !node[key].IsDefined()) return def;
    return node[key].as<bool>();
}

std::optional<LucisConfig> LucisConfig::load(const std::string& yamlPath) {
    if (!fs::exists(yamlPath)) return std::nullopt;

    try {
        YAML::Node root = YAML::LoadFile(yamlPath);

        LucisConfig cfg;
        cfg.name        = root["name"].Scalar();
        cfg.version     = optOrDefault(root, "version", "0.0.1");
        cfg.description = optOrDefault(root, "description", "");

        cfg.binary  = optOrDefault(root, "binary", cfg.name);
        cfg.outDir  = optOrDefault(root, "out_dir", "build");

        cfg.sourcePaths     = toStringVec(root["source"]);
        cfg.excludePatterns = toStringVec(root["exclude"]);

        {
            auto b = root["build"];
            cfg.build.optLevel  = optOrDefault(b, "opt_level", "O0");
            cfg.build.lto       = boolOrDefault(b, "lto", false);
            cfg.build.staticLink= boolOrDefault(b, "static", false);
            cfg.build.shared    = boolOrDefault(b, "shared", false);
            cfg.build.fpic      = boolOrDefault(b, "fpic", true);
            cfg.build.quiet     = boolOrDefault(b, "quiet", false);
            cfg.build.emitLlvm  = boolOrDefault(b, "emit_llvm", false);
            cfg.build.emitAsm   = boolOrDefault(b, "emit_asm", false);
            cfg.build.emitBc    = boolOrDefault(b, "emit_bc", false);
            cfg.build.emitObj   = boolOrDefault(b, "emit_obj", false);
        }
        {
            auto r = root["run"];
            cfg.run.optLevel = optOrDefault(r, "opt_level", "O0");
            cfg.run.lto      = boolOrDefault(r, "lto", false);
            cfg.run.quiet    = boolOrDefault(r, "quiet", false);
            cfg.run.clean    = boolOrDefault(r, "clean", false);
            cfg.run.args     = toStringVec(r["args"]);
        }
        {
            auto l = root["linker"];
            cfg.linker.flags    = toStringVec(l["flags"]);
            cfg.linker.rpath    = optOrDefault(l, "rpath", "");
            cfg.linker.libs     = toStringVec(l["libs"]);
            cfg.linker.libPaths = toStringVec(l["lib_paths"]);
        }

        cfg.includes = toStringVec(root["includes"]);

        return cfg;
    } catch (const std::exception& e) {
        std::cerr << "[lucis-config] error loading " << yamlPath
                  << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

std::optional<LucisConfig> LucisConfig::findInDir(const std::string& dir) {
    auto path = findConfigPath(dir);
    if (path.empty()) return std::nullopt;
    return load(path);
}

std::string LucisConfig::findConfigPath(const std::string& dir) {
    try {
        fs::path p = fs::absolute(dir);
        while (true) {
            auto candidate = p / "lucis.yaml";
            std::error_code ec;
            if (fs::exists(candidate, ec) && !ec)
                return candidate.string();
            if (!p.has_parent_path() || p == p.parent_path()) break;
            p = p.parent_path();
        }
    } catch (...) {}
    return {};
}

bool LucisConfig::createDefault(const std::string& dir, const std::string& name) {
    LucisConfig cfg;
    cfg.name        = name;
    cfg.version     = "0.0.1";
    cfg.description = "";
    cfg.binary      = name;
    cfg.outDir      = "build";
    cfg.sourcePaths = {"src/"};

    cfg.build.optLevel   = "O2";
    cfg.build.lto        = false;
    cfg.build.staticLink = false;
    cfg.build.shared     = false;
    cfg.build.fpic       = true;
    cfg.build.quiet      = false;
    cfg.build.emitLlvm   = false;
    cfg.build.emitAsm    = false;
    cfg.build.emitBc     = false;
    cfg.build.emitObj    = false;

    cfg.run.optLevel = "O0";
    cfg.run.lto      = false;
    cfg.run.quiet    = false;
    cfg.run.clean    = false;

    cfg.linker.rpath = "";
    cfg.includes     = {};

    return cfg.save((fs::path(dir) / "lucis.yaml").string());
}

bool LucisConfig::save(const std::string& yamlPath) const {
    try {
        YAML::Node root;

        root["name"]        = name;
        root["version"]     = version;
        root["description"] = description;
        root["binary"]      = binary;
        root["out_dir"]     = outDir;

        for (const auto& s : sourcePaths)
            root["source"].push_back(s);
        for (const auto& e : excludePatterns)
            root["exclude"].push_back(e);

        root["build"]["opt_level"]   = build.optLevel;
        root["build"]["lto"]         = build.lto;
        root["build"]["static"]      = build.staticLink;
        root["build"]["shared"]      = build.shared;
        root["build"]["fpic"]        = build.fpic;
        root["build"]["quiet"]       = build.quiet;
        root["build"]["emit_llvm"]   = build.emitLlvm;
        root["build"]["emit_asm"]    = build.emitAsm;
        root["build"]["emit_bc"]     = build.emitBc;
        root["build"]["emit_obj"]    = build.emitObj;

        root["run"]["opt_level"] = run.optLevel;
        root["run"]["lto"]       = run.lto;
        root["run"]["quiet"]     = run.quiet;
        root["run"]["clean"]     = run.clean;
        for (const auto& a : run.args)
            root["run"]["args"].push_back(a);

        root["linker"]["rpath"] = linker.rpath;
        for (const auto& f : linker.flags)
            root["linker"]["flags"].push_back(f);
        for (const auto& l : linker.libs)
            root["linker"]["libs"].push_back(l);
        for (const auto& p : linker.libPaths)
            root["linker"]["lib_paths"].push_back(p);

        for (const auto& i : includes)
            root["includes"].push_back(i);

        std::ofstream ofs(yamlPath);
        if (!ofs) return false;
        ofs << "# lucis.yaml — Lucis project configuration\n";
        ofs << "# Generated by `lucis init`\n\n";
        ofs << root;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[lucis-config] error saving " << yamlPath
                  << ": " << e.what() << "\n";
        return false;
    }
}
