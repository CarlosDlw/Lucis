#pragma once

#include <memory>
#include <string>
#include <vector>
#include <antlr4-runtime.h>
#include "generated/LucisLexer.h"
#include "generated/LucisParser.h"
#include "lsp/Diagnostic.h"

// Holds the full ANTLR4 pipeline; all objects must outlive the parse tree.
struct ParseResult {
    std::unique_ptr<antlr4::ANTLRInputStream>  input;
    std::unique_ptr<LucisLexer>               lexer;
    std::unique_ptr<antlr4::CommonTokenStream> tokens;
    std::unique_ptr<LucisParser>              parser;
    LucisParser::ProgramContext*              tree      = nullptr;
    LucisParser::ExpressionContext*           exprTree  = nullptr;  // parseExpression()
    bool                                       hasErrors = false;
    std::vector<Diagnostic>                    diagnostics;  // populated in LSP mode
};

class Parser {
public:
    // Parse from a file on disk (CLI mode — errors go to stderr).
    static ParseResult parse(const std::string& filePath);

    // Parse from a string in memory (LSP mode — errors collected in result.diagnostics).
    static ParseResult parseString(const std::string& source);

    // Parse a single expression (LSP mode — for resolving receiver chains).
    static ParseResult parseExpression(const std::string& source);
};
