#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>

#include "generated/LuxParser.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "types/BuiltinRegistry.h"
#include "intrinsics/IntrinsicRegistry.h"
#include "ffi/CBindings.h"
#include "lsp/DocComment.h"

struct ParseResult;
class ProjectContext;

// Result of a hover query — markdown content + token range.
struct HoverResult {
    std::string contents;  // Markdown
    size_t line    = 0;    // 0-based
    size_t col     = 0;    // 0-based
    size_t endLine = 0;    // 0-based
    size_t endCol  = 0;    // 0-based
};

// Provides hover information for Lux source code.
// Parses the document, resolves symbols, and returns markdown tooltips.
class HoverProvider {
public:
    HoverProvider();

    // Returns hover info for the given 0-based line:col position.
    std::optional<HoverResult> hover(const std::string& source,
                                     size_t line, size_t col,
                                     ParseResult* preParsed = nullptr);

    // Returns hover info with full project context (cross-file symbols).
    std::optional<HoverResult> hover(const std::string& source,
                                     size_t line, size_t col,
                                     const std::string& filePath,
                                     const ProjectContext* project,
                                     ParseResult* preParsed = nullptr);

    // Lightweight variable info collected from AST (no Checker needed).
    struct LocalVar {
        std::string typeName;
        unsigned    arrayDims = 0;
        bool        isParameter = false;
    };

private:
    TypeRegistry         typeRegistry_;
    MethodRegistry       methodRegistry_;
    ExtendedTypeRegistry extTypeRegistry_;
    BuiltinRegistry      builtinRegistry_;
    IntrinsicRegistry    intrinsicRegistry_;
    std::vector<DocComment> docComments_;  // rebuilt per hover() call

    struct DocCommentCacheEntry {
        std::filesystem::file_time_type mtime{};
        bool hasMtime = false;
        std::vector<DocComment> docs;
    };
    std::unordered_map<std::string, DocCommentCacheEntry> crossFileDocCache_;

    // Attach doc-comment (if any) to hover markdown.
    std::string withDoc(const std::string& md, size_t declLine);

    // Load and cache doc-comments for cross-file symbols.
    const std::vector<DocComment>& docCommentsForFile(const std::string& filePath);

    // Collect local variables + params in a function up to a given line.
    std::unordered_map<std::string, LocalVar>
    collectLocals(LuxParser::FunctionDeclContext* func, size_t beforeLine,
                  LuxParser::ProgramContext* tree = nullptr,
                  const CBindings* bindings = nullptr,
                  const ProjectContext* project = nullptr);

    // Find the function declaration that encloses a given line.
    LuxParser::FunctionDeclContext*
    findEnclosingFunction(LuxParser::ProgramContext* tree, size_t line);

    // Find user-defined function signature from program.
    LuxParser::FunctionDeclContext*
    findFunctionDecl(LuxParser::ProgramContext* tree, const std::string& name);

    // Find user struct/enum/union/typeAlias declarations.
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

    // Hover resolvers for different AST contexts.
    std::optional<HoverResult> hoverIdent(
        const std::string& name, antlr4::Token* token,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project = nullptr);

    // Recursive tree walkers — find the expression context of the hovered token.
    std::optional<HoverResult> walkTreeForHover(
        LuxParser::BlockContext* block, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkBlockForHover(
        LuxParser::BlockContext* block, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkStmtForHover(
        LuxParser::StatementContext* stmt, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkExprForHover(
        LuxParser::ExpressionContext* expr, antlr4::Token* hoveredToken,
        const std::string& tokenText, LuxParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    // Hover on type specifiers (int32, *char, Vec<int32>, etc.)
    std::optional<HoverResult> hoverTypeSpec(
        LuxParser::TypeSpecContext* ts, antlr4::Token* hoveredToken,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project);

    // Hover on a type name (IDENTIFIER in type position)
    std::optional<HoverResult> hoverTypeName(
        const std::string& name, antlr4::Token* token,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project = nullptr);

    // Hover on composite expression parts
    std::optional<HoverResult> hoverMethodCall(
        LuxParser::MethodCallExprContext* ctx,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    std::optional<HoverResult> hoverFieldAccess(
        LuxParser::FieldAccessExprContext* ctx,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    std::optional<HoverResult> hoverEnumAccess(
        LuxParser::EnumAccessExprContext* ctx,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project = nullptr);

    std::optional<HoverResult> hoverStaticMethodCall(
        LuxParser::StaticMethodCallExprContext* ctx,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    // Hover on a generic intrinsic call.
    std::optional<HoverResult> hoverGenericIntrinsic(
        LuxParser::GenericQualifiedFnCallExprContext* ctx,
        antlr4::Token* token);

    // Hover on a symbol in a `use` declaration line.
    std::optional<HoverResult> hoverImportedSymbol(
        const std::string& symbolName,
        const std::string& modulePath,
        antlr4::Token* token,
        LuxParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project);

    // Formatting helpers.
    std::string typeSpecToString(LuxParser::TypeSpecContext* ctx);
    std::string formatFunctionDecl(LuxParser::FunctionDeclContext* func);
    std::string formatExternDecl(LuxParser::ExternDeclContext* ext);
    std::string formatStructDecl(LuxParser::StructDeclContext* decl);
    std::string formatEnumDecl(LuxParser::EnumDeclContext* decl);
    std::string formatUnionDecl(LuxParser::UnionDeclContext* decl);
    std::string formatCFunction(const CFunction& func);
    std::string formatCStruct(const std::string& name, const CStruct& s);
    std::string formatCEnum(const std::string& name, const CEnum& e);
    std::string formatBuiltinSignature(const BuiltinSignature& sig);
    std::string formatTypeInfo(const TypeInfo* ti);
    std::string formatExtendMethods(LuxParser::ExtendDeclContext* ext);

    // Build HoverResult from a token + content.
    HoverResult makeResult(antlr4::Token* token, const std::string& contents);
};
