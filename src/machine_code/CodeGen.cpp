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
#  define LUX_CGFT_OBJECT llvm::CodeGenFileType::ObjectFile
#else
#  define LUX_CGFT_OBJECT llvm::CGFT_ObjectFile
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
                                      int)
    -> decltype(target->createTargetMachine(triple, cpu, features, opt,
                                            llvm::Reloc::PIC_)) {
    return target->createTargetMachine(triple, cpu, features, opt,
                                       llvm::Reloc::PIC_);
}

template <typename TargetT>
static auto createTargetMachineCompat(TargetT* target,
                                      const llvm::Triple& triple,
                                      llvm::StringRef cpu,
                                      llvm::StringRef features,
                                      const llvm::TargetOptions& opt,
                                      long)
    -> decltype(target->createTargetMachine(llvm::StringRef(), cpu, features,
                                            opt, llvm::Reloc::PIC_)) {
    auto tripleStr = triple.str();
    return target->createTargetMachine(llvm::StringRef(tripleStr), cpu, features,
                                       opt, llvm::Reloc::PIC_);
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
#ifdef LUX_RUNTIME_DIAGNOSTICS
        argv.push_back("-fsanitize=address,undefined");
        argv.push_back("-fno-omit-frame-pointer");
#endif
        argv.push_back("-lm");
        argv.push_back("-lz");
        argv.push_back("-lpthread");
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
        // Build argv: linker obj1 obj2 ... builtins -Lpath... -lm -lz -lpthread -lfoo... -o out
        std::vector<const char*> argv;
        argv.push_back(linker);
        for (auto& obj : objectPaths)
            argv.push_back(obj.c_str());
        argv.push_back(builtinsPath.c_str());
        if (withSanitizers) {
#ifdef LUX_RUNTIME_DIAGNOSTICS
            argv.push_back("-fsanitize=address,undefined");
            argv.push_back("-fno-omit-frame-pointer");
#endif
        }
        for (auto& lp : extraLibPaths)
            argv.push_back(lp.c_str());
        argv.push_back("-lm");
        argv.push_back("-lz");
        argv.push_back("-lpthread");
        for (auto& lf : extraLinkerFlags)
            argv.push_back(lf.c_str());
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

// Locate the builtins static library next to the lux executable.
static std::string findBuiltinsPath() {
    char selfPath[4096];
    ssize_t len = ::readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
    if (len > 0) {
        selfPath[len] = '\0';
        std::string dir(selfPath);
        dir = dir.substr(0, dir.rfind('/'));
        return dir + "/liblux_builtins.a";
    }
    return "liblux_builtins.a"; // fallback
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

    std::cerr << "lux: failed to compile C source '"
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
        std::cerr << "lux: linking failed — ensure clang or gcc is installed\n";
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
        std::cerr << "lux: linking failed — ensure clang or gcc is installed\n";
    }
    return linked;
}

// ── Private helpers ──────────────────────────────────────────────────────────

bool CodeGen::emitObjectFile(llvm::Module* module, const std::string& objectPath) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    llvm::Triple targetTriple(llvm::sys::getDefaultTargetTriple());

    std::string lookupError;
    const auto* target = llvm::TargetRegistry::lookupTarget(
        targetTriple.getArchName(), targetTriple, lookupError);
    if (!target) {
        std::cerr << "lux: target lookup failed: " << lookupError << "\n";
        return false;
    }

    auto cpu      = llvm::sys::getHostCPUName();
    auto features = llvm::StringRef("");
    llvm::TargetOptions opt;
    std::unique_ptr<llvm::TargetMachine> machine(
        createTargetMachineCompat(target, targetTriple, cpu, features, opt, 0));

    module->setDataLayout(machine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(objectPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "lux: could not open '" << objectPath << "': "
                  << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager passManager;
    if (machine->addPassesToEmitFile(passManager, dest, nullptr, LUX_CGFT_OBJECT)) {
        std::cerr << "lux: target machine cannot emit object files\n";
        return false;
    }

    passManager.run(*module);
    dest.flush();
    return true;
}
