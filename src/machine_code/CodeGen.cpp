#include "machine_code/CodeGen.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/IR/LegacyPassManager.h>

// getDefaultTargetTriple / getHostCPUName moved in LLVM 15
#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

// CodeGenFileType enum was renamed in LLVM 18
#ifdef LLVM_VERSION_18_OR_NEWER
#  define LUCIS_CGFT_OBJECT llvm::CodeGenFileType::ObjectFile
#else
#  define LUCIS_CGFT_OBJECT llvm::CGFT_ObjectFile
#endif

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

// ── Internal helpers ─────────────────────────────────────────────────────────

template <typename TargetT>
static auto createTargetMachineCompat(TargetT* target,
                                      const llvm::Triple& triple,
                                      llvm::StringRef cpu,
                                      llvm::StringRef features,
                                      const llvm::TargetOptions& opt,
                                      llvm::Reloc::Model reloc,
                                      int)
    -> decltype(target->createTargetMachine(triple, cpu, features, opt, reloc)) {
    return target->createTargetMachine(triple, cpu, features, opt, reloc);
}

template <typename TargetT>
static auto createTargetMachineCompat(TargetT* target,
                                      const llvm::Triple& triple,
                                      llvm::StringRef cpu,
                                      llvm::StringRef features,
                                      const llvm::TargetOptions& opt,
                                      llvm::Reloc::Model reloc,
                                      long)
    -> decltype(target->createTargetMachine(llvm::StringRef(), cpu, features, opt, reloc)) {
    auto tripleStr = triple.str();
    return target->createTargetMachine(llvm::StringRef(tripleStr), cpu, features, opt, reloc);
}

static auto lookupTargetCompat(const llvm::Triple& triple,
                               std::string& lookupError,
                               int)
    -> decltype(llvm::TargetRegistry::lookupTarget(triple, lookupError)) {
    return llvm::TargetRegistry::lookupTarget(triple, lookupError);
}

static auto lookupTargetCompat(const llvm::Triple& triple,
                               std::string& lookupError,
                               long)
    -> decltype(llvm::TargetRegistry::lookupTarget(llvm::StringRef(), lookupError)) {
    auto tripleStr = triple.str();
    return llvm::TargetRegistry::lookupTarget(llvm::StringRef(tripleStr), lookupError);
}

template <typename ModuleT>
static auto setModuleTargetTripleCompat(ModuleT* module,
                                        const llvm::Triple& triple,
                                        int)
    -> decltype(module->setTargetTriple(triple), void()) {
    module->setTargetTriple(triple);
}

template <typename ModuleT>
static auto setModuleTargetTripleCompat(ModuleT* module,
                                        const llvm::Triple& triple,
                                        long)
    -> decltype(module->setTargetTriple(triple.str()), void()) {
    auto tripleStr = triple.str();
    module->setTargetTriple(tripleStr);
}

// Spawn `linker objectPath builtinsPath -o outputPath` and wait for it.
// Returns true only if the child exits with code 0.
static bool tryLink(const char*        linker,
                    const std::string& objectPath,
                    const std::string& builtinsPath,
                    const std::string& outputPath,
                    bool quiet) {
    pid_t pid = ::fork();
    if (pid < 0) return false;

    if (pid == 0) {
        if (quiet) {
            int devNull = ::open("/dev/null", O_WRONLY);
            if (devNull >= 0) {
                ::dup2(devNull, STDOUT_FILENO);
                ::dup2(devNull, STDERR_FILENO);
                if (devNull > STDERR_FILENO) ::close(devNull);
            }
        }
        // Child process
        std::vector<const char*> argv;
        argv.push_back(linker);
        argv.push_back(objectPath.c_str());
        argv.push_back(builtinsPath.c_str());
#ifdef LUCIS_RUNTIME_DIAGNOSTICS
        argv.push_back("-fsanitize=address,undefined");
        argv.push_back("-fno-omit-frame-pointer");
#endif
        argv.push_back("-lm");
        argv.push_back("-lz");
        argv.push_back("-latomic");
        argv.push_back("-lpthread");
        if (std::string(linker) == "clang")
            argv.push_back("-Wno-override-module");
        argv.push_back("-o");
        argv.push_back(outputPath.c_str());
        argv.push_back(nullptr);
        ::execvp(linker, const_cast<char**>(argv.data()));
        ::_exit(127); // exec failed
    }

    int status = 0;
    ::waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Spawn a linker with a list of object files + builtins.
static bool tryLinkMulti(const char*                      linker,
                          const std::vector<std::string>& objectPaths,
                          const std::string&              builtinsPath,
                          const std::string&              outputPath,
                          const std::vector<std::string>& extraLinkerFlags,
                          const std::vector<std::string>& extraLibPaths,
                          bool withSanitizers,
                          bool quiet) {
    bool isStatic = false;
    for (const auto& flag : extraLinkerFlags) {
        if (flag == "-static") {
            isStatic = true;
            break;
        }
    }

    pid_t pid = ::fork();
    if (pid < 0) return false;

    if (pid == 0) {
        if (quiet) {
            int devNull = ::open("/dev/null", O_WRONLY);
            if (devNull >= 0) {
                ::dup2(devNull, STDOUT_FILENO);
                ::dup2(devNull, STDERR_FILENO);
                if (devNull > STDERR_FILENO) ::close(devNull);
            }
        }
        
        std::vector<const char*> argv;
        argv.push_back(linker);

        // Add object files
        for (auto& obj : objectPaths)
            argv.push_back(obj.c_str());
        
        argv.push_back(builtinsPath.c_str());

        if (withSanitizers) {
#ifdef LUCIS_RUNTIME_DIAGNOSTICS
            argv.push_back("-fsanitize=address,undefined");
            argv.push_back("-fno-omit-frame-pointer");
#endif
        }

        for (auto& lp : extraLibPaths) {
            argv.push_back("-L");
            argv.push_back(lp.c_str());
        }

        // Add libraries
        if (isStatic) {
            argv.push_back("-Wl,--start-group");
            argv.push_back("-lm");
            argv.push_back("-lz");
            argv.push_back("-lpthread");
            argv.push_back("-lc");
            argv.push_back("-Wl,--end-group");
        } else {
            argv.push_back("-lm");
            argv.push_back("-lz");
            argv.push_back("-lpthread");
        }

        for (auto& lf : extraLinkerFlags)
            argv.push_back(lf.c_str());

        if (std::string(linker) == "clang")
            argv.push_back("-Wno-override-module");
        
        argv.push_back("-o");
        argv.push_back(outputPath.c_str());
        argv.push_back(nullptr);
        
        ::execvp(linker, const_cast<char**>(argv.data()));
        ::_exit(127);
    }

    int status = 0;
    ::waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Locate the builtins static library.
static std::string findBuiltinsPath() {
    char selfPath[4096];
    ssize_t len = ::readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
    if (len <= 0) return "liblucis_builtins.a";

    selfPath[len] = '\0';
    std::string binPath(selfPath);
    std::string binDir = binPath.substr(0, binPath.rfind('/'));

    // Potential locations for the builtins library
    std::vector<std::string> candidates = {
        binDir + "/liblucis_builtins.a",
        binDir + "/../lib/liblucis_builtins.a",
        binDir + "/../lib64/liblucis_builtins.a"
    };

    for (const auto& candidate : candidates) {
        if (llvm::sys::fs::exists(candidate)) {
            return candidate;
        }
    }

    return binDir + "/liblucis_builtins.a"; // Final fallback
}

// ── Public API ───────────────────────────────────────────────────────────────

bool CodeGen::compileCSource(const std::string& cSourcePath,
                              const std::string& objectPath,
                              const std::vector<std::string>& extraIncludePaths,
                              bool quiet) {
    const char* compilers[] = { "cc", "clang", "gcc" };

    for (auto* cc : compilers) {
        pid_t pid = ::fork();
        if (pid < 0) return false;

        if (pid == 0) {
            if (quiet) {
                int devNull = ::open("/dev/null", O_WRONLY);
                if (devNull >= 0) {
                    ::dup2(devNull, STDOUT_FILENO);
                    ::dup2(devNull, STDERR_FILENO);
                    if (devNull > STDERR_FILENO) ::close(devNull);
                }
            }
            std::vector<const char*> argv;
            argv.push_back(cc);
            argv.push_back("-c");
            argv.push_back(cSourcePath.c_str());

            for (auto& ip : extraIncludePaths)
                argv.push_back(ip.c_str());

            argv.push_back("-o");
            argv.push_back(objectPath.c_str());
            argv.push_back(nullptr);

            ::execvp(cc, const_cast<char**>(argv.data()));
            ::_exit(127);
        }

        int status = 0;
        ::waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
    }

    std::cerr << "lucis: failed to compile C source '"
              << cSourcePath << "' — ensure cc, clang or gcc is installed\n";
    return false;
}

std::string CodeGen::builtinsLibraryPath() {
    return findBuiltinsPath();
}

bool CodeGen::emitBinary(IRModule& irModule, const std::string& outputPath) {
    const std::string objectPath = outputPath + ".o";

    if (!emitObjectFile(irModule.module(), objectPath)) {
        return false;
    }

    auto builtinsPath = findBuiltinsPath();

    bool linked = tryLink("clang", objectPath, builtinsPath, outputPath, false)
               || tryLink("gcc",   objectPath, builtinsPath, outputPath, false);

    llvm::sys::fs::remove(objectPath);

    if (!linked) {
        std::cerr << "lucis: linking failed — ensure clang or gcc is installed\n";
    }
    return linked;
}

bool CodeGen::linkObjectFiles(const std::vector<std::string>& objectPaths,
                               const std::string& outputPath,
                               const std::vector<std::string>& extraLinkerFlags,
                               const std::vector<std::string>& extraLibPaths,
                               bool withSanitizers,
                               bool quiet) {
    auto builtinsPath = findBuiltinsPath();

    bool linked = tryLinkMulti("clang", objectPaths, builtinsPath, outputPath,
                               extraLinkerFlags, extraLibPaths, withSanitizers, quiet)
               || tryLinkMulti("gcc",   objectPaths, builtinsPath, outputPath,
                               extraLinkerFlags, extraLibPaths, withSanitizers, quiet);

    if (!linked) {
        std::cerr << "lucis: linking failed — ensure clang or gcc is installed\n";
    }
    return linked;
}

// ── Private helpers ──────────────────────────────────────────────────────────

static bool emitToFile(llvm::Module* module, const std::string& outputPath, llvm::CodeGenFileType fileType, bool pic) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());

    std::string lookupError;
    const auto* target = lookupTargetCompat(targetTriple, lookupError, 0);
    if (!target) {
        std::cerr << "lucis: target lookup failed: " << lookupError << "\n";
        return false;
    }

    auto cpu      = llvm::sys::getHostCPUName();
    auto features = llvm::StringRef("");
    llvm::TargetOptions opt;
    auto reloc = pic ? llvm::Reloc::PIC_ : llvm::Reloc::Static;
    std::unique_ptr<llvm::TargetMachine> machine(
        createTargetMachineCompat(target, targetTriple, cpu, features, opt, reloc, 0));

    setModuleTargetTripleCompat(module, targetTriple, 0);
    module->setDataLayout(machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "lucis: could not open '" << outputPath << "': "
                  << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager passManager;
    if (machine->addPassesToEmitFile(passManager, dest, nullptr, fileType)) {
        std::cerr << "lucis: target machine cannot emit this file type\n";
        return false;
    }

    passManager.run(*module);
    dest.flush();
    return true;
}

bool CodeGen::emitObjectFile(llvm::Module* module, const std::string& objectPath, bool pic) {
    return emitToFile(module, objectPath, LUCIS_CGFT_OBJECT, pic);
}

bool CodeGen::emitAssembly(llvm::Module* module, const std::string& assemblyPath, bool pic) {
#ifdef LLVM_VERSION_18_OR_NEWER
    auto type = llvm::CodeGenFileType::AssemblyFile;
#else
    auto type = llvm::CGFT_AssemblyFile;
#endif
    return emitToFile(module, assemblyPath, type, pic);
}

#include <llvm/Bitcode/BitcodeWriter.h>

bool CodeGen::emitBitcode(llvm::Module* module, const std::string& bitcodePath) {
    std::error_code ec;
    llvm::raw_fd_ostream dest(bitcodePath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "lucis: could not open '" << bitcodePath << "': "
                  << ec.message() << "\n";
        return false;
    }
    llvm::WriteBitcodeToFile(*module, dest);
    dest.flush();
    return true;
}
