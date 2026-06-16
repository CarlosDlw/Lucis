#include "cli/RunCommand.h"
#include "cli/ArgParser.h"
#include "cli/LucisPipeline.h"
#include "config/LucisConfig.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <llvm/Support/MD5.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/MemoryBuffer.h>

namespace fs = std::filesystem;

void RunCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lc entrypoint file (auto-resolved from lucis.yaml if omitted)", false);
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, or fast (default: 0)");
    parser.addFlag("lto", '\0', "Enable Link Time Optimization");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addFlag("clean", 'c', "Clear cache before compiling");
    parser.addOption("link", 'l', "LIB", "Link against a library (repeatable)", true);
    parser.addOption("lib-path", 'L', "DIR", "Add library search path (repeatable)", true);
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    // Remaining args after -- are forwarded to the program
}

std::string RunCommand::resolveInputFile(const ArgParser& parser,
                                          LucisConfig* outConfig) const {
    auto file = parser.get("file");

    // Always load lucis.yaml when available (even with explicit file)
    auto config = LucisConfig::findInDir(fs::current_path().string());
    if (config && outConfig) *outConfig = *config;

    if (!file.empty()) {
        if (!fs::is_directory(file)) return file;
        // Directory was passed — fall through to config-based search
    }

    if (!config) return {};

    // No explicit file — try src/main.lc, then main.lc
    for (auto& candidate : { "src/main.lc", "main.lc" }) {
        auto path = fs::path(candidate);
        if (fs::exists(path))
            return fs::canonical(path).string();
    }
    return {};
}

int RunCommand::run(const ArgParser& parser) {
    LucisConfig config;
    std::string inputFile = resolveInputFile(parser, &config);
    if (inputFile.empty()) {
        std::cerr << "lucis: no input file specified and no lucis.yaml found\n";
        std::cerr << "usage: lucis run <file>   or   lucis run  (from a project with lucis.yaml)\n";
        return 1;
    }

    LucisPipeline::Options pipeOpts;
    pipeOpts.inputFile        = inputFile;
    pipeOpts.quiet            = parser.has("quiet") ? true : config.run.quiet;
    pipeOpts.includePaths     = parser.has("include") ? parser.getAll("include") : config.includes;
    pipeOpts.userLinkerFlags  = parser.has("link") ? parser.getAll("link") : config.linker.libs;
    pipeOpts.binaryName       = config.binary;
    pipeOpts.outDir           = config.outDir;
    pipeOpts.sourcePaths      = config.sourcePaths;

    OptimizationLevel lucisOptLevel = OptimizationLevel::O0;

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
        lucisOptLevel = parseOpt(config.run.optLevel);
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
            } catch (...) {}
        }
    }
    bool useLTO = parser.has("lto") ? true : config.run.lto;
    bool useClean = parser.has("clean") ? true : config.run.clean;

    auto pipeline = LucisPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    // ── Generate IR, link into one module ──────────────────────────────────

    std::unique_ptr<llvm::LLVMContext> masterCtx;
    std::unique_ptr<llvm::Module>      masterMod;

    size_t irIdx = 0;
    for (auto& unit : pipeline->units) {
        ++irIdx;
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [run ir " << irIdx << "/" << pipeline->units.size()
                      << "] " << unit.filePath << "\n";

        IRGen irGen;
        irGen.setModuleContext(pipeline->registry.get(), unit.modulePath, unit.filePath);
        irGen.setCBindings(pipeline->cBindings.get());
        irGen.setSemanticDB(pipeline->semanticDB.get());
        auto irMod = irGen.generate(unit.parseResult.tree, unit.filePath);
        if (!irMod) {
            std::cerr << "lucis: IR generation failed for '" << unit.filePath << "'\n";
            return 1;
        }

        // Serialize to bitcode, re-parse into master context, link
        auto doBitcode = [&](llvm::Module* srcMod, llvm::LLVMContext& targetCtx) {
            llvm::SmallVector<char, 0> buf;
            llvm::raw_svector_ostream os(buf);
            llvm::WriteBitcodeToFile(*srcMod, os);
            auto memBuf = llvm::MemoryBuffer::getMemBufferCopy(
                llvm::StringRef(buf.data(), buf.size()), unit.filePath);
            auto parsed = llvm::parseBitcodeFile(memBuf->getMemBufferRef(), targetCtx);
            if (!parsed) {
                std::cerr << "lucis: bitcode re-parse failed for '" << unit.filePath << "'\n";
                std::cerr << "lucis: " << llvm::toString(parsed.takeError()) << "\n";
                return std::unique_ptr<llvm::Module>(nullptr);
            }
            return std::move(parsed.get());
        };

        if (!masterMod) {
            masterCtx = std::make_unique<llvm::LLVMContext>();
            auto parsedMod = doBitcode(irMod->module(), *masterCtx);
            if (!parsedMod) return 1;
            masterMod = std::move(parsedMod);
        } else {
            auto parsedMod = doBitcode(irMod->module(), *masterCtx);
            if (!parsedMod) return 1;
            if (llvm::Linker::linkModules(*masterMod, std::move(parsedMod))) {
                std::cerr << "lucis: failed to link module '" << unit.filePath << "'\n";
                return 1;
            }
        }
    }

    if (!masterMod) {
        std::cerr << "lucis: no IR generated\n";
        return 1;
    }

    // Optimize master module
    if (lucisOptLevel != OptimizationLevel::O0) {
        IRModule wrap(std::move(masterCtx), std::move(masterMod));
        Optimizer::optimize(wrap, lucisOptLevel);
        masterMod = wrap.takeModule();
        masterCtx = wrap.takeContext();
    }

    // ── Cache and produce temp binary ──────────────────────────────────────
    const std::string cacheDir = pipeline->projectRoot + "/.lucis/cache";
    if (useClean) {
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [run] clearing cache...";
        std::error_code ec;
        fs::remove_all(cacheDir, ec);
    }
    fs::create_directories(cacheDir);

    std::string irText;
    llvm::raw_string_ostream os(irText);
    masterMod->print(os, nullptr);

    std::string cacheKeyData = irText;
    cacheKeyData += "\n#builtins=" + CodeGen::builtinsLibraryPath();
    cacheKeyData += "\n#opt=" + std::to_string(static_cast<int>(lucisOptLevel));
    if (useLTO) cacheKeyData += "\n#lto=1";
    for (auto& lf : pipeline->linkerFlags)
        cacheKeyData += "\n#l=" + lf;

    llvm::MD5 md5;
    md5.update(cacheKeyData);
    llvm::MD5::MD5Result md5Result;
    md5.final(md5Result);
    llvm::SmallString<32> digest;
    llvm::MD5::stringifyResult(md5Result, digest);
    const std::string runBinPath = cacheDir + "/run-" + std::string(digest.str()) + ".bin";

    bool needBuild = true;
    if (fs::exists(runBinPath)) {
        std::error_code permsEc;
        auto perms = fs::status(runBinPath, permsEc).permissions();
        if (!permsEc && (perms & fs::perms::owner_exec) != fs::perms::none)
            needBuild = false;
    }

    auto linkWithCompiler = [&](const char* cc, const std::string& irPath,
                                 const std::vector<std::string>& cObjects,
                                 const std::string& outBinPath) -> bool {
        pid_t lpid = ::fork();
        if (lpid < 0) return false;
        if (lpid == 0) {
            if (pipeOpts.quiet) {
                int devNull = ::open("/dev/null", O_WRONLY);
                if (devNull >= 0) {
                    ::dup2(devNull, STDOUT_FILENO);
                    ::dup2(devNull, STDERR_FILENO);
                    if (devNull > STDERR_FILENO) ::close(devNull);
                }
            }
            std::vector<const char*> argv;
            argv.push_back(cc);
            argv.push_back(irPath.c_str());

            // Add optimization flag
            std::string optArg;
            if (lucisOptLevel == OptimizationLevel::O1) optArg = "-O1";
            else if (lucisOptLevel == OptimizationLevel::O2) optArg = "-O2";
            else if (lucisOptLevel == OptimizationLevel::O3) optArg = "-O3";
            else if (lucisOptLevel == OptimizationLevel::Os) optArg = "-Os";
            else if (lucisOptLevel == OptimizationLevel::Oz) optArg = "-Oz";
            else if (lucisOptLevel == OptimizationLevel::Ofast) optArg = "-Ofast";
            if (!optArg.empty()) argv.push_back(optArg.c_str());

            if (useLTO) argv.push_back("-flto");

            // Add C objects
            for (auto& obj : cObjects)
                argv.push_back(obj.c_str());

            auto builtinsPath = CodeGen::builtinsLibraryPath();
            argv.push_back(builtinsPath.c_str());
#ifdef LUCIS_RUNTIME_DIAGNOSTICS
            argv.push_back("-fsanitize=address,undefined");
            argv.push_back("-fno-omit-frame-pointer");
#endif
            for (auto& lf : pipeline->linkerFlags)
                argv.push_back(lf.c_str());
            argv.push_back("-lm");
            argv.push_back("-lz");
            argv.push_back("-latomic");
            argv.push_back("-lpthread");
            argv.push_back("-o");
            argv.push_back(outBinPath.c_str());
            argv.push_back(nullptr);
            ::execvp(cc, const_cast<char**>(argv.data()));
            ::_exit(127);
        }
        int lstatus = 0;
        while (::waitpid(lpid, &lstatus, 0) < 0) {
            if (errno != EINTR) return false;
        }
        return WIFEXITED(lstatus) && WEXITSTATUS(lstatus) == 0;
    };

    if (needBuild) {
        if (!pipeOpts.quiet)
            std::cerr << "\nlucis: [run] --- compiler/linker output ---\n\n";

        // ── Compile C sources if any ───────────────────────────────────────
        std::vector<std::string> cObjectFiles;
        if (!pipeline->cSourceFiles.empty()) {
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

        // Emit object file directly via LLVM backend, then link with clang.
        // Using .ll + clang leads to incorrect va_arg lowering on x86-64.
        std::string runObjTemplate = fs::temp_directory_path().string() + "/lucis-run-obj-XXXXXX.o";
        std::vector<char> objTmpl(runObjTemplate.begin(), runObjTemplate.end());
        objTmpl.push_back('\0');
        int objfd = ::mkstemps(objTmpl.data(), 2);
        if (objfd < 0) {
            std::cerr << "lucis: could not create temporary object path: "
                      << std::strerror(errno) << "\n";
            return 1;
        }
        ::close(objfd);
        const std::string runObjPath(objTmpl.data());

        if (!CodeGen::emitObjectFile(masterMod.get(), runObjPath)) {
            std::cerr << "lucis: failed to emit object file\n";
            fs::remove(runObjPath);
            return 1;
        }

        const std::string runBinTempPath = runBinPath + ".tmp-" + std::to_string(::getpid());
        fs::remove(runBinTempPath);
        if (!linkWithCompiler("clang", runObjPath, cObjectFiles, runBinTempPath)
            && !linkWithCompiler("gcc", runObjPath, cObjectFiles, runBinTempPath)) {
            std::cerr << "lucis: linking failed — ensure clang or gcc is installed\n";
            fs::remove(runObjPath);
            return 1;
        }
        fs::remove(runObjPath);

        std::error_code renameEc;
        fs::rename(runBinTempPath, runBinPath, renameEc);
        if (renameEc) {
            fs::remove(runBinTempPath);
            if (!fs::exists(runBinPath)) {
                std::cerr << "lucis: failed to finalize cached run binary: "
                          << renameEc.message() << "\n";
                return 1;
            }
        }
    } else {
        if (!pipeOpts.quiet)
            std::cerr << "lucis: [run] cache hit: reusing run binary\n";
    }

    // ── Execute ────────────────────────────────────────────────────────────
    if (!pipeOpts.quiet)
        std::cerr << "\nlucis: [run] --- program output ---\n\n";

    pid_t pid = ::fork();
    if (pid < 0) {
        std::cerr << "lucis: failed to execute run binary: " << std::strerror(errno) << "\n";
        return 1;
    }

    if (pid == 0) {
        std::vector<char*> execArgv;
        execArgv.push_back(const_cast<char*>(runBinPath.c_str()));
        for (auto& arg : parser.remaining())
            execArgv.push_back(const_cast<char*>(arg.c_str()));
        execArgv.push_back(nullptr);
        ::execv(runBinPath.c_str(), execArgv.data());
        ::_exit(127);
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            std::cerr << "lucis: waitpid failed: " << std::strerror(errno) << "\n";
            return 1;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
