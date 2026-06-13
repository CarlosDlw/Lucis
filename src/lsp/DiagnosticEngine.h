#pragma once

#include <string>
#include <vector>
#include "lsp/Diagnostic.h"
#include "parser/Parser.h"

class ProjectContext;

// Runs the full parse + semantic check pipeline on a source string
// and produces structured diagnostics suitable for LSP.
class DiagnosticEngine {
public:
    // Run diagnostics with full project context (cross-file resolution).
    std::vector<Diagnostic> run(const std::string& source,
                                const std::string& filePath,
                                const ProjectContext* project,
                                ParseResult* preParsed = nullptr);

    // Run diagnostics in single-file mode (no project context).
    std::vector<Diagnostic> run(const std::string& source,
                                const std::string& filePath,
                                ParseResult* preParsed = nullptr);
};
