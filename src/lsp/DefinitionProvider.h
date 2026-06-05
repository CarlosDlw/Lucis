#pragma once

#include <string>
#include <optional>
#include <unordered_map>

#include "generated/LuxParser.h"
#include "ffi/CBindings.h"

struct ParseResult;
class ProjectContext;

// Result of a go-to-definition query — target location.
struct DefinitionResult {
    std::string uri;       // file URI (file:///path/to/file.lx)
    size_t line    = 0;    // 0-based
    size_t col     = 0;    // 0-based
    size_t endLine = 0;    // 0-based
    size_t endCol  = 0;    // 0-based
};

// Provides go-to-definition for Lux source code.
// Parses the document, resolves symbols, and returns target locations.
class DefinitionProvider {
public:
    // Returns definition location for the given 0-based line:col.
    std::optional<DefinitionResult> definition(
        const std::string& source, size_t line, size_t col,
        const std::string& filePath,
        const ProjectContext* project,
        ParseResult* preParsed = nullptr);

    // Lightweight variable info (reused from HoverProvider pattern).
    struct LocalVar {
        std::string typeName;
        unsigned    arrayDims = 0;
        antlr4::Token* declToken = nullptr;  // points to the IDENTIFIER token
    };

private:
    // Collect local variables + params with their declaration tokens.
    std::unordered_map<std::string, LocalVar>
    collectLocals(LuxParser::FunctionDeclContext* func, size_t beforeLine);

    // Find the function declaration that encloses a given line.
    LuxParser::FunctionDeclContext*
    findEnclosingFunction(LuxParser::ProgramContext* tree, size_t line);

    // ── Symbol finders (return AST nodes for declarations) ──────────

    LuxParser::FunctionDeclContext*
    findFunctionDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::StructDeclContext*
    findStructDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::EnumDeclContext*
    findEnumDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::UnionDeclContext*
    findUnionDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::TypeAliasDeclContext*
    findTypeAliasDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::ExtendDeclContext*
    findExtendDecl(LuxParser::ProgramContext* tree, const std::string& name);

    LuxParser::ExternDeclContext*
    findExternDecl(LuxParser::ProgramContext* tree, const std::string& name);

    // ── AST walkers ─────────────────────────────────────────────────

    std::optional<DefinitionResult> resolveAtPosition(
        LuxParser::ProgramContext* tree, antlr4::Token* hoveredToken,
        const std::string& tokenText, const CBindings& bindings,
        size_t cursorLine, const std::string& filePath,
        const ProjectContext* project);

    // Resolve an identifier to its definition.
    std::optional<DefinitionResult> resolveIdent(
        const std::string& name, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const std::string& filePath, const ProjectContext* project);

    // Resolve a type name to its definition.
    std::optional<DefinitionResult> resolveTypeName(
        const std::string& name, LuxParser::ProgramContext* tree,
        const CBindings& bindings, const std::string& filePath,
        const ProjectContext* project);

    // Resolve an imported symbol from a `use` declaration.
    std::optional<DefinitionResult> resolveImportedSymbol(
        const std::string& symbolName, const std::string& modulePath,
        const ProjectContext* project);

    // Resolve a type spec token (IDENTIFIER inside a typeSpec node).
    std::optional<DefinitionResult> resolveTypeSpecToken(
        LuxParser::TypeSpecContext* typeSpec, antlr4::Token* hoveredToken,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const std::string& filePath, const ProjectContext* project);

    // Walk expressions to find the hovered token and resolve.
    std::optional<DefinitionResult> walkExprForDef(
        LuxParser::ExpressionContext* expr, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const std::string& filePath, const ProjectContext* project);

    std::optional<DefinitionResult> walkStmtForDef(
        LuxParser::StatementContext* stmt, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const std::string& filePath, const ProjectContext* project);

    std::optional<DefinitionResult> walkBlockForDef(
        LuxParser::BlockContext* block, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const std::string& filePath, const ProjectContext* project);

    // ── Helpers ─────────────────────────────────────────────────────

    // Build result from a token in the current file.
    DefinitionResult makeResult(antlr4::Token* token,
                                const std::string& filePath);

    // Build result from a token in another file.
    DefinitionResult makeResult(antlr4::Token* token,
                                const std::string& filePath,
                                const std::string& sourceFile);

    // Attempt to infer the struct type name produced by an expression.
    // E.g. an IdentExpr "p" declared as "Point" → returns "Point".
    // An IdentExpr "ptr" declared as "*Point" → returns "Point" (strips pointer).
    std::string inferExprStructType(LuxParser::ExpressionContext* expr,
                                    LuxParser::ProgramContext* tree,
                                    size_t cursorLine);

    // Given a struct name and field name, resolve to the field's declaration.
    std::optional<DefinitionResult> resolveStructField(
        const std::string& structName, const std::string& fieldName,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const std::string& filePath, const ProjectContext* project);

    // Check if a parse tree node contains a token.
    static bool containsToken(antlr4::ParserRuleContext* ctx,
                              antlr4::Token* token);
};
