#include "cli/RunCommand.h"
#include "cli/ArgParser.h"
#include "cli/LuxPipeline.h"
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
    parser.addPositional("file", "Path to the .lx entrypoint file");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, or 3 (default: 0)");
    parser.addFlag("quiet", 'q', "Suppress pipeline logs");
    parser.addFlag("clean", 'c', "Clear cache before compiling");
    parser.addOption("link", 'l', "LIB", "Link against a library (repeatable)", true);
    parser.addOption("lib-path", 'L', "DIR", "Add library search path (repeatable)", true);
    parser.addOption("include", 'I', "DIR", "Add include search path (repeatable)", true);
    // Remaining args after -- are forwarded to the program
}

int RunCommand::run(const ArgParser& parser) {
    LuxPipeline::Options pipeOpts;
    pipeOpts.inputFile        = parser.get("file");
    pipeOpts.quiet            = parser.has("quiet");
    pipeOpts.includePaths     = parser.getAll("include");
    pipeOpts.userLinkerFlags  = parser.getAll("link");

    auto pipeline = LuxPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    // ── Generate IR, link into one module ──────────────────────────────────

    std::unique_ptr<llvm::LLVMContext> masterCtx;
    std::unique_ptr<llvm::Module>      masterMod;

    size_t irIdx = 0;
    for (auto& unit : pipeline->units) {
        ++irIdx;
        if (!pipeOpts.quiet)
            std::cerr << "lux: [run ir " << irIdx << "/" << pipeline->units.size()
                      << "] " << unit.filePath << "\n";

        IRGen irGen;
        irGen.setNamespaceContext(pipeline->registry.get(), unit.namespaceName, unit.filePath);
        irGen.setCBindings(pipeline->cBindings.get());
        auto irMod = irGen.generate(unit.parseResult.tree, unit.filePath);
        if (!irMod) {
            std::cerr << "lux: IR generation failed for '" << unit.filePath << "'\n";
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
                std::cerr << "lux: bitcode re-parse failed for '" << unit.filePath << "'\n";
                std::cerr << "lux: " << llvm::toString(parsed.takeError()) << "\n";
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
                std::cerr << "lux: failed to link module '" << unit.filePath << "'\n";
                return 1;
            }
        }
    }

    if (!masterMod) {
        std::cerr << "lux: no IR generated\n";
        return 1;
    }

    // ── Cache and produce temp binary ──────────────────────────────────────
    const std::string cacheDir = pipeline->projectRoot + "/.lux/cache";
    if (parser.has("clean")) {
        if (!pipeOpts.quiet)
            std::cerr << "lux: [run] clearing cache...";
        std::error_code ec;
        fs::remove_all(cacheDir, ec);
    }
    fs::create_directories(cacheDir);

    std::string irText;
    llvm::raw_string_ostream os(irText);
    masterMod->print(os, nullptr);

    std::string cacheKeyData = irText;
    cacheKeyData += "\n#builtins=" + CodeGen::builtinsLibraryPath();
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

            // Add C objects
            for (auto& obj : cObjects)
                argv.push_back(obj.c_str());

            auto builtinsPath = CodeGen::builtinsLibraryPath();
            argv.push_back(builtinsPath.c_str());
#ifdef LUX_RUNTIME_DIAGNOSTICS
            argv.push_back("-fsanitize=address,undefined");
            argv.push_back("-fno-omit-frame-pointer");
#endif
            for (auto& lf : pipeline->linkerFlags)
                argv.push_back(lf.c_str());
            argv.push_back("-lm");
            argv.push_back("-lz");
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
            std::cerr << "\nlux: [run] --- compiler/linker output ---\n\n";

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
                    std::cerr << "lux: failed to compile C source '" << cSrc << "'\n";
                    return 1;
                }
                cObjectFiles.push_back(objPath);
            }
        }

        std::string runLLTemplate = fs::temp_directory_path().string() + "/lux-run-ir-XXXXXX.ll";
        std::vector<char> llTmpl(runLLTemplate.begin(), runLLTemplate.end());
        llTmpl.push_back('\0');
        int llfd = ::mkstemps(llTmpl.data(), 3);
        if (llfd < 0) {
            std::cerr << "lux: could not create temporary IR path: "
                      << std::strerror(errno) << "\n";
            return 1;
        }
        ::close(llfd);
        const std::string runLLPath(llTmpl.data());
        {
            std::ofstream out(runLLPath, std::ios::binary | std::ios::trunc);
            out << irText;
        }

        const std::string runBinTempPath = runBinPath + ".tmp-" + std::to_string(::getpid());
        fs::remove(runBinTempPath);
        if (!linkWithCompiler("clang", runLLPath, cObjectFiles, runBinTempPath)
            && !linkWithCompiler("gcc", runLLPath, cObjectFiles, runBinTempPath)) {
            std::cerr << "lux: linking failed — ensure clang or gcc is installed\n";
            fs::remove(runLLPath);
            return 1;
        }
        fs::remove(runLLPath);

        std::error_code renameEc;
        fs::rename(runBinTempPath, runBinPath, renameEc);
        if (renameEc) {
            fs::remove(runBinTempPath);
            if (!fs::exists(runBinPath)) {
                std::cerr << "lux: failed to finalize cached run binary: "
                          << renameEc.message() << "\n";
                return 1;
            }
        }
    } else {
        if (!pipeOpts.quiet)
            std::cerr << "lux: [run] cache hit: reusing run binary\n";
    }

    // ── Execute ────────────────────────────────────────────────────────────
    if (!pipeOpts.quiet)
        std::cerr << "\nlux: [run] --- program output ---\n\n";

    pid_t pid = ::fork();
    if (pid < 0) {
        std::cerr << "lux: failed to execute run binary: " << std::strerror(errno) << "\n";
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
            std::cerr << "lux: waitpid failed: " << std::strerror(errno) << "\n";
            return 1;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
