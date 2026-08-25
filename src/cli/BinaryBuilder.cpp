#include "cli/BinaryBuilder.h"
#include "cli/LucisPipeline.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"
#include "comptime/ComptimeEngine.h"

#include "generated/LucisParser.h"

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include <iostream>
#include <filesystem>
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

static bool linkWithCompiler(const std::string& cc, const std::string& irPath,
                             const std::vector<std::string>& cObjects,
                             const std::string& outBinPath,
                             const BinaryBuildOptions& opts,
                             const PipelineResult& pipeline) {
    pid_t lpid = ::fork();
    if (lpid < 0) return false;
    if (lpid == 0) {
        if (opts.quiet) {
            int devNull = ::open("/dev/null", O_WRONLY);
            if (devNull >= 0) {
                ::dup2(devNull, STDOUT_FILENO);
                ::dup2(devNull, STDERR_FILENO);
                if (devNull > STDERR_FILENO) ::close(devNull);
            }
        }
        std::vector<const char*> argv;
        argv.push_back(cc.c_str());
        argv.push_back(irPath.c_str());

        std::string optArg;
        switch (opts.optLevel) {
            case OptimizationLevel::O1:   optArg = "-O1"; break;
            case OptimizationLevel::O2:   optArg = "-O2"; break;
            case OptimizationLevel::O3:   optArg = "-O3"; break;
            case OptimizationLevel::Os:   optArg = "-Os"; break;
            case OptimizationLevel::Oz:   optArg = "-Oz"; break;
            case OptimizationLevel::Ofast: optArg = "-Ofast"; break;
            default: break;
        }
        if (!optArg.empty()) argv.push_back(optArg.c_str());

        if (opts.useLTO) argv.push_back("-flto");

        for (auto& obj : cObjects)
            argv.push_back(obj.c_str());

        auto builtinsPath = CodeGen::builtinsLibraryPath();
        argv.push_back(builtinsPath.c_str());
#ifdef LUCIS_RUNTIME_DIAGNOSTICS
        argv.push_back("-fsanitize=address,undefined");
        argv.push_back("-fno-omit-frame-pointer");
#endif
        for (auto& lf : pipeline.linkerFlags)
            argv.push_back(lf.c_str());
        argv.push_back("-lm");
        argv.push_back("-lz");
        argv.push_back("-latomic");
        argv.push_back("-lpthread");
        argv.push_back("-o");
        argv.push_back(outBinPath.c_str());
        argv.push_back(nullptr);
        ::execvp(cc.c_str(), const_cast<char**>(argv.data()));
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(lpid, &status, 0) < 0) {
        if (errno != EINTR) return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool BinaryBuilder::build(PipelineResult& pipeline,
                          const BinaryBuildOptions& opts,
                          const std::function<void(IRGen&, const SourceUnit&, bool isEntry)>& customizeIrGen,
                          std::string& outBinaryPath) {
    // ── Generate per-unit IR and link into one module ───────────────────
    std::unique_ptr<llvm::LLVMContext> masterCtx;
    std::unique_ptr<llvm::Module>      masterMod;

    ComptimeEngine comptimeEngine;
    comptimeEngine.setTargetTriple(
        opts.targetTriple.empty()
            ? llvm::sys::getDefaultTargetTriple()
            : opts.targetTriple);
    for (auto& unit : pipeline.units) {
        if (!unit.parseResult || !unit.parseResult->tree) continue;
        for (auto* tld : unit.parseResult->tree->topLevelDecl()) {
            if (auto* func = tld->functionDecl()) {
                if (func->COMPTIME()) {
                    auto name = func->name->getText();
                    comptimeEngine.registry().registerFunction(
                        name, func, func->typeParamList() != nullptr);
                }
            }
        }
    }

    std::string entryCanonical;
    if (!opts.entryFilePath.empty()) {
        std::error_code ec;
        entryCanonical = fs::canonical(fs::path(opts.entryFilePath), ec).string();
    }

    size_t irIdx = 0;
    for (auto& unit : pipeline.units) {
        ++irIdx;
        if (!opts.quiet)
            std::cerr << "lucis: [" << opts.progressStage << " ir " << irIdx << "/"
                      << pipeline.units.size() << "] " << unit.filePath << "\n";

        IRGen irGen;
        irGen.setModuleContext(pipeline.registry.get(), unit.modulePath, unit.filePath, unit.isStdlib);
        irGen.setCBindings(pipeline.cBindings.get());
        irGen.setSemanticDB(pipeline.semanticDB.get());
        irGen.setComptimeEngine(&comptimeEngine);
        irGen.setCompileTimeValues(pipeline.compileTimeValues);
        irGen.setTargetTriple(
            opts.targetTriple.empty()
                ? llvm::sys::getDefaultTargetTriple()
                : opts.targetTriple);
        irGen.setEmitDebugInfo(opts.emitDebugInfo);
        irGen.setNoStd(opts.noStd);
        irGen.setProjectRoot(pipeline.projectRoot);

        bool isEntry = false;
        if (!entryCanonical.empty()) {
            std::error_code ec;
            auto unitCanonical = fs::canonical(fs::path(unit.filePath), ec);
            isEntry = !ec && unitCanonical.string() == entryCanonical;
        }
        if (customizeIrGen)
            customizeIrGen(irGen, unit, isEntry);

        auto irMod = irGen.generate(unit.parseResult->tree, unit.filePath);
        if (!irMod) {
            std::cerr << "lucis: IR generation failed for '" << unit.filePath << "'\n";
            return false;
        }

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
            if (!parsedMod) return false;
            masterMod = std::move(parsedMod);
        } else {
            auto parsedMod = doBitcode(irMod->module(), *masterCtx);
            if (!parsedMod) return false;
            if (llvm::Linker::linkModules(*masterMod, std::move(parsedMod))) {
                std::cerr << "lucis: failed to link module '" << unit.filePath << "'\n";
                return false;
            }
        }
    }

    if (!masterMod) {
        std::cerr << "lucis: no IR generated\n";
        return false;
    }

    if (opts.optLevel != OptimizationLevel::O0) {
        IRModule wrap(std::move(masterCtx), std::move(masterMod));
        Optimizer::optimize(wrap, opts.optLevel);
        masterMod = wrap.takeModule();
        masterCtx = wrap.takeContext();
    }

    // ── Cache and produce the binary ────────────────────────────────────
    const std::string cacheDir = pipeline.projectRoot + "/.lucis/cache";
    if (opts.useClean) {
        if (!opts.quiet)
            std::cerr << "lucis: [" << opts.progressStage << "] clearing cache...";
        std::error_code ec;
        fs::remove_all(cacheDir, ec);
    }
    fs::create_directories(cacheDir);

    std::string irText;
    llvm::raw_string_ostream os(irText);
    masterMod->print(os, nullptr);

    std::string cacheKeyData = irText;
    cacheKeyData += "\n#builtins=" + CodeGen::builtinsLibraryPath();
    cacheKeyData += "\n#opt=" + std::to_string(static_cast<int>(opts.optLevel));
    if (opts.useLTO) cacheKeyData += "\n#lto=1";
    for (auto& lf : pipeline.linkerFlags)
        cacheKeyData += "\n#l=" + lf;

    llvm::MD5 md5;
    md5.update(cacheKeyData);
    llvm::MD5::MD5Result md5Result;
    md5.final(md5Result);
    llvm::SmallString<32> digest;
    llvm::MD5::stringifyResult(md5Result, digest);
    const std::string binPath = cacheDir + "/" + opts.cachePrefix + "-"
                                + std::string(digest.str()) + ".bin";

    bool needBuild = true;
    if (fs::exists(binPath)) {
        std::error_code permsEc;
        auto perms = fs::status(binPath, permsEc).permissions();
        if (!permsEc && (perms & fs::perms::owner_exec) != fs::perms::none)
            needBuild = false;
    }

    if (!needBuild) {
        if (!opts.quiet)
            std::cerr << "lucis: [" << opts.progressStage << "] cache hit: reusing binary\n";
        outBinaryPath = binPath;
        return true;
    }

    if (!opts.quiet)
        std::cerr << "\nlucis: [" << opts.progressStage << "] --- compiler/linker output ---\n\n";

    // Compile extra C sources from #include resolution (same as build/run).
    std::vector<std::string> cObjectFiles;
    for (auto& cSrc : pipeline.cSourceFiles) {
        auto stem = fs::path(cSrc).stem().string();
        std::vector<std::string> cIncFlags;
        cIncFlags.push_back("-I" + fs::path(cSrc).parent_path().string());
        auto objPath = pipeline.buildDir + "/c__" + stem + ".o";
        if (!CodeGen::compileCSource(cSrc, objPath, cIncFlags, opts.quiet)) {
            std::cerr << "lucis: failed to compile C source '" << cSrc << "'\n";
            return false;
        }
        cObjectFiles.push_back(objPath);
    }

    // Emit the master module as an object file.
    std::string runObjTemplate = fs::temp_directory_path().string()
                                 + "/lucis-" + opts.cachePrefix + "-obj-XXXXXX.o";
    std::vector<char> objTmpl(runObjTemplate.begin(), runObjTemplate.end());
    objTmpl.push_back('\0');
    int objfd = ::mkstemps(objTmpl.data(), 2);
    if (objfd < 0) {
        std::cerr << "lucis: could not create temporary object path: "
                  << std::strerror(errno) << "\n";
        return false;
    }
    ::close(objfd);
    const std::string runObjPath(objTmpl.data());
    if (!CodeGen::emitObjectFile(masterMod.get(), runObjPath)) {
        std::cerr << "lucis: failed to emit object file\n";
        fs::remove(runObjPath);
        return false;
    }

    const std::string binTempPath = binPath + ".tmp-" + std::to_string(::getpid());
    fs::remove(binTempPath);
    if (!linkWithCompiler("clang", runObjPath, cObjectFiles, binTempPath, opts, pipeline)
        && !linkWithCompiler("gcc", runObjPath, cObjectFiles, binTempPath, opts, pipeline)) {
        std::cerr << "lucis: linking failed — ensure clang or gcc is installed\n";
        fs::remove(runObjPath);
        return false;
    }
    fs::remove(runObjPath);

    std::error_code renameEc;
    fs::rename(binTempPath, binPath, renameEc);
    if (renameEc) {
        fs::remove(binTempPath);
        if (!fs::exists(binPath)) {
            std::cerr << "lucis: failed to finalize cached binary: "
                      << renameEc.message() << "\n";
            return false;
        }
    }

    outBinaryPath = binPath;
    return true;
}