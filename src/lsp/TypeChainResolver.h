#pragma once

#include <string>
#include <unordered_map>
#include "generated/LucisParser.h"

class ProjectContext;
struct CBindings;
struct BuiltinRegistry;
struct MethodRegistry;
struct ExtendedTypeRegistry;
class ModuleRegistry;
class TypeRegistry;

// Shared resolver for deep access chains used by the LSP providers
// (completion, hover, signature help).
//
// Given the AST node of a receiver expression, it walks the chain step by
// step — dot access, arrow access, index access, method calls (including
// arrow method calls) and tuple indices — and returns the final type name.
// Returns "" when the chain cannot be resolved.
struct ChainResolveCtx {
    LucisParser::ProgramContext* tree = nullptr;
    const ProjectContext* project = nullptr;
    const CBindings* bindings = nullptr;
    const TypeRegistry* typeReg = nullptr;
    const BuiltinRegistry* builtinReg = nullptr;
    const MethodRegistry* methodReg = nullptr;
    const ExtendedTypeRegistry* extTypeReg = nullptr;
    std::unordered_map<std::string, std::string> locals;  // var name → type text
    std::unordered_map<std::string, std::string> globals; // fn name → return type text
};

std::string resolveChainType(LucisParser::ExpressionContext* expr,
                             const ChainResolveCtx& ctx);
