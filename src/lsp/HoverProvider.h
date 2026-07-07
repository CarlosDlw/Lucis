#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>

#include "generated/LucisParser.h"
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

// Provides hover information for Lucis source code.
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
        bool        isConst = false;
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
    collectLocals(LucisParser::FunctionDeclContext* func, size_t beforeLine,
                  LucisParser::ProgramContext* tree = nullptr,
                  const CBindings* bindings = nullptr,
                  const ProjectContext* project = nullptr);

    // Find the function declaration that encloses a given line.
    LucisParser::FunctionDeclContext*
    findEnclosingFunction(LucisParser::ProgramContext* tree, size_t line);

    // Find user-defined function signature from program.
    LucisParser::FunctionDeclContext*
    findFunctionDecl(LucisParser::ProgramContext* tree, const std::string& name);

    // Find user struct/enum/union/typeAlias declarations.
    LucisParser::StructDeclContext*
    findStructDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::EnumDeclContext*
    findEnumDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::UnionDeclContext*
    findUnionDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::TypeAliasDeclContext*
    findTypeAliasDecl(LucisParser::ProgramContext* tree, const std::string& name);

    LucisParser::ExtendDeclContext*
    findExtendDecl(LucisParser::ProgramContext* tree, const std::string& name);

    // Hover resolvers for different AST contexts.
    std::optional<HoverResult> hoverIdent(
        const std::string& name, antlr4::Token* token,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project = nullptr);

    // Recursive tree walkers — find the expression context of the hovered token.
    std::optional<HoverResult> walkTreeForHover(
        LucisParser::BlockContext* block, antlr4::Token* hoveredToken,
        const std::string& tokenText, LucisParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkBlockForHover(
        LucisParser::BlockContext* block, antlr4::Token* hoveredToken,
        const std::string& tokenText, LucisParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkStmtForHover(
        LucisParser::StatementContext* stmt, antlr4::Token* hoveredToken,
        const std::string& tokenText, LucisParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    std::optional<HoverResult> walkExprForHover(
        LucisParser::ExpressionContext* expr, antlr4::Token* hoveredToken,
        const std::string& tokenText, LucisParser::ProgramContext* tree,
        const CBindings& bindings, size_t cursorLine,
        const ProjectContext* project);

    // Hover on type specifiers (int32, *char, Vec<int32>, etc.)
    std::optional<HoverResult> hoverTypeSpec(
        LucisParser::TypeSpecContext* ts, antlr4::Token* hoveredToken,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project);

    // Hover on a type name (IDENTIFIER in type position)
    std::optional<HoverResult> hoverTypeName(
        const std::string& name, antlr4::Token* token,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project = nullptr);

    // Hover on composite expression parts
    std::optional<HoverResult> hoverMethodCall(
        LucisParser::MethodCallExprContext* ctx,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    std::optional<HoverResult> hoverFieldAccess(
        LucisParser::FieldAccessExprContext* ctx,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    std::optional<HoverResult> hoverEnumAccess(
        LucisParser::EnumAccessExprContext* ctx,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project = nullptr);

    std::optional<HoverResult> hoverStaticMethodCall(
        LucisParser::StaticMethodCallExprContext* ctx,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        size_t cursorLine, const ProjectContext* project);

    // Hover on a generic intrinsic call.
    std::optional<HoverResult> hoverGenericIntrinsic(
        LucisParser::GenericQualifiedFnCallExprContext* ctx,
        antlr4::Token* token);

    // Hover on a symbol in a `use` declaration line.
    std::optional<HoverResult> hoverImportedSymbol(
        const std::string& symbolName,
        const std::string& modulePath,
        antlr4::Token* token,
        LucisParser::ProgramContext* tree, const CBindings& bindings,
        const ProjectContext* project);

    std::optional<HoverResult> hoverModulePathSegment(
        const std::string& modulePath,
        antlr4::Token* token,
        const ProjectContext* project);

    // Formatting helpers.
    std::string typeSpecToString(LucisParser::TypeSpecContext* ctx);
    std::string formatFunctionDecl(LucisParser::FunctionDeclContext* func);
    std::string formatExternDecl(LucisParser::ExternDeclContext* ext);
    std::string formatStructDecl(LucisParser::StructDeclContext* decl);
    std::string formatEnumDecl(LucisParser::EnumDeclContext* decl);
    std::string formatUnionDecl(LucisParser::UnionDeclContext* decl);
    std::string formatCFunction(const CFunction& func);
    std::string formatCStruct(const std::string& name, const CStruct& s);
    std::string formatCEnum(const std::string& name, const CEnum& e);
    std::string formatBuiltinSignature(const BuiltinSignature& sig);
    std::string formatTypeInfo(const TypeInfo* ti);
    std::string formatExtendMethods(LucisParser::ExtendDeclContext* ext);

    // Build HoverResult from a token + content.
    HoverResult makeResult(antlr4::Token* token, const std::string& contents);

    // Types that have a user-defined drop() method (populated during hover()).
    std::unordered_set<std::string> typesWithDrop_;
};
