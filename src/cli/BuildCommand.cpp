#include "cli/BuildCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
#include "cli/LucisPipeline.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"
#include "comptime/ComptimeEngine.h"

#include "generated/LucisParser.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>

namespace fs = std::filesystem;

void BuildCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file (auto-resolved from lucis.yaml if omitted)", false);

    parser.addSection("Compilation");
    parser.addOption("target", '\0', "TRIPLE", "Target triple (default: host)");
    parser.addFlag("no-std", '\0', "Build without standard library (freestanding/kernel)");
    parser.addFlag("debug", 'g', "Emit debug info (DWARF)");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, fast (default: 0)");
    parser.addFlag("lto", '\0', "Enable Link Time Optimization");
    parser.addFlag("fPIC",   '\0', "Generate position-independent code");
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);

    parser.addSection("Assembly");
    parser.addOption("asm", '\0', "FILE", "Assembly source file (.s/.asm) (repeatable)", true);
    parser.addOption("asm-syntax", '\0', "att|intel", "Assembly syntax from LLVM (default: att)");
    parser.addOption("assembler", '\0', "nasm|as", "Assembler program (default: try nasm, fallback as)");
    parser.addOption("assembler-arg", '\0', "FLAG", "Flag for the assembler (repeatable)", true);

    parser.addSection("Direct inputs");
    parser.addOption("obj", '\0', "FILE", "Pre-compiled object file .o (repeatable)", true);
    parser.addOption("lib", '\0', "FILE", "Library file .a/.so (repeatable)", true);

    parser.addSection("Link");
    parser.addOption("linker", '\0', "PATH", "Linker program (default: gcc/clang for host, ld for freestanding)");
    parser.addOption("linker-script", '\0', "FILE", "Linker script (-T)");
    parser.addOption("linker-entry", '\0', "SYMBOL", "Entry point symbol (-e)");
    parser.addFlag("linker-nmagic", '\0', "Suppress page alignment in linker (-n)");
    parser.addFlag("linker-omagic", '\0', "Set text segment writable (-N)");
    parser.addFlag("linker-gc-sections", '\0', "Garbage collect unused sections at link time");
    parser.addOption("linker-arg", '\0', "FLAG", "Pass raw argument to linker (repeatable)", true);
    parser.addOption("rpath",    '\0', "DIR", "Add runtime library search path", false);
    parser.addOption("link", 'l', "LIB", "Link against a library (repeatable)", true);
    parser.addOption("lib-path", 'L', "DIR", "Add library search path (repeatable)", true);
    parser.addFlag("static", '\0', "Produce a statically linked executable");
    parser.addFlag("shared", '\0', "Produce a shared library");

    parser.addSection("Post-process");
    parser.addFlag("strip", '\0', "Strip debug and symbol info from the output binary");
    parser.addFlag("emit-llvm", '\0', "Emit LLVM IR (.ll)");
    parser.addFlag("emit-asm",  '\0', "Emit assembly (.s) from LLVM");
    parser.addFlag("emit-bc",   '\0', "Emit LLVM bitcode (.bc)");
    parser.addFlag("emit-obj",  '\0', "Emit object file (.o) and stop");
    parser.addFlag("emit-bin",  '\0', "Emit raw binary (.bin) via objcopy");
    parser.addFlag("recursive", 'r', "Include all modules in emit output");
    parser.addFlag("split", '\0', "Split emit output into one file per module (requires -o <dir>)");

    parser.addSection("Output");
    parser.addOption("output", 'o', "FILE", "Output path (default: <input>.out)");

    parser.addSection("General");
    parser.addFlag("ignore-config", '\0', "Ignore lucis.yaml, use CLI flags only");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addOption("config", '\0', "FILE", "Path to lucis.yaml configuration");

    // Deprecated flags (still accepted for backward compat, hidden from help)
    parser.addOption("entry", '\0', "SYMBOL", "");
    parser.addFlag("nmagic", '\0', "");
    parser.addFlag("omagic", '\0', "");
    parser.addOption("link-arg", '\0', "FLAG", "", true);
    parser.addFlag("gc-sections", '\0', "");
}

int BuildCommand::run(const ArgParser& parser) {
    auto resolved = resolveInputFile(parser.get("file"), parser.get("config"),
                                     parser.has("ignore-config"));
    if (resolved.filePath.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis build <file>   or   lucis build  (from a project with lucis.yaml)\n";
        return 1;
    }

    bool useConfig = resolved.useConfig;
    auto& cfg = resolved.config;

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = resolved.filePath;
    pipeOpts.quiet     = parser.has("quiet");
    if (useConfig)
        pipeOpts.includePaths = cfg->build.includePaths;
    auto cliIncludes = parser.getAll("include");
    pipeOpts.includePaths.insert(pipeOpts.includePaths.end(), cliIncludes.begin(), cliIncludes.end());

    pipeOpts.sourcePaths = useConfig ? cfg->sourcePaths : std::vector<std::string>{"src/"};

    // ── Target / no-std / entry ─────────────────────────────────────
    pipeOpts.noStd = parser.has("no-std") ? true : (useConfig ? cfg->build.noStd : false);
    pipeOpts.emitDebugInfo = parser.has("debug") ? true : (useConfig ? cfg->build.debug : false);
    pipeOpts.targetTriple = parser.get("target");
    if (pipeOpts.targetTriple.empty() && useConfig)
        pipeOpts.targetTriple = cfg->build.target;
    pipeOpts.codeModel = useConfig ? cfg->build.codeModel : "";

    // Linker entry: CLI (--linker-entry / --entry) > config.linker.entry
    std::string linkerEntry = parser.has("linker-entry") ? parser.get("linker-entry") :
                              parser.get("entry");
    if (linkerEntry.empty() && useConfig && !cfg->linker.entry.empty())
        linkerEntry = cfg->linker.entry;
    if (parser.has("entry") && !parser.has("linker-entry"))
        std::cerr << "lucis: warning: --entry is deprecated, use --linker-entry\n";
    pipeOpts.entryPoint = linkerEntry;

    pipeOpts.userLinkerFlags = parser.getAll("link");
    if (useConfig && pipeOpts.userLinkerFlags.empty())
        pipeOpts.userLinkerFlags = cfg->linker.libs;

    pipeOpts.binaryName = useConfig ? cfg->binary : "";
    pipeOpts.outDir     = useConfig ? cfg->outDir : "";

    // ── Compute project paths for scripts and env vars ───────────
    std::string projRoot = LucisPipeline::getProjectRoot(pipeOpts.inputFile);
    auto setScriptEnv = [&](const std::string& outputPath) {
        ::setenv("LUCIS_PROJECT_ROOT", projRoot.c_str(), 1);
        ::setenv("LUCIS_BUILD_DIR", (projRoot + "/.lucis/build").c_str(), 1);
        ::setenv("LUCIS_TARGET", pipeOpts.targetTriple.c_str(), 1);
        ::setenv("LUCIS_OUTPUT", outputPath.c_str(), 1);
    };

    // ── Export script env vars from config ────────────────────────
    if (useConfig) {
        for (auto& [k, v] : cfg->scripts.env)
            ::setenv(k.c_str(), v.c_str(), 1);
    }

    // ── Run pre-build scripts (before everything) ─────────────────
    if (useConfig && !cfg->scripts.pre.empty()) {
        setScriptEnv(pipeOpts.binaryName.empty() ? pipeOpts.inputFile : pipeOpts.binaryName);
        for (auto& cmd : cfg->scripts.pre) {
            std::cerr << "lucis: [scripts:pre] " << cmd << "\n";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "lucis: [scripts:pre] command failed with exit code " << ret << "\n";
                return 1;
            }
        }
    }

    OptimizationLevel optLevel = OptimizationLevel::O0;
    if (parser.has("opt"))
        optLevel = parseOptimizationLevel(parser.get("opt"));
    else if (useConfig)
        optLevel = parseOptimizationLevel(cfg->build.optLevel);

    bool useLTO    = parser.has("lto")    ? true : (useConfig ? cfg->build.lto        : false);
    bool useStatic = parser.has("static") ? true : (useConfig ? cfg->build.staticLink : false);
    bool useShared = parser.has("shared") ? true : (useConfig ? cfg->build.shared     : false);

    bool usePIC;
    if (parser.has("fPIC"))
        usePIC = true;
    else if (useShared)
        usePIC = true;
    else if (useStatic)
        usePIC = false;
    else {
        auto tripleEndsWith = [](const std::string& s, const std::string& suffix) -> bool {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        bool isBareMetal = pipeOpts.noStd &&
            (tripleEndsWith(pipeOpts.targetTriple, "-none") ||
             tripleEndsWith(pipeOpts.targetTriple, "-eabi"));
        usePIC = isBareMetal ? false : (useConfig ? cfg->build.fpic : true);
    }

    bool useRecursive = parser.has("recursive");
    bool useSplit = parser.has("split");
    std::string asmSyntax = parser.get("asm-syntax");
    if (!asmSyntax.empty() && asmSyntax != "att" && asmSyntax != "intel") {
        std::cerr << "lucis: --asm-syntax must be 'att' or 'intel', got '" << asmSyntax << "'\n";
        return 1;
    }
    std::string outputFile = parser.get("output");

    // ── Split validation ─────────────────────────────────────────────────
    if (useSplit) {
        if (outputFile.empty()) {
            std::cerr << "lucis: --split requires -o/--output <directory>\n";
            return 1;
        }
        if (fs::exists(outputFile) && !fs::is_directory(outputFile)) {
            std::cerr << "lucis: --split requires -o to be a directory, but '"
                      << outputFile << "' is a file\n";
            return 1;
        }
        if (fs::exists(outputFile)) {
            bool empty = true;
            for (auto it = fs::directory_iterator(outputFile);
                 it != fs::directory_iterator(); ++it) {
                empty = false;
                break;
            }
            if (!empty) {
                std::cerr << "lucis: --split requires an empty or non-existent directory, '"
                          << outputFile << "' is not empty\n";
                return 1;
            }
        }
    }

    // ── Resolve emit tasks (CLI + config) ─────────────────────────────────
    struct EmitTask {
        enum Type { LLVM, ASM, BC, OBJ, BIN };
        Type type;
        std::string outPath;
    };
    std::vector<EmitTask> tasks;
    bool hasObjEmit = false;
    bool hasTextEmit = false;

    auto addEmitTask = [&](const std::string& key, EmitTask::Type etype) {
        EmitTask t;
        t.type = etype;
        if (!outputFile.empty())
            t.outPath = outputFile;
        if (etype == EmitTask::OBJ) hasObjEmit = true;
        else if (etype != EmitTask::BIN) hasTextEmit = true;
        tasks.push_back(t);
    };

    bool hasAnyCliEmit = parser.has("emit-llvm") || parser.has("emit-asm") ||
                         parser.has("emit-bc") || parser.has("emit-obj") ||
                         parser.has("emit-bin");
    if (hasAnyCliEmit) {
        auto addIfSet = [&](const std::string& key, EmitTask::Type etype) {
            if (parser.has("emit-" + key)) addEmitTask(key, etype);
        };
        addIfSet("llvm", EmitTask::LLVM);
        addIfSet("asm",  EmitTask::ASM);
        addIfSet("bc",   EmitTask::BC);
        addIfSet("obj",  EmitTask::OBJ);
        addIfSet("bin",  EmitTask::BIN);
    } else if (useConfig) {
        if (cfg->output.emitLlvm) addEmitTask("llvm", EmitTask::LLVM);
        if (cfg->output.emitAsm)  addEmitTask("asm",  EmitTask::ASM);
        if (cfg->output.emitBc)   addEmitTask("bc",   EmitTask::BC);
        if (cfg->output.emitObj)  addEmitTask("obj",  EmitTask::OBJ);
        if (cfg->output.emitBin)  addEmitTask("bin",  EmitTask::BIN);
    }

    for (size_t i = 0; i < tasks.size(); ++i) {
        for (size_t j = i + 1; j < tasks.size(); ++j) {
            if (tasks[i].outPath.empty() || tasks[j].outPath.empty()) continue;
            if (tasks[i].outPath == tasks[j].outPath) {
                std::cerr << "lucis: emit path conflict — '" << tasks[i].outPath
                          << "' is used by multiple emit targets\n";
                return 1;
            }
        }
    }

    // ── Parse extra inputs (assembly, object files, etc.) ────────────────
    std::vector<std::string> assemblySources;
    std::vector<std::string> extraObjectFiles;

    // From config (lucis.yaml) — lowest priority
    if (useConfig) {
        for (auto& f : cfg->assembly.files)
            assemblySources.push_back(f);
        for (auto& f : cfg->inputs.objects)
            extraObjectFiles.push_back(f);
        for (auto& f : cfg->inputs.staticLibs)
            extraObjectFiles.push_back(f);
        for (auto& f : cfg->inputs.sharedLibs)
            extraObjectFiles.push_back(f);
    }

    // From --asm flag (overrides config)
    for (auto& asmFile : parser.getAll("asm"))
        assemblySources.push_back(fs::canonical(asmFile).string());

    // From --obj flag (overrides config)
    for (auto& objFile : parser.getAll("obj"))
        extraObjectFiles.push_back(fs::canonical(objFile).string());

    // From --lib flag (overrides config)
    for (auto& libFile : parser.getAll("lib"))
        extraObjectFiles.push_back(fs::canonical(libFile).string());

    // From remaining positional args (legacy, lowest priority)
    for (auto& arg : parser.remaining()) {
        auto ext = fs::path(arg).extension();
        if (ext == ".o" || ext == ".a" || ext == ".so") {
            extraObjectFiles.push_back(fs::canonical(arg).string());
        } else if (ext == ".s" || ext == ".asm" || ext == ".S") {
            assemblySources.push_back(fs::canonical(arg).string());
        } else if (ext == ".lc") {
            // skip — main entry file, already handled
        } else {
            std::cerr << "lucis: unexpected argument '" << arg << "'\n";
            return 1;
        }
    }

    // Assembler selection (CLI > config.assembly.assembler > config.tools.nasm)
    std::string assemblerChoice;
    if (parser.has("assembler"))
        assemblerChoice = parser.get("assembler");
    else if (useConfig && !cfg->assembly.assembler.empty())
        assemblerChoice = cfg->assembly.assembler;
    else if (useConfig && !cfg->tools.nasm.empty())
        assemblerChoice = cfg->tools.nasm;
    std::vector<std::string> assemblerFlags = parser.has("assembler-arg") ?
        parser.getAll("assembler-arg") : (useConfig ? cfg->assembly.flags :
                                          std::vector<std::string>{});

    // ── Build flag hash for cache invalidation ──────────────────────────
    std::string customLinker = parser.has("linker") ? parser.get("linker") :
                                (useConfig && !cfg->linker.program.empty() ? cfg->linker.program :
                                 (useConfig && !cfg->tools.ld.empty() ? cfg->tools.ld : ""));
    bool isRawLd = !customLinker.empty() &&
                   (customLinker == "ld" ||
                    customLinker.find("/ld") != std::string::npos);

    auto libPaths = useConfig ? cfg->linker.libPaths : std::vector<std::string>{};
    auto cliLibPaths = parser.getAll("lib-path");
    libPaths.insert(libPaths.end(), cliLibPaths.begin(), cliLibPaths.end());

    auto buildFlagHash = [&]() -> std::string {
        std::string buf;
        buf += "opt:" + parser.get("opt") + ";";
        buf += "target:" + pipeOpts.targetTriple + ";";
        buf += "noStd:" + std::to_string(pipeOpts.noStd) + ";";
        buf += "static:" + std::to_string(useStatic) + ";";
        buf += "shared:" + std::to_string(useShared) + ";";
        buf += "PIC:" + std::to_string(usePIC) + ";";
        buf += "LTO:" + std::to_string(useLTO) + ";";
        buf += "entry:" + pipeOpts.entryPoint + ";";
        buf += "linkerNmagic:" + std::to_string(parser.has("linker-nmagic") || parser.has("nmagic")) + ";";
        buf += "linkerOmagic:" + std::to_string(parser.has("linker-omagic") || parser.has("omagic")) + ";";
        buf += "ls:" + parser.get("linker-script") + ";";
        if (useConfig && parser.get("linker-script").empty())
            buf += "ls_cfg:" + cfg->linker.script + ";";
        buf += "linker:" + customLinker + ";";
        buf += "strip:" + std::to_string(parser.has("strip") || (useConfig && cfg->output.strip)) + ";";
        buf += "linkerGcSections:" + std::to_string(parser.has("linker-gc-sections") || parser.has("gc-sections")) + ";";
        for (auto& ip : pipeOpts.includePaths) buf += "I:" + ip + ";";
        for (auto& lp : libPaths) buf += "L:" + lp + ";";
        for (auto& eo : extraObjectFiles) buf += "O:" + eo + ";";
        if (useConfig) {
            for (auto& [k, v] : cfg->build.defines) buf += "D:" + k + "=" + v + ";";
            for (auto& flag : cfg->linker.flags) buf += "LF:" + flag + ";";
            for (auto& arg : cfg->linker.args) buf += "LA:" + arg + ";";
        }
        return std::to_string(std::hash<std::string>{}(buf));
    };

    std::string currentFlagHash = buildFlagHash();

    // ── Incremental build cache ──────────────────────────────────────────
    bool buildCached = false;
    std::string pipelineBuildDir = projRoot + "/.lucis/build";
    std::string cacheManifestPath = pipelineBuildDir + "/cache/build_manifest.txt";

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [build] checking incremental cache\n";

    std::vector<std::string> savedLinkerFlags;
    std::vector<std::string> savedObjectFiles;
    std::string savedFlagHash;
    {
        std::ifstream manifest(cacheManifestPath);
        if (manifest) {
            std::string line;
            bool allMatch = true;
            while (std::getline(manifest, line)) {
                if (line.empty()) continue;
                auto colonPos = line.find(':');
                if (colonPos != std::string::npos && colonPos > 0 &&
                    (colonPos + 1 >= line.size() || line[colonPos + 1] != ':')) {
                    // Prefix line: "C:file.c mtime", "ASM:file.s mtime"
                    auto content = line.substr(colonPos + 1);
                    auto sep = content.find(' ');
                    if (sep == std::string::npos) { allMatch = false; break; }
                    auto filePath = content.substr(0, sep);
                    auto savedMtime = std::stoll(content.substr(sep + 1));
                    std::error_code ec;
                    auto currentMtime = fs::last_write_time(filePath, ec);
                    if (ec || std::chrono::duration_cast<std::chrono::seconds>(
                            currentMtime.time_since_epoch()).count() != savedMtime) {
                        allMatch = false;
                        break;
                    }
                } else if (line[0] == '#') {
                    if (line.rfind("#linkerFlags", 0) == 0) {
                        savedLinkerFlags.clear();
                        size_t pos = 12;
                        while (pos < line.size()) {
                            while (pos < line.size() && line[pos] == ' ') pos++;
                            if (pos >= line.size()) break;
                            size_t end = line.find(' ', pos);
                            if (end == std::string::npos) end = line.size();
                            savedLinkerFlags.push_back(line.substr(pos, end - pos));
                            pos = end;
                        }
                    } else if (line.rfind("#buildHash", 0) == 0) {
                        savedFlagHash = line.substr(10);
                        // Trim leading space
                        if (!savedFlagHash.empty() && savedFlagHash[0] == ' ')
                            savedFlagHash = savedFlagHash.substr(1);
                    } else if (line.rfind("#objectFiles", 0) == 0) {
                        savedObjectFiles.clear();
                        size_t pos = 12;
                        while (pos < line.size()) {
                            while (pos < line.size() && line[pos] == ' ') pos++;
                            if (pos >= line.size()) break;
                            size_t end = line.find(' ', pos);
                            if (end == std::string::npos) end = line.size();
                            savedObjectFiles.push_back(line.substr(pos, end - pos));
                            pos = end;
                        }
                    }
                } else {
                    auto sep = line.find(' ');
                    if (sep == std::string::npos) { allMatch = false; break; }
                    auto filePath = line.substr(0, sep);
                    auto savedMtime = std::stoll(line.substr(sep + 1));
                    std::error_code ec;
                    auto currentMtime = fs::last_write_time(filePath, ec);
                    if (ec || std::chrono::duration_cast<std::chrono::seconds>(
                            currentMtime.time_since_epoch()).count() != savedMtime) {
                        allMatch = false;
                        break;
                    }
                }
            }
            if (allMatch && savedFlagHash != currentFlagHash) {
                allMatch = false;
                if (!pipeOpts.quiet)
                    std::cerr << "lucis: [build] flags changed, rebuilding\n";
            }
            if (allMatch) {
                std::error_code ec;
                size_t objCount = 0;
                for (auto& entry : fs::directory_iterator(pipelineBuildDir, ec)) {
                    if (entry.path().extension() == ".o") objCount++;
                }
                if (objCount > 0 && !ec) {
                    buildCached = true;
                    if (!pipeOpts.quiet)
                        std::cerr << "lucis: [build] sources unchanged, using cached objects\n";
                }
            }
        }
    }

    if (buildCached && !tasks.empty()) {
        buildCached = false;
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [build] emit flags active, rebuilding\n";
    }

    std::unique_ptr<PipelineResult> pipeline;
    std::vector<std::string> cachedObjectFiles;
    if (!buildCached) {
        pipeline = LucisPipeline::run(pipeOpts);
        if (!pipeline || pipeline->hasErrors) return 1;

        if (pipeline->buildDir.empty()) pipeline->buildDir = pipelineBuildDir;
        std::string cacheDir = pipeline->buildDir + "/cache";
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (!ec) {
            std::ofstream manifest(cacheManifestPath);
            if (manifest) {
                for (auto& unit : pipeline->units) {
                    std::error_code lec;
                    auto mtime = fs::last_write_time(unit.filePath, lec);
                    if (!lec) {
                        manifest << unit.filePath << " "
                            << std::chrono::duration_cast<std::chrono::seconds>(
                                mtime.time_since_epoch()).count() << "\n";
                    }
                }
                for (auto& cSrc : pipeline->cSourceFiles) {
                    std::error_code lec;
                    auto mtime = fs::last_write_time(cSrc, lec);
                    if (!lec) {
                        manifest << "C:" << cSrc << " "
                            << std::chrono::duration_cast<std::chrono::seconds>(
                                mtime.time_since_epoch()).count() << "\n";
                    }
                }
                for (auto& asmSrc : assemblySources) {
                    std::error_code lec;
                    auto mtime = fs::last_write_time(asmSrc, lec);
                    if (!lec) {
                        manifest << "ASM:" << asmSrc << " "
                            << std::chrono::duration_cast<std::chrono::seconds>(
                                mtime.time_since_epoch()).count() << "\n";
                    }
                }
                manifest << "#linkerFlags";
                for (auto& lf : pipeline->linkerFlags)
                    manifest << " " << lf;
                manifest << "\n";
                manifest << "#buildHash " << currentFlagHash << "\n";
            }
        }
    } else {
        // Only link the .o files that were recorded in the manifest.
        // Fallback: if the manifest predates #objectFiles support, scan the
        // build directory (old behaviour) so the user is not left with an
        // empty linker invocation.
        if (!savedObjectFiles.empty()) {
            cachedObjectFiles = savedObjectFiles;
        } else {
            std::error_code ec;
            for (auto& entry : fs::directory_iterator(pipelineBuildDir, ec)) {
                if (entry.path().extension() == ".o")
                    cachedObjectFiles.push_back(entry.path().string());
            }
        }
    }

    if (!buildCached && useConfig && !cfg->outDir.empty()) {
        auto outDirPath = fs::path(pipeline->projectRoot) / cfg->outDir;
        fs::create_directories(outDirPath);
        for (auto& t : tasks) {
            if (!t.outPath.empty() && fs::path(t.outPath).is_relative())
                t.outPath = (outDirPath / t.outPath).string();
        }
    }

    // ── Compile C sources ──────────────────────────────────────────────────
    std::vector<std::string> cObjectFiles;
    if (!buildCached && !pipeline->cSourceFiles.empty()) {
        std::vector<std::string> cIncFlags;
        for (auto& ip : pipeOpts.includePaths)
            cIncFlags.push_back("-I" + ip);
        // Config defines: -DKEY=VAL
        if (useConfig) {
            for (auto& [k, v] : cfg->build.defines)
                cIncFlags.push_back("-D" + k + "=" + v);
        }
        for (auto& cSrc : pipeline->cSourceFiles) {
            auto stem = fs::path(cSrc).stem().string();
            cIncFlags.push_back("-I" + fs::path(cSrc).parent_path().string());
            auto objPath = pipeline->buildDir + "/c__" + stem + ".o";
            if (!CodeGen::compileCSource(cSrc, objPath, cIncFlags, pipeOpts.quiet, pipeOpts.targetTriple)) {
                std::cerr << "lucis: failed to compile C source '" << cSrc << "'\n";
                return 1;
            }
            cObjectFiles.push_back(objPath);
        }
    }

    // ── Generate IR for all units ───────────────────────────────────────
    std::vector<std::string> objectFiles;

    if (!buildCached) {
        struct UnitIR {
            std::string filePath;
            std::unique_ptr<IRModule> mod;
        };
        std::vector<UnitIR> unitIRs;
        bool anyIRError = false;

        // Set up comptime engine and register all comptime functions
        ComptimeEngine comptimeEngine;
        for (auto& unit : pipeline->units) {
            if (!unit.parseResult || !unit.parseResult->tree) continue;
            for (auto* tld : unit.parseResult->tree->topLevelDecl()) {
                if (auto* func = tld->functionDecl()) {
                    if (func->COMPTIME()) {
                        auto name = func->IDENTIFIER(0)->getText();
                        comptimeEngine.registry().registerFunction(
                            name, func, func->typeParamList() != nullptr);
                    }
                }
            }
        }

        for (size_t idx = 0; idx < pipeline->units.size(); ++idx) {
            auto& unit = pipeline->units[idx];
            if (!pipeOpts.quiet)
                std::cerr << "lucis: [build ir " << (idx + 1) << "/" << pipeline->units.size()
                          << "] " << unit.filePath << "\n";

            IRGen irGen;
            irGen.setModuleContext(pipeline->registry.get(), unit.modulePath, unit.filePath, unit.isStdlib);
            irGen.setCBindings(pipeline->cBindings.get());
            irGen.setProjectRoot(pipeline->projectRoot);
            irGen.setSemanticDB(pipeline->semanticDB.get());
            irGen.setTargetTriple(pipeOpts.targetTriple);
            irGen.setNoStd(pipeOpts.noStd);
            irGen.setEmitDebugInfo(pipeOpts.emitDebugInfo);
            irGen.setComptimeEngine(&comptimeEngine);
            auto irMod = irGen.generate(unit.parseResult->tree, unit.filePath);
            if (!irMod) {
                std::cerr << "lucis: IR generation failed for '" << unit.filePath << "'\n";
                anyIRError = true;
                continue;
            }
            // Collect inline assembly files from this module
            for (auto& asmFile : irGen.inlineAssemblyFiles())
                assemblySources.push_back(asmFile.filePath);
            if (optLevel != OptimizationLevel::O0)
                Optimizer::optimize(*irMod, optLevel);
            unitIRs.push_back({unit.filePath, std::move(irMod)});
        }
        if (anyIRError) return 1;

        auto mainPath = fs::canonical(fs::path(pipeOpts.inputFile)).string();
        llvm::Module* mainMod = nullptr;
        for (auto& uir : unitIRs) {
            if (fs::canonical(fs::path(uir.filePath)).string() == mainPath) {
                mainMod = uir.mod->module();
                break;
            }
        }

        // ── Default emit output paths (when no -o given) ─────────────
        auto stem = fs::path(pipeOpts.inputFile).stem().string();
        std::string emitOutDir = projRoot + "/build";
        if (useConfig && !cfg->outDir.empty())
            emitOutDir = (fs::path(projRoot) / cfg->outDir).string();
        auto defaultEmitPath = [&](const char* ext) -> std::string {
            if (!outputFile.empty()) return "";
            fs::create_directories(emitOutDir);
            return emitOutDir + "/" + stem + ext;
        };
        // ── Split-mode helper: derive per-unit output path ────────────
        auto splitUnitPath = [&](const std::string& modulePath, const char* ext) -> std::string {
            std::string relPath = modulePath;
            for (auto& c : relPath) if (c == '/' || c == '\\') c = '_';
            return outputFile + "/" + relPath + ext;
        };

        // ── Execute emit tasks ───────────────────────────────────────
        if (!tasks.empty()) {
            bool anyEmitError = false;
            for (auto& t : tasks) {
                if (t.type == EmitTask::LLVM) {
                    if (useSplit) {
                        fs::create_directories(outputFile);
                        for (auto& uir : unitIRs) {
                            std::string modPath = uir.filePath;
                            for (auto& unit : pipeline->units) {
                                if (unit.filePath == uir.filePath) {
                                    modPath = unit.modulePath;
                                    break;
                                }
                            }
                            auto unitPath = splitUnitPath(modPath, ".ll");
                            std::error_code ec;
                            llvm::raw_fd_ostream dest(unitPath, ec, llvm::sys::fs::OF_None);
                            if (ec) {
                                std::cerr << "lucis: split LLVM: could not open '" << unitPath << "': " << ec.message() << "\n";
                                anyEmitError = true;
                            } else {
                                uir.mod->module()->print(dest, nullptr);
                                if (!pipeOpts.quiet)
                                    std::cout << "lucis: LLVM IR written to '" << unitPath << "'\n";
                            }
                        }
                    } else if (useRecursive) {
                        if (!t.outPath.empty()) {
                            std::error_code ec;
                            llvm::raw_fd_ostream dest(t.outPath, ec, llvm::sys::fs::OF_None);
                            if (ec) {
                                std::cerr << "lucis: could not open '" << t.outPath << "': " << ec.message() << "\n";
                                anyEmitError = true;
                            } else {
                                for (auto& uir : unitIRs) {
                                    uir.mod->module()->print(dest, nullptr);
                                    dest << "\n";
                                }
                                if (!pipeOpts.quiet)
                                    std::cout << "lucis: LLVM IR written to '" << t.outPath << "'\n";
                            }
                        } else {
                            bool first = true;
                            for (auto& uir : unitIRs) {
                                if (!first) llvm::outs() << "\n";
                                uir.mod->module()->print(llvm::outs(), nullptr);
                                first = false;
                            }
                        }
                    } else {
                        if (!mainMod) { std::cerr << "lucis: no main module for LLVM emit\n"; anyEmitError = true; break; }
                        if (!t.outPath.empty()) {
                            std::error_code ec;
                            llvm::raw_fd_ostream dest(t.outPath, ec, llvm::sys::fs::OF_None);
                            if (ec) {
                                std::cerr << "lucis: could not open '" << t.outPath << "': " << ec.message() << "\n";
                                anyEmitError = true;
                            } else {
                                mainMod->print(dest, nullptr);
                                if (!pipeOpts.quiet)
                                    std::cout << "lucis: LLVM IR written to '" << t.outPath << "'\n";
                            }
                        } else {
                            mainMod->print(llvm::outs(), nullptr);
                        }
                    }
                } else if (t.type == EmitTask::ASM) {
                    if (useSplit) {
                        fs::create_directories(outputFile);
                        for (auto& uir : unitIRs) {
                            std::string modPath = uir.filePath;
                            for (auto& unit : pipeline->units) {
                                if (unit.filePath == uir.filePath) {
                                    modPath = unit.modulePath;
                                    break;
                                }
                            }
                            auto unitPath = splitUnitPath(modPath, ".s");
                            if (!CodeGen::emitAssembly(uir.mod->module(), unitPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel, asmSyntax)) {
                                std::cerr << "lucis: split ASM: failed to emit '" << unitPath << "'\n";
                                anyEmitError = true;
                            } else if (!pipeOpts.quiet) {
                                std::cout << "lucis: assembly written to '" << unitPath << "'\n";
                            }
                        }
                    } else if (useRecursive) {
                        auto emitOneAsm = [&](llvm::Module* mod, llvm::raw_fd_ostream& dest) -> bool {
                            char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                            int fd = mkstemps(tmpAsm, 2);
                            if (fd == -1) return false;
                            ::close(fd);
                            if (!CodeGen::emitAssembly(mod, tmpAsm, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel, asmSyntax)) { fs::remove(tmpAsm); return false; }
                            std::ifstream in(tmpAsm);
                            dest << in.rdbuf();
                            fs::remove(tmpAsm);
                            return true;
                        };
                        if (!t.outPath.empty()) {
                            std::error_code ec;
                            llvm::raw_fd_ostream dest(t.outPath, ec, llvm::sys::fs::OF_None);
                            if (ec) {
                                std::cerr << "lucis: could not open '" << t.outPath << "': " << ec.message() << "\n";
                                anyEmitError = true;
                            } else {
                                for (auto& uir : unitIRs) {
                                    if (!emitOneAsm(uir.mod->module(), dest)) { anyEmitError = true; break; }
                                    dest << "\n";
                                }
                                if (!pipeOpts.quiet && !anyEmitError)
                                    std::cout << "lucis: assembly written to '" << t.outPath << "'\n";
                            }
                        } else {
                            for (auto& uir : unitIRs) {
                                char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                                int fd = mkstemps(tmpAsm, 2);
                                if (fd != -1) {
                                    ::close(fd);
                                    if (CodeGen::emitAssembly(uir.mod->module(), tmpAsm, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel, asmSyntax)) {
                                        std::ifstream in(tmpAsm);
                                        std::cout << in.rdbuf();
                                        std::cout << "\n";
                                    }
                                    fs::remove(tmpAsm);
                                }
                            }
                        }
                    } else {
                        if (!mainMod) { std::cerr << "lucis: no main module for ASM emit\n"; anyEmitError = true; break; }
                        if (!t.outPath.empty()) {
                            if (!CodeGen::emitAssembly(mainMod, t.outPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel, asmSyntax)) anyEmitError = true;
                            else if (!pipeOpts.quiet)
                                std::cout << "lucis: assembly written to '" << t.outPath << "'\n";
                        } else {
                            char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                            int fd = mkstemps(tmpAsm, 2);
                            if (fd != -1) {
                                ::close(fd);
                                if (CodeGen::emitAssembly(mainMod, tmpAsm, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel, asmSyntax)) {
                                    std::ifstream in(tmpAsm);
                                    std::cout << in.rdbuf();
                                } else anyEmitError = true;
                                fs::remove(tmpAsm);
                            } else anyEmitError = true;
                        }
                    }
                } else if (t.type == EmitTask::BC) {
                    if (useSplit) {
                        fs::create_directories(outputFile);
                        for (auto& uir : unitIRs) {
                            std::string modPath = uir.filePath;
                            for (auto& unit : pipeline->units) {
                                if (unit.filePath == uir.filePath) {
                                    modPath = unit.modulePath;
                                    break;
                                }
                            }
                            auto unitPath = splitUnitPath(modPath, ".bc");
                            if (!CodeGen::emitBitcode(uir.mod->module(), unitPath)) {
                                std::cerr << "lucis: split BC: failed to emit '" << unitPath << "'\n";
                                anyEmitError = true;
                            } else if (!pipeOpts.quiet) {
                                std::cout << "lucis: bitcode written to '" << unitPath << "'\n";
                            }
                        }
                    } else if (useRecursive) {
                        // Link all modules via LLVM Linker, then emit bitcode
                        auto masterCtx = std::make_unique<llvm::LLVMContext>();
                        std::unique_ptr<llvm::Module> masterMod;
                        for (auto& uir : unitIRs) {
                            // Serialize + re-parse into master context
                            llvm::SmallVector<char, 0> buf;
                            llvm::raw_svector_ostream os(buf);
                            llvm::WriteBitcodeToFile(*uir.mod->module(), os);
                            auto memBuf = llvm::MemoryBuffer::getMemBufferCopy(
                                llvm::StringRef(buf.data(), buf.size()), uir.filePath);
                            auto parsed = llvm::parseBitcodeFile(memBuf->getMemBufferRef(), *masterCtx);
                            if (!parsed) {
                                std::cerr << "lucis: bitcode re-parse failed for '" << uir.filePath << "'\n";
                                anyEmitError = true;
                                break;
                            }
                            if (!masterMod) {
                                masterMod = std::move(parsed.get());
                            } else {
                                if (llvm::Linker::linkModules(*masterMod, std::move(parsed.get()))) {
                                    std::cerr << "lucis: failed to link module '" << uir.filePath << "'\n";
                                    anyEmitError = true;
                                    break;
                                }
                            }
                        }
                        if (anyEmitError) break;
                        if (!masterMod) { std::cerr << "lucis: no IR for BC emit\n"; anyEmitError = true; break; }

                        auto outPath = t.outPath.empty() ? defaultEmitPath(".bc") : t.outPath;
                        if (!CodeGen::emitBitcode(masterMod.get(), outPath)) anyEmitError = true;
                        else if (!pipeOpts.quiet)
                            std::cout << "lucis: bitcode written to '" << outPath << "'\n";
                    } else {
                        if (!mainMod) { std::cerr << "lucis: no main module for BC emit\n"; anyEmitError = true; break; }
                        auto outPath = t.outPath.empty() ? defaultEmitPath(".bc") : t.outPath;
                        if (!CodeGen::emitBitcode(mainMod, outPath)) anyEmitError = true;
                        else if (!pipeOpts.quiet)
                            std::cout << "lucis: bitcode written to '" << outPath << "'\n";
                    }
                }
            }
            if (anyEmitError) return 1;

            // ── Obj emit ─────────────────────────────────────────────
            if (hasObjEmit) {
                std::string objEmitPath = defaultEmitPath(".o");
                for (auto& t : tasks) {
                    if (t.type == EmitTask::OBJ && !t.outPath.empty()) {
                        objEmitPath = t.outPath;
                        break;
                    }
                }

                if (useSplit) {
                    // Generate individual .o files, one per unit, into output directory
                    fs::create_directories(outputFile);
                    for (auto& uir : unitIRs) {
                        std::string modPath = uir.filePath;
                        for (auto& unit : pipeline->units) {
                            if (unit.filePath == uir.filePath) {
                                modPath = unit.modulePath;
                                break;
                            }
                        }
                        auto unitPath = splitUnitPath(modPath, ".o");
                        if (!CodeGen::emitObjectFile(uir.mod->module(), unitPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel)) {
                            std::cerr << "lucis: split OBJ: failed to emit '" << unitPath << "'\n";
                            return 1;
                        }
                        if (!pipeOpts.quiet)
                            std::cout << "lucis: object file written to '" << unitPath << "'\n";
                    }
                } else if (useRecursive) {
                    // Generate .o for all units, then ld -r merge
                    std::vector<std::string> allObjs;
                    for (auto& uir : unitIRs) {
                        std::string stem;
                        for (auto& unit : pipeline->units) {
                            if (unit.filePath == uir.filePath) {
                                stem = unit.modulePath + "__" + fs::path(uir.filePath).stem().string();
                                break;
                            }
                        }
                        if (stem.empty()) stem = fs::path(uir.filePath).stem().string();
                        for (auto& c : stem) if (c == '/' || c == '\\') c = '_';
                        auto objPath = pipelineBuildDir + "/" + stem + ".o";
                        fs::create_directories(fs::path(objPath).parent_path());
                        if (!CodeGen::emitObjectFile(uir.mod->module(), objPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel)) {
                            std::cerr << "lucis: failed to emit object for '" << uir.filePath << "'\n";
                            return 1;
                        }
                        allObjs.push_back(objPath);
                    }
                    for (auto& co : cObjectFiles)
                        allObjs.push_back(co);

                    pid_t ldPid = ::fork();
                    if (ldPid < 0) { std::cerr << "lucis: failed to fork for ld -r\n"; return 1; }
                    if (ldPid == 0) {
                        if (pipeOpts.quiet) {
                            int devNull = ::open("/dev/null", O_WRONLY);
                            if (devNull >= 0) {
                                ::dup2(devNull, STDOUT_FILENO);
                                ::dup2(devNull, STDERR_FILENO);
                                if (devNull > STDERR_FILENO) ::close(devNull);
                            }
                        }
                        std::vector<const char*> argv;
                        argv.push_back("ld");
                        argv.push_back("-r");
                        for (auto& obj : allObjs) argv.push_back(obj.c_str());
                        argv.push_back("-o");
                        argv.push_back(objEmitPath.c_str());
                        argv.push_back(nullptr);
                        ::execvp("ld", const_cast<char**>(argv.data()));
                        ::_exit(127);
                    }
                    int ldStatus = 0;
                    while (::waitpid(ldPid, &ldStatus, 0) < 0)
                        if (errno != EINTR) { std::cerr << "lucis: ld -r failed\n"; return 1; }
                    if (!WIFEXITED(ldStatus) || WEXITSTATUS(ldStatus) != 0) {
                        std::cerr << "lucis: ld -r failed (exit " << WEXITSTATUS(ldStatus) << ")\n";
                        return 1;
                    }
                    if (!pipeOpts.quiet)
                        std::cout << "lucis: relocatable object written to '" << objEmitPath << "'\n";
                } else {
                    if (!mainMod) { std::cerr << "lucis: no main module for OBJ emit\n"; return 1; }
                    if (!CodeGen::emitObjectFile(mainMod, objEmitPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel)) {
                        std::cerr << "lucis: failed to emit object file\n";
                        return 1;
                    }
                    if (!pipeOpts.quiet)
                        std::cout << "lucis: object file written to '" << objEmitPath << "'\n";
                }
                return 0;
            }

            if (hasTextEmit) return 0;
        }

        for (auto& uir : unitIRs) {
            std::string stem;
            for (auto& unit : pipeline->units) {
                if (unit.filePath == uir.filePath) {
                    stem = unit.modulePath + "__" + fs::path(uir.filePath).stem().string();
                    break;
                }
            }
            if (stem.empty()) stem = fs::path(uir.filePath).stem().string();
            for (auto& c : stem) if (c == '/' || c == '\\') c = '_';

            auto objPath = pipeline->buildDir + "/" + stem + ".o";
            fs::create_directories(fs::path(objPath).parent_path());
            if (!CodeGen::emitObjectFile(uir.mod->module(), objPath, usePIC, pipeOpts.targetTriple, pipeOpts.codeModel)) {
                std::cerr << "lucis: failed to emit object for '" << uir.filePath << "'\n";
                return 1;
            }
            objectFiles.push_back(objPath);
        }
        objectFiles.insert(objectFiles.end(), cObjectFiles.begin(), cObjectFiles.end());

        // Compile assembly sources
        for (auto& asmSrc : assemblySources) {
            auto stem = fs::path(asmSrc).stem().string();
            auto objPath = pipelineBuildDir + "/asm__" + stem + ".o";
            fs::create_directories(fs::path(objPath).parent_path());
            if (!CodeGen::compileAssembly(asmSrc, objPath, pipeOpts.targetTriple,
                                          pipeOpts.quiet, assemblerChoice, assemblerFlags)) {
                std::cerr << "lucis: failed to assemble '" << asmSrc << "'\n";
                return 1;
            }
            objectFiles.push_back(objPath);
        }

        // Extra object/archive files passed directly
        for (auto& obj : extraObjectFiles)
            objectFiles.push_back(obj);

        // Save the list of object file paths in the manifest so that on cache
        // hit we only link these files (not ALL .o files from the build dir).
        {
            std::ofstream manifest(cacheManifestPath, std::ios::app);
            if (manifest) {
                manifest << "#objectFiles";
                for (auto& obj : objectFiles)
                    manifest << " " << obj;
                manifest << "\n";
            }
        }
    } else {
        objectFiles = std::move(cachedObjectFiles);

        // Extra object/archive files (always included, even on cache hit)
        for (auto& obj : extraObjectFiles)
            objectFiles.push_back(obj);
    }

    // ── Link ────────────────────────────────────────────────────────────────
    if (buildCached) {
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [build] linking cached objects\n";
    }

    if (outputFile.empty()) {
        if (useConfig && !cfg->output.path.empty()) {
            outputFile = cfg->output.path;
            if (fs::path(outputFile).is_relative())
                outputFile = (fs::path(projRoot) / outputFile).string();
        } else if (useConfig && !cfg->binary.empty()) {
            outputFile = cfg->binary;
        } else {
            outputFile = fs::path(pipeOpts.inputFile).stem().string() + ".out";
        }

        if (useConfig && !cfg->outDir.empty()) {
            auto outDirPath = fs::path(buildCached ? projRoot : pipeline->projectRoot) / cfg->outDir;
            fs::create_directories(outDirPath);
            outputFile = (outDirPath / outputFile).string();
        }

        if (!pipeOpts.quiet)
            std::cerr << "lucis: [build] no -o given, writing to '" << outputFile << "'\n";
    }
    if (!pipeOpts.quiet)
        std::cerr << "\nlucis: [build] --- linker output ---\n\n";

    std::vector<std::string> finalLinkerFlags = buildCached ? savedLinkerFlags : pipeline->linkerFlags;
    if (useLTO)    finalLinkerFlags.push_back("-flto");
    if (useStatic) finalLinkerFlags.push_back("-static");
    if (useShared) finalLinkerFlags.push_back("-shared");

    auto deprecate = [&](const std::string& oldFlag, const std::string& newFlag) {
        if (parser.has(oldFlag) && !parser.has(newFlag))
            std::cerr << "lucis: warning: --" << oldFlag << " is deprecated, use --" << newFlag << "\n";
    };
    deprecate("nmagic", "linker-nmagic");
    deprecate("omagic", "linker-omagic");
    deprecate("gc-sections", "linker-gc-sections");
    deprecate("link-arg", "linker-arg");

    if (parser.has("linker-gc-sections") || parser.has("gc-sections"))
        finalLinkerFlags.push_back(isRawLd ? "--gc-sections" : "-Wl,--gc-sections");

    if (parser.has("linker-nmagic") || parser.has("nmagic")) {
        finalLinkerFlags.push_back(isRawLd ? "-n" : "-Wl,-n");
    }
    if (parser.has("linker-omagic") || parser.has("omagic")) {
        finalLinkerFlags.push_back(isRawLd ? "-N" : "-Wl,-N");
    }

    if (parser.has("linker-script")) {
        auto lsPath = parser.get("linker-script");
        // Resolve relative to CWD (CLI flags use CWD, not project root)
        if (!fs::path(lsPath).is_absolute())
            lsPath = (fs::current_path() / lsPath).string();
        if (isRawLd) {
            finalLinkerFlags.push_back("-T");
            finalLinkerFlags.push_back(lsPath);
        } else {
            finalLinkerFlags.push_back("-Wl,-T" + lsPath);
        }
    } else if (useConfig && !cfg->linker.script.empty()) {
        auto lsPath = cfg->linker.script;
        if (fs::path(lsPath).is_relative())
            lsPath = (fs::path(projRoot) / lsPath).string();
        if (isRawLd) {
            finalLinkerFlags.push_back("-T");
            finalLinkerFlags.push_back(lsPath);
        } else {
            finalLinkerFlags.push_back("-Wl,-T" + lsPath);
        }
    }

    // linker-arg (new) + link-arg (deprecated)
    for (auto& arg : parser.getAll("linker-arg"))
        finalLinkerFlags.push_back(arg);
    for (auto& arg : parser.getAll("link-arg"))
        finalLinkerFlags.push_back(arg);

    // Config linker.flags and linker.args (CLI > config)
    if (useConfig) {
        for (auto& flag : cfg->linker.flags)
            finalLinkerFlags.push_back(flag);
        for (auto& arg : cfg->linker.args)
            finalLinkerFlags.push_back(arg);
    }

    if (parser.has("rpath")) {
        auto rpathDir = parser.get("rpath");
        if (isRawLd) {
            finalLinkerFlags.push_back("-rpath");
            finalLinkerFlags.push_back(rpathDir);
        } else {
            finalLinkerFlags.push_back("-Wl,-rpath," + rpathDir);
        }
    }

    if (!CodeGen::linkObjectFiles(objectFiles, outputFile,
                                    finalLinkerFlags,
                                    libPaths,
                                    !useStatic && !pipeOpts.noStd, pipeOpts.quiet,
                                    pipeOpts.entryPoint,
                                    pipeOpts.noStd,
                                    parser.get("linker"))) {
        std::cerr << "lucis: failed to link binary '" << outputFile << "'\n";
        return 1;
    }

    // ── Run post-build scripts (after everything) ─────────────────
    if (useConfig && !cfg->scripts.pos.empty()) {
        setScriptEnv(outputFile);
        for (auto& cmd : cfg->scripts.pos) {
            std::cerr << "lucis: [scripts:pos] " << cmd << "\n";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "lucis: [scripts:pos] command failed with exit code " << ret << "\n";
                return 1;
            }
        }
    }

    // ── Emit raw binary via objcopy (after poscmds, on final ELF) ─
    std::string objcopyCmd = "objcopy";
    if (useConfig && !cfg->tools.objcopy.empty())
        objcopyCmd = cfg->tools.objcopy;

    for (auto& t : tasks) {
        if (t.type != EmitTask::BIN) continue;
        std::string binPath;
        if (t.outPath.empty() || t.outPath == outputFile)
            binPath = (fs::path(outputFile).parent_path() / (fs::path(outputFile).stem().string() + ".bin")).string();
        else
            binPath = t.outPath;
        std::string cmd = objcopyCmd + " -O binary " + outputFile + " " + binPath;
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [emit-bin] " << cmd << "\n";
        int ret = system(cmd.c_str());
        if (ret != 0) {
            std::cerr << "lucis: " << objcopyCmd << " failed with exit code " << ret << "\n";
            return 1;
        }
    }

    // ── Strip debug/symbol info ──────────────────────────────────────────
    bool shouldStrip = parser.has("strip") || (useConfig && cfg->output.strip);
    if (shouldStrip) {
        std::string stripCmd = objcopyCmd + " --strip-all " + outputFile;
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [strip] " << stripCmd << "\n";
        int ret = system(stripCmd.c_str());
        if (ret != 0) {
            std::cerr << "lucis: " << objcopyCmd << " strip failed with exit code " << ret << "\n";
            return 1;
        }
    }

    std::cout << "lucis: binary written to '" << outputFile << "'\n";
    return 0;
}
