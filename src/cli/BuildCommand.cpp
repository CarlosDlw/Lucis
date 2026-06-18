#include "cli/BuildCommand.h"
#include "cli/ArgParser.h"
#include "cli/LucisPipeline.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <unistd.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>

namespace fs = std::filesystem;

void BuildCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file (auto-resolved from lucis.yaml if omitted)", false);
    parser.addOption("output", 'o', "FILE", "Output binary path (default: <input>.out, or lucis.yaml binary name verbatim)");
    parser.addFlag("emit-llvm", '\0', "Emit LLVM IR (.ll)");
    parser.addFlag("emit-asm",  '\0', "Emit assembly (.s)");
    parser.addFlag("emit-bc",   '\0', "Emit LLVM bitcode (.bc)");
    parser.addFlag("emit-obj",  '\0', "Emit object file (.o)");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, or fast (default: 0)");
    parser.addFlag("lto", '\0', "Enable Link Time Optimization");
    parser.addFlag("static", '\0', "Produce a statically linked executable");
    parser.addFlag("shared", '\0', "Produce a shared library");
    parser.addFlag("fPIC",   '\0', "Generate position-independent code");
    parser.addOption("link-arg", '\0', "FLAG", "Pass argument directly to linker (repeatable)", true);
    parser.addOption("rpath",    '\0', "DIR", "Add runtime library search path", false);
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addOption("link", 'l', "LIB", "Link against a library (repeatable)", true);
    parser.addOption("lib-path", 'L', "DIR", "Add library search path (repeatable)", true);
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
}

std::string BuildCommand::resolveInputFile(const ArgParser& parser,
                                            LucisConfig* outConfig) const {
    auto file = parser.get("file");

    auto config = LucisConfig::findInDir(fs::current_path().string());
    if (config && outConfig) *outConfig = *config;

    if (!file.empty()) {
        if (!fs::is_directory(file)) return file;
        // Directory was passed — fall through to config-based search
    }

    if (!config) return {};

    for (auto& candidate : { "src/main.lc", "main.lc" }) {
        auto path = fs::path(candidate);
        if (fs::exists(path))
            return fs::canonical(path).string();
    }
    return {};
}

int BuildCommand::run(const ArgParser& parser) {
    LucisConfig config;
    std::string inputFile = resolveInputFile(parser, &config);
    if (inputFile.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis build <file>   or   lucis build  (from a project with lucis.yaml)\n";
        return 1;
    }

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile        = inputFile;
    pipeOpts.quiet            = parser.has("quiet") ? true : config.build.quiet;
    pipeOpts.includePaths     = parser.has("include") ? parser.getAll("include") : config.includes;
    pipeOpts.userLinkerFlags  = parser.has("link") ? parser.getAll("link") : config.linker.libs;
    pipeOpts.binaryName       = config.binary;
    pipeOpts.outDir           = config.outDir;
    pipeOpts.sourcePaths      = config.sourcePaths;

    OptimizationLevel lucisOptLevel = OptimizationLevel::O0;

    // Apply config opt_level as default, then CLI override
    {
        auto parseOpt = [](const std::string& s) -> OptimizationLevel {
            if (s == "0")      return OptimizationLevel::O0;
            if (s == "1")      return OptimizationLevel::O1;
            if (s == "2")      return OptimizationLevel::O2;
            if (s == "3")      return OptimizationLevel::O3;
            if (s == "s")      return OptimizationLevel::Os;
            if (s == "z")      return OptimizationLevel::Oz;
            if (s == "fast")   return OptimizationLevel::Ofast;
            return OptimizationLevel::O0;
        };
        lucisOptLevel = parseOpt(config.build.optLevel);
    }

    if (parser.has("opt")) {
        std::string optStr = parser.get("opt");
        if (optStr == "0")      lucisOptLevel = OptimizationLevel::O0;
        else if (optStr == "1") lucisOptLevel = OptimizationLevel::O1;
        else if (optStr == "2") lucisOptLevel = OptimizationLevel::O2;
        else if (optStr == "3") lucisOptLevel = OptimizationLevel::O3;
        else if (optStr == "s") lucisOptLevel = OptimizationLevel::Os;
        else if (optStr == "z") lucisOptLevel = OptimizationLevel::Oz;
        else if (optStr == "fast") lucisOptLevel = OptimizationLevel::Ofast;
        else {
            try {
                int level = std::stoi(optStr);
                if (level >= 0 && level <= 3)
                    lucisOptLevel = static_cast<OptimizationLevel>(level);
            } catch (...) {
                // Fallback to O0 or log warning if needed
            }
        }
    }

    std::string outputFile = parser.get("output");
    bool useLTO       = parser.has("lto") ? true : config.build.lto;
    bool useStatic    = parser.has("static") ? true : config.build.staticLink;
    bool useShared    = parser.has("shared") ? true : config.build.shared;

    // ── Resolve emit tasks from config + CLI overrides ──────────────────────
    struct EmitTask {
        enum Type { LLVM, ASM, BC, OBJ };
        Type type;
        std::string outPath; // resolved full path, empty = stdout for text emits
    };
    std::vector<EmitTask> tasks;
    bool emitObjEnabled = false;
    bool hasTextEmits = false;

    auto addEmitTask = [&](const std::string& key, EmitTask::Type etype) {
        bool fromConfig = false;
        std::string cfgFile;
        auto it = config.build.emits.find(key);
        if (it != config.build.emits.end()) {
            fromConfig = it->second.enabled;
            cfgFile    = it->second.file;
        }
        bool enabled = parser.has("emit-" + key) || fromConfig;
        if (!enabled) return;

        EmitTask t;
        t.type = etype;
        if (etype == EmitTask::OBJ) emitObjEnabled = true;
        else hasTextEmits = true;

        // Resolve output path
        if (!cfgFile.empty()) {
            t.outPath = cfgFile;
        } else if (!outputFile.empty()) {
            t.outPath = outputFile;
        }
        // else: text emits go to stdout (outPath stays empty)

        tasks.push_back(t);
    };

    addEmitTask("llvm", EmitTask::LLVM);
    addEmitTask("asm",  EmitTask::ASM);
    addEmitTask("bc",   EmitTask::BC);
    addEmitTask("obj",  EmitTask::OBJ);

    // If there are multiple emits writing to the same path, error out
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

    // Intelligent PIC inference
    bool usePIC;
    if (parser.has("fPIC")) {
        usePIC = true;
    } else if (useShared) {
        usePIC = true;
    } else if (useStatic) {
        usePIC = false;
    } else {
        usePIC = config.build.fpic;
    }

    // ── Incremental build cache ──────────────────────────────────────────
    // Check if all object files from the previous build are up-to-date
    // (source mtimes unchanged since last build).
    bool buildCached = false;
    std::string projRoot = LucisPipeline::getProjectRoot(pipeOpts.inputFile);
    std::string pipelineBuildDir = projRoot + "/.lucis/build";
    std::string cacheManifestPath = pipelineBuildDir + "/cache/build_manifest.txt";

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [build] checking incremental cache\n";

    {
        std::ifstream manifest(cacheManifestPath);
        if (manifest) {
            std::string line;
            bool allMatch = true;
            while (std::getline(manifest, line)) {
                if (line.empty()) continue;
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

    std::unique_ptr<PipelineResult> pipeline;
    std::vector<std::string> cachedObjectFiles;
    if (!buildCached) {
        pipeline = LucisPipeline::run(pipeOpts);
        if (!pipeline || pipeline->hasErrors) return 1;

        // Save build manifest after successful compilation
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
            }
        }
    } else {
        // ── Cached build: collect existing .o files and link directly ──
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(pipelineBuildDir, ec)) {
            if (entry.path().extension() == ".o")
                cachedObjectFiles.push_back(entry.path().string());
        }
    }

    // Prepend out_dir to emit file paths (need projectRoot from pipeline)
    if (!buildCached && !config.outDir.empty()) {
        auto outDirPath = fs::path(pipeline->projectRoot) / config.outDir;
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
        for (auto& ip : parser.getAll("include"))
            cIncFlags.push_back("-I" + ip);
        for (auto& cSrc : pipeline->cSourceFiles) {
            auto stem = fs::path(cSrc).stem().string();
            cIncFlags.push_back("-I" + fs::path(cSrc).parent_path().string());
            auto objPath = pipeline->buildDir + "/c__" + stem + ".o";
            if (!CodeGen::compileCSource(cSrc, objPath, cIncFlags, pipeOpts.quiet)) {
                std::cerr << "lucis: failed to compile C source '" << cSrc << "'\n";
                return 1;
            }
            cObjectFiles.push_back(objPath);
        }
    }

    // ── Generate object files (or use cached) ──────────────────────────
    std::vector<std::string> objectFiles;

    if (!buildCached) {
        // ── Generate IR for all units ──────────────────────────────────
        struct UnitIR {
            std::string filePath;
            std::unique_ptr<IRModule> mod;
        };
        std::vector<UnitIR> unitIRs;
        bool anyIRError = false;

        for (size_t idx = 0; idx < pipeline->units.size(); ++idx) {
            auto& unit = pipeline->units[idx];
            if (!pipeOpts.quiet)
                std::cerr << "lucis: [build ir " << (idx + 1) << "/" << pipeline->units.size()
                          << "] " << unit.filePath << "\n";

            IRGen irGen;
            irGen.setModuleContext(pipeline->registry.get(), unit.modulePath, unit.filePath);
            irGen.setCBindings(pipeline->cBindings.get());
            irGen.setSemanticDB(pipeline->semanticDB.get());
            auto irMod = irGen.generate(unit.parseResult.tree, unit.filePath);
            if (!irMod) {
                std::cerr << "lucis: IR generation failed for '" << unit.filePath << "'\n";
                anyIRError = true;
                continue;
            }
            if (lucisOptLevel != OptimizationLevel::O0)
                Optimizer::optimize(*irMod, lucisOptLevel);
            unitIRs.push_back({unit.filePath, std::move(irMod)});
        }
        if (anyIRError) return 1;

        // Locate the main unit's module
        auto mainPath = fs::canonical(fs::path(pipeOpts.inputFile)).string();
        llvm::Module* mainMod = nullptr;
        for (auto& uir : unitIRs) {
            if (fs::canonical(fs::path(uir.filePath)).string() == mainPath) {
                mainMod = uir.mod->module();
                break;
            }
        }

        // ── Execute emit tasks on the main module ──────────────────────
        if (!tasks.empty()) {
            bool anyEmitError = false;
            for (auto& t : tasks) {
                if (t.type == EmitTask::LLVM) {
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
                } else if (t.type == EmitTask::ASM) {
                    if (!mainMod) { std::cerr << "lucis: no main module for ASM emit\n"; anyEmitError = true; break; }
                    if (!t.outPath.empty()) {
                        if (CodeGen::emitAssembly(mainMod, t.outPath)) {
                            if (!pipeOpts.quiet)
                                std::cout << "lucis: assembly written to '" << t.outPath << "'\n";
                        } else anyEmitError = true;
                    } else {
                        char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                        int fd = mkstemps(tmpAsm, 2);
                        if (fd != -1) {
                            ::close(fd);
                            if (CodeGen::emitAssembly(mainMod, tmpAsm)) {
                                std::ifstream in(tmpAsm);
                                std::cout << in.rdbuf();
                                fs::remove(tmpAsm);
                            } else anyEmitError = true;
                        } else anyEmitError = true;
                    }
                } else if (t.type == EmitTask::BC) {
                    if (!mainMod) { std::cerr << "lucis: no main module for BC emit\n"; anyEmitError = true; break; }
                    if (!t.outPath.empty()) {
                        if (CodeGen::emitBitcode(mainMod, t.outPath)) {
                            if (!pipeOpts.quiet)
                                std::cout << "lucis: bitcode written to '" << t.outPath << "'\n";
                        } else anyEmitError = true;
                    } else {
                        llvm::WriteBitcodeToFile(*mainMod, llvm::outs());
                    }
                }
            }
            if (anyEmitError) return 1;

            if (emitObjEnabled) {
                std::string objEmitPath = pipeline->buildDir + "/" + fs::path(pipeOpts.inputFile).stem().string() + ".o";
                for (auto& t : tasks) {
                    if (t.type == EmitTask::OBJ && !t.outPath.empty()) {
                        objEmitPath = t.outPath;
                        break;
                    }
                }
                if (!mainMod) { std::cerr << "lucis: no main module for OBJ emit\n"; return 1; }
                if (!CodeGen::emitObjectFile(mainMod, objEmitPath, usePIC)) {
                    std::cerr << "lucis: failed to emit object file\n";
                    return 1;
                }
                if (!pipeOpts.quiet)
                    std::cout << "lucis: object file written to '" << objEmitPath << "'\n";
                return 0;
            }

            if (hasTextEmits) return 0;
        }

        for (auto& arg : parser.remaining()) {
            if (fs::path(arg).extension() == ".o") {
                objectFiles.push_back(fs::canonical(arg).string());
            } else {
                std::cerr << "lucis: unexpected argument '" << arg << "'\n";
                return 1;
            }
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
            if (!CodeGen::emitObjectFile(uir.mod->module(), objPath, usePIC)) {
                std::cerr << "lucis: failed to emit object for '" << uir.filePath << "'\n";
                return 1;
            }
            objectFiles.push_back(objPath);
        }
        objectFiles.insert(objectFiles.end(), cObjectFiles.begin(), cObjectFiles.end());
    } else {
        objectFiles = std::move(cachedObjectFiles);
    }

    // ── Link ────────────────────────────────────────────────────────────────
    if (buildCached) {
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [build] linking cached objects\n";
    }

    if (outputFile.empty()) {
        if (!config.binary.empty()) {
            outputFile = config.binary;
        } else {
            auto inPath = fs::path(pipeOpts.inputFile);
            outputFile = inPath.stem().string() + ".out";
        }

        if (!config.outDir.empty()) {
            auto outDirPath = fs::path(buildCached ? projRoot : pipeline->projectRoot) / config.outDir;
            fs::create_directories(outDirPath);
            outputFile = (outDirPath / outputFile).string();
        }

        if (!pipeOpts.quiet)
            std::cerr << "lucis: [build] no -o given, writing to '" << outputFile << "'\n";
    }
    if (!pipeOpts.quiet)
        std::cerr << "\nlucis: [build] --- linker output ---\n\n";

    std::vector<std::string> finalLinkerFlags = buildCached ? std::vector<std::string>{} : pipeline->linkerFlags;
    if (useLTO)         finalLinkerFlags.push_back("-flto");
    if (useStatic)      finalLinkerFlags.push_back("-static");
    if (useShared)      finalLinkerFlags.push_back("-shared");

    for (const auto& f : config.linker.flags)
        finalLinkerFlags.push_back(f);
    for (auto& arg : parser.getAll("link-arg"))
        finalLinkerFlags.push_back(arg);

    if (parser.has("rpath"))
        finalLinkerFlags.push_back("-Wl,-rpath," + parser.get("rpath"));
    else if (!config.linker.rpath.empty())
        finalLinkerFlags.push_back("-Wl,-rpath," + config.linker.rpath);

    auto libPaths = parser.has("lib-path") ? parser.getAll("lib-path") : config.linker.libPaths;

    if (!CodeGen::linkObjectFiles(objectFiles, outputFile,
                                    finalLinkerFlags,
                                    libPaths,
                                    !useStatic, pipeOpts.quiet)) {
        std::cerr << "lucis: failed to link binary '" << outputFile << "'\n";
        return 1;
    }
    std::cout << "lucis: binary written to '" << outputFile << "'\n";
    return 0;
}
