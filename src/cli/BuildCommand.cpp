#include "cli/BuildCommand.h"
#include "cli/ArgParser.h"
#include "cli/LuxPipeline.h"
#include "IRBuilder/IRGen.h"
#include "LLVM_IR/IRModule.h"
#include "LLVM_Optimizer/Optimizer.h"
#include "machine_code/CodeGen.h"

#include <iostream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

void BuildCommand::buildArgs(ArgParser& parser) const {
    parser.addPositional("file", "Path to the .lx entrypoint file");
    parser.addOption("output", 'o', "FILE", "Output binary path (default: <input>.out)");
    parser.addFlag("emit-llvm", '\0', "Print LLVM IR to stdout and exit");
    parser.addOption("opt", 'O', "LEVEL", "Optimization level: 0, 1, 2, 3, s, z, or fast (default: 0)");
    parser.addFlag("lto", '\0', "Enable Link Time Optimization");
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
    bool useLTO       = parser.has("lto");

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
    bool anyIRError = false;
    size_t irIdx = 0;

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

        // Show IR mode: print only the main file's IR
        if (emitLLVM) {
            auto mainPath = fs::canonical(fs::path(pipeOpts.inputFile)).string();
            if (unit.filePath == mainPath)
                irModule->print();
            continue;
        }

        // Optimize
        if (luxOptLevel != OptimizationLevel::O0)
            Optimizer::optimize(*irModule, luxOptLevel);

        // Emit object file
        auto stem = unit.namespaceName + "__" + fs::path(unit.filePath).stem().string();
        auto objPath = pipeline->buildDir + "/" + stem + ".o";
        if (!CodeGen::emitObjectFile(irModule->module(), objPath)) {
            std::cerr << "lux: failed to emit object for '" << unit.filePath << "'\n";
            anyIRError = true;
            continue;
        }
        objectFiles.push_back(objPath);
    }

    if (anyIRError) return 1;
    if (emitLLVM) return 0;

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

    if (!CodeGen::linkObjectFiles(objectFiles, outputFile,
                                   finalLinkerFlags,
                                   parser.getAll("lib-path"),
                                   true, pipeOpts.quiet)) {
        std::cerr << "lux: failed to link binary '" << outputFile << "'\n";
        return 1;
    }
    std::cout << "lux: binary written to '" << outputFile << "'\n";
    return 0;
}
