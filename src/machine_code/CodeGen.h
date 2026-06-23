#pragma once

#include <string>
#include <vector>
#include "LLVM_IR/IRModule.h"

class CodeGen {
public:
    // Emit a native executable from a single module (legacy single-file path).
    static bool emitBinary(IRModule& irModule, const std::string& outputPath);

    // Emit a single LLVM module as an object file.
    static bool emitObjectFile(llvm::Module* module, const std::string& objectPath, bool pic = true,
                               const std::string& targetTriple = "");

    // Emit a single LLVM module as assembly text.
    static bool emitAssembly(llvm::Module* module, const std::string& assemblyPath, bool pic = true,
                             const std::string& targetTriple = "");

    // Emit a single LLVM module as bitcode.
    static bool emitBitcode(llvm::Module* module, const std::string& bitcodePath);

    // Link multiple object files into a single executable.
    static bool linkObjectFiles(const std::vector<std::string>& objectPaths,
                                const std::string& outputPath,
                                const std::vector<std::string>& extraLinkerFlags = {},
                                const std::vector<std::string>& extraLibPaths = {},
                                bool withSanitizers = true,
                                bool quiet = false,
                                const std::string& entryPoint = "");

    // Compile a C source file into an object file using the system C compiler.
    static bool compileCSource(const std::string& cSourcePath,
                               const std::string& objectPath,
                               const std::vector<std::string>& extraIncludePaths = {},
                               bool quiet = false);

    // Locate runtime builtins static library next to the lucis executable.
    static std::string builtinsLibraryPath();

private:
    static bool linkToExecutable(const std::string& objectPath,
                                 const std::string& outputPath);
};
