#include "cli/BuildCommand.h"
#include "cli/ArgParser.h"
#include "cli/CliHelpers.h"
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
    parser.addOption("output", 'o', "FILE", "Output binary path (default: <input>.out)");
    parser.addFlag("emit-llvm", '\0', "Emit LLVM IR (.ll) to stdout or -o path");
    parser.addFlag("emit-asm",  '\0', "Emit assembly (.s) to stdout or -o path");
    parser.addFlag("emit-bc",   '\0', "Emit LLVM bitcode (.bc)");
    parser.addFlag("emit-obj",  '\0', "Emit object file (.o)");
    parser.addFlag("recursive", 'r', "Include all modules in emit output (works with --emit-*)");
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

int BuildCommand::run(const ArgParser& parser) {
    auto resolved = resolveInputFile(parser.get("file"));
    if (resolved.filePath.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis build <file>   or   lucis build  (from a project with lucis.yaml)\n";
        return 1;
    }

    bool useConfig = resolved.useConfig;
    auto& cfg = resolved.config;

    // ── Run pre-build scripts (before everything) ─────────────────
    if (useConfig && !cfg->scripts.pre.empty()) {
        for (auto& cmd : cfg->scripts.pre) {
            std::cerr << "lucis: [scripts:pre] " << cmd << "\n";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "lucis: [scripts:pre] command failed with exit code " << ret << "\n";
                return 1;
            }
        }
    }

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile = resolved.filePath;
    pipeOpts.quiet     = parser.has("quiet");
    pipeOpts.includePaths = parser.getAll("include");
    if (useConfig) {
        if (pipeOpts.includePaths.empty())
            pipeOpts.includePaths = cfg->includes;
    }

    pipeOpts.sourcePaths = useConfig ? cfg->sourcePaths : std::vector<std::string>{"src/"};

    pipeOpts.userLinkerFlags = parser.getAll("link");
    if (useConfig && pipeOpts.userLinkerFlags.empty())
        pipeOpts.userLinkerFlags = cfg->linker.libs;

    pipeOpts.binaryName = useConfig ? cfg->binary : "";
    pipeOpts.outDir     = useConfig ? cfg->outDir : "";

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
    else
        usePIC = useConfig ? cfg->build.fpic : true;

    bool useRecursive = parser.has("recursive");
    std::string outputFile = parser.get("output");

    // ── Resolve emit tasks (CLI-only) ─────────────────────────────────────
    struct EmitTask {
        enum Type { LLVM, ASM, BC, OBJ };
        Type type;
        std::string outPath;
    };
    std::vector<EmitTask> tasks;
    bool hasObjEmit = false;
    bool hasTextEmit = false;

    auto addEmitTask = [&](const std::string& key, EmitTask::Type etype) {
        if (!parser.has("emit-" + key)) return;
        EmitTask t;
        t.type = etype;
        if (!outputFile.empty())
            t.outPath = outputFile;
        if (etype == EmitTask::OBJ) hasObjEmit = true;
        else hasTextEmit = true;
        tasks.push_back(t);
    };

    addEmitTask("llvm", EmitTask::LLVM);
    addEmitTask("asm",  EmitTask::ASM);
    addEmitTask("bc",   EmitTask::BC);
    addEmitTask("obj",  EmitTask::OBJ);

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

    // ── Incremental build cache ──────────────────────────────────────────
    bool buildCached = false;
    std::string projRoot = LucisPipeline::getProjectRoot(pipeOpts.inputFile);
    std::string pipelineBuildDir = projRoot + "/.lucis/build";
    std::string cacheManifestPath = pipelineBuildDir + "/cache/build_manifest.txt";

    if (!pipeOpts.quiet)
        std::cerr << "lucis: [build] checking incremental cache\n";

    std::vector<std::string> savedLinkerFlags;
    {
        std::ifstream manifest(cacheManifestPath);
        if (manifest) {
            std::string line;
            bool allMatch = true;
            while (std::getline(manifest, line)) {
                if (line.empty()) continue;
                if (line[0] == 'C' && line.size() > 2 && line[1] == ':') {
                    auto content = line.substr(2);
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
                manifest << "#linkerFlags";
                for (auto& lf : pipeline->linkerFlags)
                    manifest << " " << lf;
                manifest << "\n";
            }
        }
    } else {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(pipelineBuildDir, ec)) {
            if (entry.path().extension() == ".o")
                cachedObjectFiles.push_back(entry.path().string());
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

    // ── Generate IR for all units ───────────────────────────────────────
    std::vector<std::string> objectFiles;

    if (!buildCached) {
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
            irGen.setModuleContext(pipeline->registry.get(), unit.modulePath, unit.filePath, unit.isStdlib);
            irGen.setCBindings(pipeline->cBindings.get());
            irGen.setSemanticDB(pipeline->semanticDB.get());
            auto irMod = irGen.generate(unit.parseResult->tree, unit.filePath);
            if (!irMod) {
                std::cerr << "lucis: IR generation failed for '" << unit.filePath << "'\n";
                anyIRError = true;
                continue;
            }
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

        // ── Execute emit tasks ───────────────────────────────────────
        if (!tasks.empty()) {
            bool anyEmitError = false;
            for (auto& t : tasks) {
                if (t.type == EmitTask::LLVM) {
                    if (useRecursive) {
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
                    if (useRecursive) {
                        auto emitOneAsm = [&](llvm::Module* mod, llvm::raw_fd_ostream& dest) -> bool {
                            char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                            int fd = mkstemps(tmpAsm, 2);
                            if (fd == -1) return false;
                            ::close(fd);
                            if (!CodeGen::emitAssembly(mod, tmpAsm)) { fs::remove(tmpAsm); return false; }
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
                                    if (CodeGen::emitAssembly(uir.mod->module(), tmpAsm)) {
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
                            if (!CodeGen::emitAssembly(mainMod, t.outPath)) anyEmitError = true;
                            else if (!pipeOpts.quiet)
                                std::cout << "lucis: assembly written to '" << t.outPath << "'\n";
                        } else {
                            char tmpAsm[] = "/tmp/lucis-asm-XXXXXX.s";
                            int fd = mkstemps(tmpAsm, 2);
                            if (fd != -1) {
                                ::close(fd);
                                if (CodeGen::emitAssembly(mainMod, tmpAsm)) {
                                    std::ifstream in(tmpAsm);
                                    std::cout << in.rdbuf();
                                } else anyEmitError = true;
                                fs::remove(tmpAsm);
                            } else anyEmitError = true;
                        }
                    }
                } else if (t.type == EmitTask::BC) {
                    if (useRecursive) {
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

                if (useRecursive) {
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
                        if (!CodeGen::emitObjectFile(uir.mod->module(), objPath, usePIC)) {
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
                    if (!CodeGen::emitObjectFile(mainMod, objEmitPath, usePIC)) {
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
        if (useConfig && !cfg->binary.empty()) {
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

    for (auto& arg : parser.getAll("link-arg"))
        finalLinkerFlags.push_back(arg);

    if (parser.has("rpath"))
        finalLinkerFlags.push_back("-Wl,-rpath," + parser.get("rpath"));

    auto libPaths = parser.getAll("lib-path");
    if (useConfig && libPaths.empty())
        libPaths = cfg->linker.libPaths;

    if (!CodeGen::linkObjectFiles(objectFiles, outputFile,
                                    finalLinkerFlags,
                                    libPaths,
                                    !useStatic, pipeOpts.quiet)) {
        std::cerr << "lucis: failed to link binary '" << outputFile << "'\n";
        return 1;
    }

    // ── Run post-build scripts (after everything) ─────────────────
    if (useConfig && !cfg->scripts.pos.empty()) {
        for (auto& cmd : cfg->scripts.pos) {
            std::cerr << "lucis: [scripts:pos] " << cmd << "\n";
            int ret = system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "lucis: [scripts:pos] command failed with exit code " << ret << "\n";
                return 1;
            }
        }
    }

    std::cout << "lucis: binary written to '" << outputFile << "'\n";
    return 0;
}
