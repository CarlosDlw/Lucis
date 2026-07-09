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

static std::map<std::string, std::string>
toStringMap(const YAML::Node& node) {
    std::map<std::string, std::string> result;
    if (!node.IsDefined() || !node.IsMap()) return result;
    for (const auto& entry : node)
        result[entry.first.Scalar()] = entry.second.Scalar();
    return result;
}

// ── Legacy key detection ──────────────────────────────────────────

static bool hasLegacyKey(const YAML::Node& root, const std::string& key) {
    return root[key].IsDefined();
}

static void warnLegacy(const std::string& oldKey, const std::string& newKey) {
    std::cerr << "lucis: warning: lucis.yaml uses legacy key '" << oldKey
              << "', use '" << newKey << "'\n";
}

// ── Load ──────────────────────────────────────────────────────────

std::optional<LucisConfig> LucisConfig::load(const std::string& yamlPath) {
    if (!fs::exists(yamlPath)) return std::nullopt;

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
        cfg.name    = root["name"].Scalar();
        cfg.version = optOrDefault(root, "version", "0.0.1");
        cfg.binary  = optOrDefault(root, "binary", cfg.name);
        cfg.outDir  = optOrDefault(root, "out_dir", "build");

        // ── source paths ──────────────────────────────────────────
        cfg.sourcePaths = toStringVec(root["source"]);

        // ── tools ─────────────────────────────────────────────────
        {
            auto t = root["tools"];
            cfg.tools.nasm    = optOrDefault(t, "nasm", "");
            cfg.tools.ld      = optOrDefault(t, "ld", "");
            cfg.tools.objcopy = optOrDefault(t, "objcopy", "");
        }

        // ── assembly ──────────────────────────────────────────────
        {
            auto a = root["assembly"];
            // new: assembly.files
            cfg.assembly.files = toStringVec(a["files"]);
            // legacy: assembly is flat list
            if (cfg.assembly.files.empty() && hasLegacyKey(root, "assembly")
                && root["assembly"].IsSequence()) {
                cfg.assembly.files = toStringVec(root["assembly"]);
                warnLegacy("assembly: [files]", "assembly.files: [files]");
            }
            cfg.assembly.assembler = optOrDefault(a, "assembler",
                optOrDefault(root["build"], "assembler", ""));
            if (root["build"]["assembler"].IsDefined()
                && !a["assembler"].IsDefined())
                warnLegacy("build.assembler", "assembly.assembler");
            cfg.assembly.flags = toStringVec(a["flags"].IsDefined()
                ? a["flags"] : root["build"]["assembler_flags"]);
            if (root["build"]["assembler_flags"].IsDefined()
                && !a["flags"].IsDefined())
                warnLegacy("build.assembler_flags", "assembly.flags");
        }

        // ── build ─────────────────────────────────────────────────
        {
            auto b = root["build"];
            cfg.build.target      = optOrDefault(b, "target", "");
            cfg.build.optLevel    = optOrDefault(b, "opt_level", "O0");
            cfg.build.noStd       = boolOrDefault(b, "no_std", false);
            cfg.build.debug       = boolOrDefault(b, "debug", false);
            cfg.build.lto         = boolOrDefault(b, "lto", false);
            cfg.build.staticLink  = boolOrDefault(b, "static", false);
            cfg.build.shared      = boolOrDefault(b, "shared", false);
            cfg.build.fpic        = boolOrDefault(b, "fpic", true);
            cfg.build.codeModel   = optOrDefault(b, "code_model", "");

            // include_paths
            cfg.build.includePaths = toStringVec(b["include_paths"]);
            // legacy: top-level includes
            if (cfg.build.includePaths.empty() && hasLegacyKey(root, "includes")) {
                cfg.build.includePaths = toStringVec(root["includes"]);
                warnLegacy("includes", "build.include_paths");
            }

            cfg.build.defines = toStringMap(b["defines"]);

            // legacy build.entry → linker.entry (warn handled in linker)
        }

        // ── inputs ────────────────────────────────────────────────
        {
            auto i = root["inputs"];
            cfg.inputs.objects    = toStringVec(i["objects"].IsDefined()
                ? i["objects"] : root["objects"]);
            if (hasLegacyKey(root, "objects") && !i["objects"].IsDefined())
                warnLegacy("objects", "inputs.objects");
            cfg.inputs.staticLibs = toStringVec(i["static_libs"].IsDefined()
                ? i["static_libs"] : root["static_libs"]);
            if (hasLegacyKey(root, "static_libs") && !i["static_libs"].IsDefined())
                warnLegacy("static_libs", "inputs.static_libs");
            cfg.inputs.sharedLibs = toStringVec(i["shared_libs"].IsDefined()
                ? i["shared_libs"] : root["shared_libs"]);
            if (hasLegacyKey(root, "shared_libs") && !i["shared_libs"].IsDefined())
                warnLegacy("shared_libs", "inputs.shared_libs");
        }

        // ── linker ────────────────────────────────────────────────
        {
            auto l = root["linker"];
            cfg.linker.program  = optOrDefault(l, "program", "");
            cfg.linker.script   = optOrDefault(l, "script", "");
            cfg.linker.libs     = toStringVec(l["libs"]);
            cfg.linker.libPaths = toStringVec(l["lib_paths"]);
            cfg.linker.flags    = toStringVec(l["flags"]);
            cfg.linker.args     = toStringVec(l["args"]);

            cfg.linker.entry    = optOrDefault(l, "entry",
                optOrDefault(root["build"], "entry", ""));
            if (root["build"]["entry"].IsDefined() && !l["entry"].IsDefined()) {
                std::cerr << "lucis: warning: 'build.entry' is deprecated, "
                          << "use 'linker.entry'\n";
            }
        }

        // ── output / emit ─────────────────────────────────────────
        {
            auto o = root["output"];
            auto e = root["emit"];
            cfg.output.path   = optOrDefault(o, "path", "");
            cfg.output.strip  = o["strip"].IsDefined()
                ? boolOrDefault(o, "strip", false)
                : boolOrDefault(e, "bin", false); // compat
            cfg.output.emitBin = o["emit_bin"].IsDefined()
                ? boolOrDefault(o, "emit_bin", false)
                : boolOrDefault(e, "bin", false);
            cfg.output.emitLlvm = boolOrDefault(o, "emit_llvm",
                boolOrDefault(e, "llvm", false));
            cfg.output.emitAsm  = boolOrDefault(o, "emit_asm",
                boolOrDefault(e, "asm", false));
            cfg.output.emitBc   = boolOrDefault(o, "emit_bc",
                boolOrDefault(e, "bc", false));
            cfg.output.emitObj  = boolOrDefault(o, "emit_obj",
                boolOrDefault(e, "obj", false));

            if (hasLegacyKey(root, "emit") && !hasLegacyKey(root, "output"))
                std::cerr << "lucis: warning: 'emit:' section is deprecated, "
                          << "use 'output:'\n";
        }

        // ── scripts ───────────────────────────────────────────────
        {
            auto s = root["scripts"];
            cfg.scripts.env = toStringMap(s["env"]);
            cfg.scripts.pre = toStringVec(s["pre"]);
            cfg.scripts.pos = toStringVec(s["pos"]);
        }

        // ── run ───────────────────────────────────────────────────
        {
            auto r = root["run"];
            cfg.run.optLevel = optOrDefault(r, "opt_level", "O0");
            cfg.run.lto      = boolOrDefault(r, "lto", false);
            cfg.run.args     = toStringVec(r["args"]);
        }

        return cfg;
    } catch (const std::exception& e) {
        std::cerr << "[lucis-config] error loading " << yamlPath
                  << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

// ── Validation helpers ────────────────────────────────────────────

static bool yamlIsMap(const YAML::Node& n) {
    return n.IsDefined() && n.IsMap();
}

static bool yamlIsSeq(const YAML::Node& n) {
    return n.IsDefined() && n.IsSequence();
}

static bool yamlIsScalar(const YAML::Node& n) {
    return n.IsDefined() && n.IsScalar();
}

static bool yamlIsBool(const YAML::Node& n) {
    if (!n.IsDefined()) return true;
    try { n.as<bool>(); return true; } catch (...) { return false; }
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

static void checkType(const YAML::Node& node, const std::string& path,
                       const std::string& expected,
                       std::vector<LucisConfig::ValidationMsg>& out) {
    if (!node.IsDefined()) return;
    bool ok = false;
    if (expected == "scalar") ok = node.IsScalar();
    else if (expected == "seq") ok = node.IsSequence();
    else if (expected == "map") ok = node.IsMap();
    else if (expected == "bool") ok = yamlIsBool(node);
    if (!ok)
        out.push_back({path, "expected " + expected});
}

// ── Validate ──────────────────────────────────────────────────────

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

    if (!root["name"].IsDefined())
        msgs.push_back({"name", "missing required field"});
    else if (!root["name"].IsScalar())
        msgs.push_back({"name", "expected scalar"});

    for (auto& k : {"version", "binary", "out_dir"}) {
        if (root[k].IsDefined() && !root[k].IsScalar())
            msgs.push_back({k, "expected scalar"});
    }

    if (root["source"].IsDefined() && !root["source"].IsSequence())
        msgs.push_back({"source", "expected sequence"});

    // tools
    if (yamlIsMap(root["tools"])) {
        static const std::vector<std::string> toolsKeys = {
            "nasm", "ld", "objcopy"
        };
        checkUnknownKeys(root["tools"], "tools", toolsKeys, msgs);
        for (auto& k : toolsKeys)
            checkType(root["tools"][k], std::string("tools.") + k, "scalar", msgs);
    }

    // assembly
    if (yamlIsMap(root["assembly"])) {
        static const std::vector<std::string> asmKeys = {
            "files", "assembler", "flags"
        };
        checkUnknownKeys(root["assembly"], "assembly", asmKeys, msgs);
        checkType(root["assembly"]["files"], "assembly.files", "seq", msgs);
        checkType(root["assembly"]["assembler"], "assembly.assembler", "scalar", msgs);
        checkType(root["assembly"]["flags"], "assembly.flags", "seq", msgs);
        // validate assembler value
        if (yamlIsScalar(root["assembly"]["assembler"])) {
            auto val = root["assembly"]["assembler"].Scalar();
            if (!val.empty() && val != "nasm" && val != "as")
                msgs.push_back({"assembly.assembler",
                    "expected 'nasm' or 'as', got '" + val + "'"});
        }
    }

    // build
    if (yamlIsMap(root["build"])) {
        static const std::vector<std::string> buildKeys = {
            "target", "opt_level", "no_std", "lto", "static", "shared",
            "fpic", "code_model", "include_paths", "defines",
            // legacy (accepted with warning)
            "entry", "assembler", "assembler_flags"
        };
        checkUnknownKeys(root["build"], "build", buildKeys, msgs);
        checkType(root["build"]["opt_level"], "build.opt_level", "scalar", msgs);
        checkType(root["build"]["target"], "build.target", "scalar", msgs);
        for (auto& k : {"no_std", "lto", "static", "shared", "fpic"})
            checkType(root["build"][k], std::string("build.") + k, "bool", msgs);
        checkType(root["build"]["include_paths"], "build.include_paths", "seq", msgs);
        checkType(root["build"]["defines"], "build.defines", "map", msgs);

        // legacy warnings
        if (root["build"]["entry"].IsDefined())
            msgs.push_back({"build.entry", "deprecated, use 'linker.entry'"});
        if (root["build"]["assembler"].IsDefined())
            msgs.push_back({"build.assembler", "deprecated, use 'assembly.assembler'"});

        // conflict check
        if (boolOrDefault(root["build"], "static", false)
            && boolOrDefault(root["build"], "shared", false))
            msgs.push_back({"build", "'static' and 'shared' are mutually exclusive"});
    }

    // inputs
    if (yamlIsMap(root["inputs"])) {
        static const std::vector<std::string> inputsKeys = {
            "objects", "static_libs", "shared_libs"
        };
        checkUnknownKeys(root["inputs"], "inputs", inputsKeys, msgs);
        checkType(root["inputs"]["objects"], "inputs.objects", "seq", msgs);
        checkType(root["inputs"]["static_libs"], "inputs.static_libs", "seq", msgs);
        checkType(root["inputs"]["shared_libs"], "inputs.shared_libs", "seq", msgs);
    }

    // linker
    if (yamlIsMap(root["linker"])) {
        static const std::vector<std::string> linkerKeys = {
            "program", "script", "entry", "libs", "lib_paths", "flags", "args"
        };
        checkUnknownKeys(root["linker"], "linker", linkerKeys, msgs);
        for (auto& k : {"program", "script", "entry"})
            checkType(root["linker"][k], std::string("linker.") + k, "scalar", msgs);
        for (auto& k : {"libs", "lib_paths", "flags", "args"})
            checkType(root["linker"][k], std::string("linker.") + k, "seq", msgs);
    }

    // output
    if (yamlIsMap(root["output"])) {
        static const std::vector<std::string> outputKeys = {
            "path", "strip", "emit_bin", "emit_llvm", "emit_asm",
            "emit_bc", "emit_obj"
        };
        checkUnknownKeys(root["output"], "output", outputKeys, msgs);
        checkType(root["output"]["path"], "output.path", "scalar", msgs);
        for (auto& k : {"strip", "emit_bin", "emit_llvm", "emit_asm", "emit_bc", "emit_obj"})
            checkType(root["output"][k], std::string("output.") + k, "bool", msgs);
    }

    // scripts
    if (yamlIsMap(root["scripts"])) {
        static const std::vector<std::string> scriptsKeys = {
            "env", "pre", "pos"
        };
        checkUnknownKeys(root["scripts"], "scripts", scriptsKeys, msgs);
        checkType(root["scripts"]["env"], "scripts.env", "map", msgs);
        checkType(root["scripts"]["pre"], "scripts.pre", "seq", msgs);
        checkType(root["scripts"]["pos"], "scripts.pos", "seq", msgs);
    }

    // run
    if (yamlIsMap(root["run"])) {
        static const std::vector<std::string> runKeys = {
            "opt_level", "lto", "args"
        };
        checkUnknownKeys(root["run"], "run", runKeys, msgs);
        checkType(root["run"]["opt_level"], "run.opt_level", "scalar", msgs);
        checkType(root["run"]["lto"], "run.lto", "bool", msgs);
        checkType(root["run"]["args"], "run.args", "seq", msgs);
    }

    static const std::vector<std::string> topKeys = {
        "name", "version", "binary", "out_dir",
        "tools", "assembly", "source", "build", "inputs",
        "linker", "output", "scripts", "run",
        // legacy top-level keys (accepted with warning)
        "emit", "assembly", "objects", "static_libs", "shared_libs", "includes"
    };
    checkUnknownKeys(root, "", topKeys, msgs);

    return msgs;
}

// ── find / create default ─────────────────────────────────────────

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
    cfg.binary      = name;
    cfg.outDir      = "build";
    cfg.sourcePaths = {"src/"};
    cfg.build.optLevel   = "O2";
    cfg.build.fpic       = true;
    cfg.run.optLevel     = "O0";

    // Set all other fields to their defaults so they appear in the saved YAML
    cfg.tools.nasm    = "";
    cfg.tools.ld      = "";
    cfg.tools.objcopy = "";
    cfg.assembly.files.clear();
    cfg.assembly.assembler = "";
    cfg.assembly.flags.clear();
    cfg.build.target      = "";
    cfg.build.noStd       = false;
    cfg.build.lto         = false;
    cfg.build.staticLink  = false;
    cfg.build.shared      = false;
    cfg.build.codeModel   = "";
    cfg.build.includePaths.clear();
    cfg.build.defines.clear();
    cfg.inputs.objects.clear();
    cfg.inputs.staticLibs.clear();
    cfg.inputs.sharedLibs.clear();
    cfg.linker.program  = "";
    cfg.linker.script   = "";
    cfg.linker.entry    = "";
    cfg.linker.libs.clear();
    cfg.linker.libPaths.clear();
    cfg.linker.flags.clear();
    cfg.linker.args.clear();
    cfg.output.path      = "";
    cfg.output.strip     = false;
    cfg.output.emitBin   = false;
    cfg.output.emitLlvm  = false;
    cfg.output.emitAsm   = false;
    cfg.output.emitBc    = false;
    cfg.output.emitObj   = false;
    cfg.scripts.env.clear();
    cfg.scripts.pre.clear();
    cfg.scripts.pos.clear();
    cfg.run.lto      = false;
    cfg.run.args.clear();

    return cfg.save((fs::path(dir) / "lucis.yaml").string());
}

// ── Save ──────────────────────────────────────────────────────────

bool LucisConfig::save(const std::string& yamlPath) const {
    try {
        YAML::Node root;

        root["name"]    = name;
        root["version"] = version;
        root["binary"]  = binary;
        root["out_dir"] = outDir;

        // source
        for (const auto& s : sourcePaths)
            root["source"].push_back(s);

        // tools
        auto writeScalar = [&](YAML::Node parent, const std::string& key,
                                const std::string& val) {
            parent[key] = val;
        };
        writeScalar(root["tools"], "nasm", tools.nasm);
        writeScalar(root["tools"], "ld", tools.ld);
        writeScalar(root["tools"], "objcopy", tools.objcopy);

        // assembly
        auto writeSeq = [&](YAML::Node parent, const std::string& key,
                             const std::vector<std::string>& vals) {
            for (const auto& v : vals)
                parent[key].push_back(v);
            // Ensure key exists even if empty
            if (vals.empty() && !parent[key].IsDefined())
                parent[key] = YAML::Node(YAML::NodeType::Sequence);
        };
        writeSeq(root["assembly"], "files", assembly.files);
        writeScalar(root["assembly"], "assembler", assembly.assembler);
        writeSeq(root["assembly"], "flags", assembly.flags);

        // build
        root["build"]["target"]    = build.target;
        root["build"]["opt_level"] = build.optLevel;
        root["build"]["no_std"]    = build.noStd;
        root["build"]["lto"]       = build.lto;
        root["build"]["static"]    = build.staticLink;
        root["build"]["shared"]    = build.shared;
        root["build"]["fpic"]      = build.fpic;
        root["build"]["code_model"]= build.codeModel;
        writeSeq(root["build"], "include_paths", build.includePaths);
        if (!build.defines.empty()) {
            for (const auto& [k, v] : build.defines)
                root["build"]["defines"][k] = v;
        } else {
            root["build"]["defines"] = YAML::Node(YAML::NodeType::Map);
        }

        // inputs
        writeSeq(root["inputs"], "objects", inputs.objects);
        writeSeq(root["inputs"], "static_libs", inputs.staticLibs);
        writeSeq(root["inputs"], "shared_libs", inputs.sharedLibs);

        // linker
        writeScalar(root["linker"], "program", linker.program);
        writeScalar(root["linker"], "script",  linker.script);
        writeScalar(root["linker"], "entry",   linker.entry);
        writeSeq(root["linker"], "libs", linker.libs);
        writeSeq(root["linker"], "lib_paths", linker.libPaths);
        writeSeq(root["linker"], "flags", linker.flags);
        writeSeq(root["linker"], "args", linker.args);

        // output
        root["output"]["path"]      = output.path;
        root["output"]["strip"]     = output.strip;
        root["output"]["emit_bin"]  = output.emitBin;
        root["output"]["emit_llvm"] = output.emitLlvm;
        root["output"]["emit_asm"]  = output.emitAsm;
        root["output"]["emit_bc"]   = output.emitBc;
        root["output"]["emit_obj"]  = output.emitObj;

        // scripts
        if (!scripts.env.empty()) {
            for (const auto& [k, v] : scripts.env)
                root["scripts"]["env"][k] = v;
        } else {
            root["scripts"]["env"] = YAML::Node(YAML::NodeType::Map);
        }
        writeSeq(root["scripts"], "pre", scripts.pre);
        writeSeq(root["scripts"], "pos", scripts.pos);

        // run
        root["run"]["opt_level"] = run.optLevel;
        root["run"]["lto"]       = run.lto;
        writeSeq(root["run"], "args", run.args);

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
