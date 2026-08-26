#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "generated/LucisParser.h"

// Per-file index of top-level declarations, built once per parse tree.
// The LSP providers used to re-scan tree->topLevelDecl() on every
// request; this index turns those linear sweeps into O(1) lookups.
//
// All name lookups keep "first declaration in source order" semantics —
// identical to the linear sweeps they replace.
class FileSymbolIndex {
public:
    // Builds the index from a parse tree. The index borrows the tree's
    // AST nodes (they stay alive with the ParseResult), so it must not
    // outlive the tree it was built from.
    explicit FileSymbolIndex(LucisParser::ProgramContext* tree);

    // Name → first declaration (in source order), or nullptr.
    LucisParser::FunctionDeclContext*  function(const std::string& name) const;
    LucisParser::StructDeclContext*    structDecl(const std::string& name) const;
    LucisParser::EnumDeclContext*      enumDecl(const std::string& name) const;
    LucisParser::UnionDeclContext*     unionDecl(const std::string& name) const;
    LucisParser::BitstructDeclContext* bitstructDecl(const std::string& name) const;
    LucisParser::TypeAliasDeclContext* typeAliasDecl(const std::string& name) const;
    LucisParser::ExtendDeclContext*    extendDecl(const std::string& name) const;
    LucisParser::ExternDeclContext*    externDecl(const std::string& name) const;
    LucisParser::ConstDeclStmtContext* constDecl(const std::string& name) const;

    // The function whose source span contains the given 0-based line
    // (top-level functions never overlap), or nullptr.
    LucisParser::FunctionDeclContext* enclosingFunctionAt(size_t line) const;

    // The extend-block method whose source span contains the given
    // 0-based line, or nullptr.
    LucisParser::ExtendMethodContext* enclosingExtendMethodAt(size_t line) const;

private:
    // emplace() keeps the first declaration for each name: same result as
    // the linear "return first match" sweeps the index replaces.
    std::unordered_map<std::string, LucisParser::FunctionDeclContext*>  functions_;
    std::unordered_map<std::string, LucisParser::StructDeclContext*>    structs_;
    std::unordered_map<std::string, LucisParser::EnumDeclContext*>      enums_;
    std::unordered_map<std::string, LucisParser::UnionDeclContext*>     unions_;
    std::unordered_map<std::string, LucisParser::BitstructDeclContext*> bitstructs_;
    std::unordered_map<std::string, LucisParser::TypeAliasDeclContext*> aliases_;
    std::unordered_map<std::string, LucisParser::ExtendDeclContext*>    extends_;
    std::unordered_map<std::string, LucisParser::ExternDeclContext*>    externs_;
    std::unordered_map<std::string, LucisParser::ConstDeclStmtContext*> consts_;

    // Function spans sorted by start line for O(log n) enclosure lookups.
    struct FuncSpan {
        unsigned startLine = 0;
        unsigned endLine   = 0;
        LucisParser::FunctionDeclContext* decl = nullptr;
    };
    std::vector<FuncSpan> funcSpans_;

    // Extend-method spans sorted by start line (same lookup strategy).
    struct ExtendMethodSpan {
        unsigned startLine = 0;
        unsigned endLine   = 0;
        LucisParser::ExtendMethodContext* decl = nullptr;
    };
    std::vector<ExtendMethodSpan> extendMethodSpans_;
};