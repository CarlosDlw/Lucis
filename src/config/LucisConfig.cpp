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

    // Run schema validation
    auto valMsgs = validate(yamlPath);
    bool hasError = false;
    for (auto& v : valMsgs) {
        if (v.message.rfind("missing required", 0) == 0) hasError = true;
        std::string loc = v.path.empty() ? "" : v.path + ": ";
        std::cerr << "[lucis-config] " << loc << v.message << "\n";
    }
    if (hasError) return std::nullopt;

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

            // Parse structured emits
            auto emitsNode = b["emits"];
            if (emitsNode.IsDefined() && emitsNode.IsMap()) {
                for (const auto& entry : emitsNode) {
                    auto key = entry.first.Scalar();
                    auto val = entry.second;
                    LucisConfig::EmitConfig ec;
                    ec.enabled = boolOrDefault(val, "enabled", false);
                    ec.file    = optOrDefault(val, "file", "");
                    cfg.build.emits[key] = ec;
                }
            }
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

// ── Schema validation ────────────────────────────────────────────────

static bool yamlIsMap(const YAML::Node& n) {
    return n.IsDefined() && n.IsMap();
}

static void checkUnknownKeys(const YAML::Node& node,
                             const std::string& parentPath,
                             const std::vector<std::string>& known,
                             std::vector<LucisConfig::ValidationMsg>& out) {
    if (!yamlIsMap(node)) return;
    for (const auto& entry : node) {
        auto key = entry.first.Scalar();
        bool found = false;
        for (auto& k : known) {
            if (k == key) { found = true; break; }
        }
        if (!found) {
            std::string fullPath = parentPath.empty() ? key : parentPath + "." + key;
            out.push_back({fullPath, "unknown field"});
        }
    }
}

std::vector<LucisConfig::ValidationMsg>
LucisConfig::validate(const std::string& yamlPath) {
    std::vector<ValidationMsg> msgs;
    if (!fs::exists(yamlPath)) {
        msgs.push_back({"", "file not found: " + yamlPath});
        return msgs;
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(yamlPath);
    } catch (const std::exception& e) {
        msgs.push_back({"", "YAML parse error: " + std::string(e.what())});
        return msgs;
    }

    if (!yamlIsMap(root)) {
        msgs.push_back({"", "root must be a mapping"});
        return msgs;
    }

    // Required fields
    if (!root["name"].IsDefined())
        msgs.push_back({"name", "missing required field"});
    else if (!root["name"].IsScalar())
        msgs.push_back({"name", "expected scalar, got " + std::string(root["name"].IsSequence() ? "sequence" : root["name"].IsMap() ? "map" : "null")});

    // Optional scalar fields
    static const std::vector<std::string> optScalars = {
        "version", "description", "binary", "out_dir"
    };
    for (auto& k : optScalars) {
        if (root[k].IsDefined() && !root[k].IsScalar())
            msgs.push_back({k, "expected scalar, got " + std::string(root[k].IsSequence() ? "sequence" : root[k].IsMap() ? "map" : "null")});
    }

    // Optional sequence fields
    static const std::vector<std::string> optSeqs = {
        "source", "exclude", "includes"
    };
    for (auto& k : optSeqs) {
        if (root[k].IsDefined() && !root[k].IsSequence())
            msgs.push_back({k, "expected sequence, got " + std::string(root[k].IsScalar() ? "scalar" : root[k].IsMap() ? "map" : "null")});
    }

    // Sub-map validations
    if (yamlIsMap(root["build"])) {
        if (root["build"]["opt_level"].IsDefined() && !root["build"]["opt_level"].IsScalar())
            msgs.push_back({"build.opt_level", "expected scalar, got " + std::string(root["build"]["opt_level"].IsSequence() ? "sequence" : root["build"]["opt_level"].IsMap() ? "map" : "null")});
        if (root["build"]["emits"].IsDefined() && !root["build"]["emits"].IsMap())
            msgs.push_back({"build.emits", "expected map, got " + std::string(root["build"]["emits"].IsSequence() ? "sequence" : root["build"]["emits"].IsScalar() ? "scalar" : "null")});
        // Check build sub-keys for unknown
        static const std::vector<std::string> buildKeys = {
            "opt_level", "lto", "static", "shared", "fpic", "quiet", "emits"
        };
        checkUnknownKeys(root["build"], "build", buildKeys, msgs);
    }

    if (yamlIsMap(root["run"])) {
        static const std::vector<std::string> runKeys = {
            "opt_level", "lto", "quiet", "clean", "args"
        };
        checkUnknownKeys(root["run"], "run", runKeys, msgs);
    }

    if (yamlIsMap(root["linker"])) {
        static const std::vector<std::string> linkerKeys = {
            "flags", "rpath", "libs", "lib_paths"
        };
        checkUnknownKeys(root["linker"], "linker", linkerKeys, msgs);
    }

    // Unknown top-level keys
    static const std::vector<std::string> topKeys = {
        "name", "version", "description", "binary", "out_dir",
        "source", "exclude", "build", "run", "linker", "includes"
    };
    checkUnknownKeys(root, "", topKeys, msgs);

    return msgs;
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
    cfg.excludePatterns = {};
    cfg.includes        = {};

    cfg.build.optLevel   = "O2";
    cfg.build.lto        = false;
    cfg.build.staticLink = false;
    cfg.build.shared     = false;
    cfg.build.fpic       = true;
    cfg.build.quiet      = false;

    // All emit types with defaults
    cfg.build.emits["llvm"] = {false, ""};
    cfg.build.emits["asm"]  = {false, ""};
    cfg.build.emits["bc"]   = {false, ""};
    cfg.build.emits["obj"]  = {false, ""};

    cfg.run.optLevel = "O0";
    cfg.run.lto      = false;
    cfg.run.quiet    = false;
    cfg.run.clean    = false;
    cfg.run.args     = {};

    cfg.linker.flags    = {};
    cfg.linker.rpath    = "";
    cfg.linker.libs     = {};
    cfg.linker.libPaths = {};

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

        // Always write exclude (emit empty array if none)
        if (excludePatterns.empty()) {
            root["exclude"] = YAML::Node(YAML::NodeType::Sequence);
        } else {
            for (const auto& e : excludePatterns)
                root["exclude"].push_back(e);
        }

        root["build"]["opt_level"]   = build.optLevel;
        root["build"]["lto"]         = build.lto;
        root["build"]["static"]      = build.staticLink;
        root["build"]["shared"]      = build.shared;
        root["build"]["fpic"]        = build.fpic;
        root["build"]["quiet"]       = build.quiet;

        // Save structured emits (always write all four types)
        for (const auto& [key, ec] : build.emits) {
            root["build"]["emits"][key]["enabled"] = ec.enabled;
            root["build"]["emits"][key]["file"]    = ec.file;
        }

        root["run"]["opt_level"] = run.optLevel;
        root["run"]["lto"]       = run.lto;
        root["run"]["quiet"]     = run.quiet;
        root["run"]["clean"]     = run.clean;

        // Always write run.args
        if (run.args.empty()) {
            root["run"]["args"] = YAML::Node(YAML::NodeType::Sequence);
        } else {
            for (const auto& a : run.args)
                root["run"]["args"].push_back(a);
        }

        root["linker"]["rpath"] = linker.rpath;

        // Always write linker arrays
        auto writeSeq = [&](YAML::Node parent, const std::string& key,
                             const std::vector<std::string>& vals) {
            if (vals.empty()) {
                parent[key] = YAML::Node(YAML::NodeType::Sequence);
            } else {
                for (const auto& v : vals)
                    parent[key].push_back(v);
            }
        };
        writeSeq(root["linker"], "flags", linker.flags);
        writeSeq(root["linker"], "libs", linker.libs);
        writeSeq(root["linker"], "lib_paths", linker.libPaths);

        // Always write includes
        if (includes.empty()) {
            root["includes"] = YAML::Node(YAML::NodeType::Sequence);
        } else {
            for (const auto& i : includes)
                root["includes"].push_back(i);
        }

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
