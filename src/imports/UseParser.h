#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "generated/LucisParser.h"

// Single normalized representation of every `use` declaration form:
//
//   use module;              → Module        (local = last path segment)
//   use fold::module;        → Module        (local = last path segment)
//   use std::module;         → Module
//   use module::func;        → Symbol        (exported = func, local = func)
//   use module::func as f;   → Symbol        (exported = func, local = f)
//   use module::{ a, b as c }; → Symbol ×2
//   use Type::*;             → EnumWildcard  (type resolved by caller)
//
// Every consumer (checker, IRGen, LSP) must go through this helper so the
// four forms are never re-interpreted differently across the codebase.
struct UseBinding {
    enum class Kind {
        Module,        // namespace import: use std::log;
        Symbol,        // single or group symbol: use m::f as local;
        EnumWildcard,  // enum variant injection: use Type::*;
    };

    Kind kind = Kind::Symbol;
    std::string modulePath;    // normalized with '::' separators
    std::string exportedName;  // name inside the module (Symbol only)
    std::string localName;     // binding in the importing scope
    antlr4::ParserRuleContext* ctx = nullptr;  // for diagnostics
};

// Parses a single useDecl into zero or more normalized bindings.
// Returns an empty vector for unsupported/invalid declarations; the
// caller is responsible for reporting semantic errors.
std::vector<UseBinding> parseUseDecl(LucisParser::UseDeclContext* useDecl);

// Like parseUseDecl but also exposes the source tokens of each binding,
// for LSP providers (hover/definition/completion/semantic-tokens).
struct UseBindingWithTokens {
    UseBinding binding;
    antlr4::Token* exportedToken = nullptr;  // token of the exported name
    antlr4::Token* localToken = nullptr;     // alias token (or exported when no alias)
};
std::vector<UseBindingWithTokens> parseUseDeclWithTokens(
    LucisParser::UseDeclContext* useDecl);

// Splits a module path into its last segment (the local binding name).
std::string lastPathSegment(const std::string& modulePath);

// Per-file import map for LSP resolution: given an identifier used in the
// file, tells whether it is an imported symbol (and under which exported
// name) or an imported module namespace.
struct ImportAliasMap {
    // localName -> {modulePath, exportedName} (use m::f as local; use m::{f as local};)
    std::unordered_map<std::string, std::pair<std::string, std::string>> symbols;
    // localName -> modulePath (use module; use fold::module;)
    std::unordered_map<std::string, std::string> modules;

    bool isEmpty() const { return symbols.empty() && modules.empty(); }
};
ImportAliasMap buildImportAliasMap(LucisParser::ProgramContext* tree);