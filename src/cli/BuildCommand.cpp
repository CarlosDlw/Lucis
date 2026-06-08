#include "cli/BuildCommand.h"
#include "cli/ArgParser.h"
#include "cli/LuxPipeline.h"
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
    parser.addPositional("file", "Path to the .lx entrypoint file");
    parser.addOption("output", 'o', "FILE", "Output binary path (default: <input>.out)");
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

int BuildCommand::run(const ArgParser& parser) {
    LuxPipeline::Options pipeOpts;
    pipeOpts.inputFile        = parser.get("file");
    pipeOpts.quiet            = parser.has("quiet");
    pipeOpts.includePaths     = parser.getAll("include");
    pipeOpts.userLinkerFlags  = parser.getAll("link");

    OptimizationLevel luxOptLevel = OptimizationLevel::O0;
    if (parser.has("opt")) {
        std::string optStr = parser.get("opt");
        if (optStr == "0")      luxOptLevel = OptimizationLevel::O0;
        else if (optStr == "1") luxOptLevel = OptimizationLevel::O1;
        else if (optStr == "2") luxOptLevel = OptimizationLevel::O2;
        else if (optStr == "3") luxOptLevel = OptimizationLevel::O3;
        else if (optStr == "s") luxOptLevel = OptimizationLevel::Os;
        else if (optStr == "z") luxOptLevel = OptimizationLevel::Oz;
        else if (optStr == "fast") luxOptLevel = OptimizationLevel::Ofast;
        else {
            try {
                int level = std::stoi(optStr);
                if (level >= 0 && level <= 3)
                    luxOptLevel = static_cast<OptimizationLevel>(level);
            } catch (...) {
                // Fallback to O0 or log warning if needed
            }
        }
    }

    std::string outputFile = parser.get("output");
    bool emitLLVM     = parser.has("emit-llvm");
    bool emitAsm      = parser.has("emit-asm");
    bool emitBc       = parser.has("emit-bc");
    bool emitObj      = parser.has("emit-obj");
    bool isEmitMode   = emitLLVM || emitAsm || emitBc || emitObj;
    bool useLTO       = parser.has("lto");
    bool useStatic    = parser.has("static");
    bool useShared    = parser.has("shared");
    bool usePIC       = parser.has("fPIC") || useShared;

    auto pipeline = LuxPipeline::run(pipeOpts);
    if (!pipeline || pipeline->hasErrors) return 1;

    // ── Compile C sources ──────────────────────────────────────────────────
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

    // ── Generate IR, optimize, emit objects ────────────────────────────────
    std::vector<std::string> objectFiles;
    
    // Add positional .o files
    for (auto& arg : parser.remaining()) {
        if (fs::path(arg).extension() == ".o") {
            objectFiles.push_back(fs::canonical(arg).string());
        } else {
            std::cerr << "lux: unexpected argument '" << arg << "'\n";
            return 1;
        }
    }

    bool anyIRError = false;
    size_t irIdx = 0;

    auto mainPath = fs::canonical(fs::path(pipeOpts.inputFile)).string();

    for (auto& unit : pipeline->units) {
        ++irIdx;
        if (!pipeOpts.quiet)
            std::cerr << "lux: [build ir " << irIdx << "/" << pipeline->units.size()
                      << "] " << unit.filePath << "\n";

        IRGen irGen;
        irGen.setNamespaceContext(pipeline->registry.get(), unit.namespaceName, unit.filePath);
        irGen.setCBindings(pipeline->cBindings.get());
        auto irModule = irGen.generate(unit.parseResult.tree, unit.filePath);
        if (!irModule) {
            std::cerr << "lux: IR generation failed for '" << unit.filePath << "'\n";
            anyIRError = true;
            continue;
        }

        // Optimize
        if (luxOptLevel != OptimizationLevel::O0)
            Optimizer::optimize(*irModule, luxOptLevel);

        // Handle Emit modes (LLVM, ASM, BC) - only for the main file
        if (isEmitMode && unit.filePath == mainPath) {
            if (emitLLVM) {
                if (!outputFile.empty()) {
                    std::error_code ec;
                    llvm::raw_fd_ostream dest(outputFile, ec, llvm::sys::fs::OF_None);
                    if (ec) {
                        std::cerr << "lux: could not open '" << outputFile << "': " << ec.message() << "\n";
                        anyIRError = true;
                    } else {
                        irModule->module()->print(dest, nullptr);
                        if (!pipeOpts.quiet) std::cout << "lux: LLVM IR written to '" << outputFile << "'\n";
                    }
                } else {
                    irModule->print();
                }
            } else if (emitAsm) {
                if (!outputFile.empty()) {
                    if (CodeGen::emitAssembly(irModule->module(), outputFile)) {
                        if (!pipeOpts.quiet) std::cout << "lux: assembly written to '" << outputFile << "'\n";
                    } else anyIRError = true;
                } else {
                    // To stdout
                    char tmpAsm[] = "/tmp/lux-asm-XXXXXX.s";
                    int fd = mkstemps(tmpAsm, 2);
                    if (fd != -1) {
                        ::close(fd);
                        if (CodeGen::emitAssembly(irModule->module(), tmpAsm)) {
                            std::ifstream in(tmpAsm);
                            std::cout << in.rdbuf();
                            fs::remove(tmpAsm);
                        } else anyIRError = true;
                    } else anyIRError = true;
                }
            } else if (emitBc) {
                if (!outputFile.empty()) {
                    if (CodeGen::emitBitcode(irModule->module(), outputFile)) {
                        if (!pipeOpts.quiet) std::cout << "lux: bitcode written to '" << outputFile << "'\n";
                    } else anyIRError = true;
                } else {
                    // Bitcode to stdout
                    llvm::WriteBitcodeToFile(*irModule->module(), llvm::outs());
                }
            } else if (emitObj) {
                // If emitObj, we just need to continue and the normal object emit will handle it, 
                // but we need to stop before linking.
            }

            if (anyIRError) continue;
            if (!emitObj) continue; // Skip object generation if not emitObj
        }

        // Emit object file (normal path or --emit-obj)
        auto stem = unit.namespaceName + "__" + fs::path(unit.filePath).stem().string();
        auto objPath = (emitObj && unit.filePath == mainPath && !outputFile.empty()) 
                       ? outputFile 
                       : pipeline->buildDir + "/" + stem + ".o";

        if (!CodeGen::emitObjectFile(irModule->module(), objPath, usePIC)) {
            std::cerr << "lux: failed to emit object for '" << unit.filePath << "'\n";
            anyIRError = true;
            continue;
        }

        if (emitObj && unit.filePath == mainPath) {
            if (!pipeOpts.quiet) std::cout << "lux: object file written to '" << objPath << "'\n";
            continue; 
        }

        objectFiles.push_back(objPath);
    }

    if (anyIRError) return 1;
    if (isEmitMode && !emitObj) return 0;
    if (emitObj) return 0;

    // Append C objects
    objectFiles.insert(objectFiles.end(), cObjectFiles.begin(), cObjectFiles.end());

    // ── Link ────────────────────────────────────────────────────────────────
    if (outputFile.empty()) {
        auto inPath = fs::path(pipeOpts.inputFile);
        outputFile = inPath.stem().string() + ".out";
        if (!pipeOpts.quiet)
            std::cerr << "lux: [build] no -o given, writing to '" << outputFile << "'\n";
    }
    if (!pipeOpts.quiet)
        std::cerr << "\nlux: [build] --- linker output ---\n\n";

    std::vector<std::string> finalLinkerFlags = pipeline->linkerFlags;
    if (useLTO) {
        finalLinkerFlags.push_back("-flto");
    }
    if (useStatic) {
        finalLinkerFlags.push_back("-static");
    }
    if (useShared) {
        finalLinkerFlags.push_back("-shared");
    }
    
    for (auto& arg : parser.getAll("link-arg")) {
        finalLinkerFlags.push_back(arg);
    }
    if (parser.has("rpath")) {
        finalLinkerFlags.push_back("-Wl,-rpath," + parser.get("rpath"));
    }

    if (!CodeGen::linkObjectFiles(objectFiles, outputFile,
                                   finalLinkerFlags,
                                   parser.getAll("lib-path"),
                                   !useStatic, pipeOpts.quiet)) {
        std::cerr << "lux: failed to link binary '" << outputFile << "'\n";
        return 1;
    }
    std::cout << "lux: binary written to '" << outputFile << "'\n";
    return 0;
}
