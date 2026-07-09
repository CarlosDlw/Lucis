#include "lsp/SemanticTokensProvider.h"
#include "parser/Parser.h"
#include "generated/LucisLexer.h"
#include "generated/LucisParser.h"

#include <algorithm>
#include <regex>
#include <sstream>

#include <unordered_set>
#include <mutex>
#include "imports/ImportResolver.h"
#include "types/BuiltinRegistry.h"

// ═══════════════════════════════════════════════════════════════════════
//  Legend
// ═══════════════════════════════════════════════════════════════════════

const std::vector<std::string>& SemanticTokensProvider::tokenTypes() {
    static const std::vector<std::string> types = {
        "namespace", "type", "struct", "enum", "enumMember",
        "function", "method", "parameter", "variable", "property",
        "keyword", "comment", "string", "number", "operator", "macro",
        "escapeSequence"
    };
    return types;
}

const std::vector<std::string>& SemanticTokensProvider::tokenModifiers() {
    static const std::vector<std::string> mods = {
        "declaration", "definition", "readonly", "static", "defaultLibrary",
        "comptime"
    };
    return mods;
}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

static inline void emit(std::vector<RawSemanticToken>& out,
                         uint32_t line, uint32_t col, uint32_t len,
                         SemanticTokenType type,
                         uint32_t modifiers = 0) {
    if (len == 0) return;
    out.push_back({line, col, len,
                   static_cast<uint32_t>(type), modifiers});
}

// Keyword token types (by lexer token id)
static bool isKeyword(size_t tokenType) {
    switch (tokenType) {
        case LucisLexer::STRUCT: case LucisLexer::UNION: case LucisLexer::ENUM:
        case LucisLexer::FN: case LucisLexer::TYPE: case LucisLexer::AS:
        case LucisLexer::IS: case LucisLexer::SIZEOF: case LucisLexer::TYPEOF:
        case LucisLexer::ALIGNOF: case LucisLexer::OFFSETOF:
        case LucisLexer::IF: case LucisLexer::ELSE: case LucisLexer::FOR:
        case LucisLexer::IN: case LucisLexer::LOOP: case LucisLexer::WHILE:
        case LucisLexer::DO: case LucisLexer::BREAK: case LucisLexer::CONTINUE:
        case LucisLexer::SWITCH: case LucisLexer::CASE: case LucisLexer::DEFAULT:
        case LucisLexer::SPAWN: case LucisLexer::AWAIT: case LucisLexer::LOCK:
        case LucisLexer::EXTEND: case LucisLexer::TRY: case LucisLexer::CATCH:
        case LucisLexer::FINALLY: case LucisLexer::THROW: case LucisLexer::DEFER:
        case LucisLexer::EXTERN: case LucisLexer::AUTO: case LucisLexer::NULL_LIT:
        case LucisLexer::RET:    case LucisLexer::OR:   case LucisLexer::ATTR_ERROR:
        case LucisLexer::MATCH:  case LucisLexer::WILDCARD:
        case LucisLexer::INLINE_BLOCK: case LucisLexer::SCOPE_BLOCK:
        case LucisLexer::ASM:    case LucisLexer::VOLATILE: case LucisLexer::GOTO:
        case LucisLexer::INTEL:  case LucisLexer::COMPTIME: case LucisLexer::CONST:
            return true;
        default:
            return false;
    }
}

static bool isPrimitiveType(size_t tokenType) {
    if (tokenType >= LucisLexer::INT1 && tokenType <= LucisLexer::CSTRING)
        return true;
    // Native collection type keywords are also types
    if (tokenType == LucisLexer::VEC || tokenType == LucisLexer::MAP ||
        tokenType == LucisLexer::SET)
        return true;
    return false;
}

static bool isOperator(size_t tokenType) {
    switch (tokenType) {
        case LucisLexer::PLUS: case LucisLexer::MINUS: case LucisLexer::STAR:
        case LucisLexer::SLASH: case LucisLexer::PERCENT:
        case LucisLexer::EQ: case LucisLexer::NEQ: case LucisLexer::LT:
        case LucisLexer::GT: case LucisLexer::LTE: case LucisLexer::GTE:
        case LucisLexer::LAND: case LucisLexer::LOR: case LucisLexer::NOT:
        case LucisLexer::AMPERSAND: case LucisLexer::PIPE: case LucisLexer::CARET:
        case LucisLexer::TILDE: case LucisLexer::LSHIFT:
        case LucisLexer::INCR: case LucisLexer::DECR:
        case LucisLexer::ASSIGN: case LucisLexer::ARROW:
        case LucisLexer::PLUS_ASSIGN: case LucisLexer::MINUS_ASSIGN:
        case LucisLexer::STAR_ASSIGN: case LucisLexer::SLASH_ASSIGN:
        case LucisLexer::PERCENT_ASSIGN: case LucisLexer::AMP_ASSIGN:
        case LucisLexer::PIPE_ASSIGN: case LucisLexer::CARET_ASSIGN:
        case LucisLexer::LSHIFT_ASSIGN: case LucisLexer::RSHIFT_ASSIGN:
        case LucisLexer::NULLCOAL_ASSIGN: case LucisLexer::NULLCOAL: case LucisLexer::SPREAD:
        case LucisLexer::RANGE: case LucisLexer::RANGE_INCL:
        case LucisLexer::QUESTION:
            return true;
        default:
            return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Collect contextual identifiers from the parse tree
// ═══════════════════════════════════════════════════════════════════════

struct IdentClassification {
    SemanticTokenType type;
    uint32_t modifiers;
};

// Key: "line:col" → classification
using IdentMap = std::unordered_map<std::string, IdentClassification>;

static std::string key(antlr4::Token* tok) {
    return std::to_string(tok->getLine() - 1) + ":" +
           std::to_string(tok->getCharPositionInLine());
}

static std::string key(antlr4::tree::TerminalNode* node) {
    return key(node->getSymbol());
}

static void classifyIdent(IdentMap& map, antlr4::tree::TerminalNode* node,
                           SemanticTokenType type, uint32_t modifiers = 0) {
    if (!node) return;
    map[key(node)] = {type, modifiers};
}

// Walk the parse tree to classify IDENTIFIER tokens by their context.
static void walkTree(IdentMap& map, antlr4::tree::ParseTree* node) {
    if (!node) return;

    // ── use ── modulePath identifiers are namespaces
    else if (auto* ctx = dynamic_cast<LucisParser::ModulePathContext*>(node)) {
        for (auto* id : ctx->IDENTIFIER())
            classifyIdent(map, id, SemanticTokenType::Namespace);
    }
    else if (auto* ctx = dynamic_cast<LucisParser::UseItemContext*>(node)) {
        // The last IDENTIFIER is the imported symbol (function/type)
        uint32_t mods = 0;
        if (ctx->modulePath() &&
            ctx->modulePath()->getText().rfind("std::", 0) == 0) {
            mods = static_cast<uint32_t>(SemanticTokenMod::DefaultLib);
        }
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Function, mods);
    }
    else if (auto* ctx = dynamic_cast<LucisParser::UseGroupContext*>(node)) {
        uint32_t mods = 0;
        std::string modulePath;
        if (ctx->modulePath()) {
            modulePath = ctx->modulePath()->getText();
            if (modulePath.rfind("std::", 0) == 0) {
                mods = static_cast<uint32_t>(SemanticTokenMod::DefaultLib);
            }
        }
        for (auto* id : ctx->IDENTIFIER()) {
            std::string sym = id->getText();
            if (!modulePath.empty() && ImportResolver::isStdModule(modulePath)) {
                if (ImportResolver::moduleExportsSymbol(modulePath, sym)) {
                    classifyIdent(map, id, SemanticTokenType::Function, mods);
                    continue;
                }
            }
            // Fallback: se for constante conhecida
            if (BuiltinRegistry().lookupConstant(sym) != "") {
                classifyIdent(map, id, SemanticTokenType::Variable, mods);
                continue;
            }
            // Default: função
            classifyIdent(map, id, SemanticTokenType::Function, mods);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::UseRootContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Namespace);
    }
    else if (auto* ctx = dynamic_cast<LucisParser::UseEnumWildcardContext*>(node)) {
        if (ctx->typeSpec() && ctx->typeSpec()->IDENTIFIER())
            classifyIdent(map, ctx->typeSpec()->IDENTIFIER(), SemanticTokenType::Enum);
    }

    // ── struct ──
    else if (auto* ctx = dynamic_cast<LucisParser::StructDeclContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Struct,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                      static_cast<uint32_t>(SemanticTokenMod::Definition));
        // Type params (T, U, ...) declared in the generic list
        if (ctx->typeParamList()) {
            for (auto* tp : ctx->typeParamList()->typeParam()) {
                auto ids = tp->IDENTIFIER();
                if (!ids.empty())
                    classifyIdent(map, ids[0], SemanticTokenType::Type,
                                  static_cast<uint32_t>(SemanticTokenMod::Declaration));
            }
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::StructFieldContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Property,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── union ──
    else if (auto* ctx = dynamic_cast<LucisParser::UnionDeclContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Struct,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                      static_cast<uint32_t>(SemanticTokenMod::Definition));
        if (ctx->typeParamList()) {
            for (auto* tp : ctx->typeParamList()->typeParam()) {
                auto ids = tp->IDENTIFIER();
                if (!ids.empty())
                    classifyIdent(map, ids[0], SemanticTokenType::Type,
                                  static_cast<uint32_t>(SemanticTokenMod::Declaration));
            }
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::UnionFieldContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Property,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── enum ──
    else if (auto* ctx = dynamic_cast<LucisParser::EnumDeclContext*>(node)) {
        if (ctx->IDENTIFIER()) {
            classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Enum,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                          static_cast<uint32_t>(SemanticTokenMod::Definition));
            for (auto* variant : ctx->enumVariant()) {
                if (variant->IDENTIFIER()) {
                    classifyIdent(map, variant->IDENTIFIER(), SemanticTokenType::EnumMember,
                                  static_cast<uint32_t>(SemanticTokenMod::Declaration));
                }
            }
        }
    }

    // ── type alias ──
    else if (auto* ctx = dynamic_cast<LucisParser::TypeAliasDeclContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Type,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── function decl ──
    else if (auto* ctx = dynamic_cast<LucisParser::FunctionDeclContext*>(node)) {
        if (!ctx->IDENTIFIER().empty()) {
            uint32_t mods = static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                            static_cast<uint32_t>(SemanticTokenMod::Definition);
            if (ctx->COMPTIME())
                mods |= static_cast<uint32_t>(SemanticTokenMod::Comptime);
            classifyIdent(map, ctx->IDENTIFIER(0), SemanticTokenType::Function, mods);
        }
        // Generic type params
        if (ctx->typeParamList()) {
            for (auto* tp : ctx->typeParamList()->typeParam()) {
                auto ids = tp->IDENTIFIER();
                if (!ids.empty())
                    classifyIdent(map, ids[0], SemanticTokenType::Type,
                                  static_cast<uint32_t>(SemanticTokenMod::Declaration));
            }
        }
    }

    // ── extern decl ──
    else if (auto* ctx = dynamic_cast<LucisParser::ExternDeclContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Function,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── extend block ──
    else if (auto* ctx = dynamic_cast<LucisParser::ExtendDeclContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Struct);
        // Generic type params
        if (ctx->typeParamList()) {
            for (auto* tp : ctx->typeParamList()->typeParam()) {
                auto ids = tp->IDENTIFIER();
                if (!ids.empty())
                    classifyIdent(map, ids[0], SemanticTokenType::Type,
                                  static_cast<uint32_t>(SemanticTokenMod::Declaration));
            }
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::ExtendMethodContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            // extendMethod grammar always places the method name at IDENTIFIER[0]
            classifyIdent(map, ids[0], SemanticTokenType::Method,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                          static_cast<uint32_t>(SemanticTokenMod::Definition));

            // Instance method form: type name(&self, ...)
            // Mark the self identifier as a parameter declaration.
            if (ctx->AMPERSAND() && ids.size() >= 2) {
                classifyIdent(map, ids[1], SemanticTokenType::Parameter,
                              static_cast<uint32_t>(SemanticTokenMod::Declaration));
            }
        }
    }

    // ── parameters ──
    else if (auto* ctx = dynamic_cast<LucisParser::ParamContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Parameter,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }
    else if (auto* ctx = dynamic_cast<LucisParser::ExternParamContext*>(node)) {
        if (ctx->IDENTIFIER())
            classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Parameter,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── variable declarations ──
    else if (auto* ctx = dynamic_cast<LucisParser::VarDeclStmtContext*>(node)) {
        for (auto* id : ctx->IDENTIFIER())
            classifyIdent(map, id, SemanticTokenType::Variable,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── const declarations ──
    else if (auto* ctx = dynamic_cast<LucisParser::ConstDeclStmtContext*>(node)) {
        for (auto* decl : ctx->constDeclarator()) {
            auto* id = decl->IDENTIFIER();
            if (id)
                classifyIdent(map, id, SemanticTokenType::Variable,
                              static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                              static_cast<uint32_t>(SemanticTokenMod::Readonly));
        }
    }

    // ── for-in variable ──
    else if (auto* ctx = dynamic_cast<LucisParser::ForInStmtContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Variable,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }
    else if (auto* ctx = dynamic_cast<LucisParser::ForClassicStmtContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Variable,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── catch variable ──
    else if (auto* ctx = dynamic_cast<LucisParser::CatchClauseContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Variable,
                      static_cast<uint32_t>(SemanticTokenMod::Declaration));
    }

    // ── self usage in member assignment statements (LHS) ──
    else if (auto* ctx = dynamic_cast<LucisParser::ArrowAssignStmtContext*>(node)) {
        auto* base = ctx->IDENTIFIER(0);
        if (base && base->getText() == "self") {
            classifyIdent(map, base, SemanticTokenType::Parameter);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::ArrowCompoundAssignStmtContext*>(node)) {
        auto* base = ctx->IDENTIFIER(0);
        if (base && base->getText() == "self") {
            classifyIdent(map, base, SemanticTokenType::Parameter);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::FieldAssignStmtContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            if (ids[0]->getText() == "self") {
                classifyIdent(map, ids[0], SemanticTokenType::Parameter);
            }
            for (size_t i = 1; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::FieldCompoundAssignStmtContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            if (ids[0]->getText() == "self") {
                classifyIdent(map, ids[0], SemanticTokenType::Parameter);
            }
            for (size_t i = 1; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::IndexFieldAssignStmtContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            if (ids[0]->getText() == "self") {
                classifyIdent(map, ids[0], SemanticTokenType::Parameter);
            }
            for (size_t i = 1; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }

    // ── function/method call expressions ──
    else if (auto* ctx = dynamic_cast<LucisParser::FnCallExprContext*>(node)) {
        // If the callee is a bare identifier, classify it as function
        if (auto* identExpr = dynamic_cast<LucisParser::IdentExprContext*>(ctx->expression())) {
            classifyIdent(map, identExpr->IDENTIFIER(), SemanticTokenType::Function);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::MethodCallExprContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Method);
    }
    else if (auto* ctx = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            std::string ownerPath;
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                if (!ownerPath.empty()) ownerPath += "::";
                ownerPath += ids[i]->getText();
            }
            uint32_t mods = static_cast<uint32_t>(SemanticTokenMod::Static);
            if (ownerPath == "std" || ownerPath.rfind("std::", 0) == 0 ||
                ownerPath == "lucis" || ownerPath.rfind("lucis::", 0) == 0) {
                mods |= static_cast<uint32_t>(SemanticTokenMod::DefaultLib);
            }

            // Multi-segment owner path support: A::B::C::method()
            if (ids.size() == 2) {
                classifyIdent(map, ids[0], SemanticTokenType::Type);
            } else {
                for (size_t i = 0; i + 1 < ids.size(); ++i)
                    classifyIdent(map, ids[i], SemanticTokenType::Namespace);
            }

            classifyIdent(map, ids.back(), SemanticTokenType::Method, mods);
        }
    }

    // ── call statement ──
    else if (auto* ctx = dynamic_cast<LucisParser::CallStmtContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Function);
    }

    // ── field access ──
    else if (auto* ctx = dynamic_cast<LucisParser::FieldAccessExprContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Property);
    }
    else if (auto* ctx = dynamic_cast<LucisParser::ArrowAccessExprContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Property);
    }

    // ── enum access ──
    else if (auto* ctx = dynamic_cast<LucisParser::EnumAccessExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Enum);
            classifyIdent(map, ids[1], SemanticTokenType::EnumMember);
        }
    }

    // ── generic enum access: Result<int32, string>::Unit ──
    else if (auto* ctx = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Enum);
            classifyIdent(map, ids[1], SemanticTokenType::EnumMember);
        }
    }

    // ── match arm patterns: Ok and Ok(v) — variant names ──
    else if (auto* ctx = dynamic_cast<LucisParser::PatternContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ctx->SCOPE() && ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Enum);
            classifyIdent(map, ids[1], SemanticTokenType::EnumMember);
        } else if (!ctx->SCOPE() && ids.size() >= 1 && !ctx->LT()) {
            // Bare variant name (e.g. Ok in pattern)
            classifyIdent(map, ids[0], SemanticTokenType::EnumMember);
        }
    }

    // ── qualified struct/enum init lexpr ──
    else if (auto* ctx = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Namespace);
            classifyIdent(map, ids[1], SemanticTokenType::Type);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Namespace);
            classifyIdent(map, ids[1], SemanticTokenType::Type);
            for (size_t i = 2; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }
    else if (auto* ctx = dynamic_cast<LucisParser::GenericEnumNamedVariantExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 2) {
            classifyIdent(map, ids[0], SemanticTokenType::Enum);
            classifyIdent(map, ids[1], SemanticTokenType::EnumMember);
            for (size_t i = 2; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }

    // ── is variant check: expr is Type::Variant ──
    else if (auto* ctx = dynamic_cast<LucisParser::IsExprContext*>(node)) {
        if (ctx->SCOPE() && ctx->IDENTIFIER(0)) {
            classifyIdent(map, ctx->IDENTIFIER(0), SemanticTokenType::EnumMember);
        }
        if (ctx->LPAREN() && ctx->IDENTIFIER(1)) {
            classifyIdent(map, ctx->IDENTIFIER(1), SemanticTokenType::Variable,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration));
        }
    }

    // ── struct literal ──
    else if (auto* ctx = dynamic_cast<LucisParser::StructLitExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            classifyIdent(map, ids[0], SemanticTokenType::Struct);
            // Rest are field names
            for (size_t i = 1; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }

    // ── generic function call: max<int32>(a, b) ──
    else if (auto* ctx = dynamic_cast<LucisParser::GenericFnCallExprContext*>(node)) {
        classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Function);
    }

    // ── generic qualified function call: lucis::unsafe::va_arg<int32>(va) ──
    else if (auto* ctx = dynamic_cast<LucisParser::GenericQualifiedFnCallExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        for (size_t i = 0; i + 1 < ids.size(); ++i)
            classifyIdent(map, ids[i], SemanticTokenType::Namespace);
        if (!ids.empty())
            classifyIdent(map, ids.back(), SemanticTokenType::Function);
    }

    // ── generic static method call: Node<int32>::create(42) ──
    else if (auto* ctx = dynamic_cast<LucisParser::GenericStaticMethodCallExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (ids.size() >= 1) classifyIdent(map, ids[0], SemanticTokenType::Struct);
        if (ids.size() >= 2) classifyIdent(map, ids[1], SemanticTokenType::Method,
                                            static_cast<uint32_t>(SemanticTokenMod::Static));
    }

    // ── generic struct literal: Node<int32> { value: 42 } ──
    else if (auto* ctx = dynamic_cast<LucisParser::GenericStructLitExprContext*>(node)) {
        auto ids = ctx->IDENTIFIER();
        if (!ids.empty()) {
            classifyIdent(map, ids[0], SemanticTokenType::Struct);
            // Field names start at [1]
            for (size_t i = 1; i < ids.size(); ++i)
                classifyIdent(map, ids[i], SemanticTokenType::Property);
        }
    }

    // ── typeSpec IDENTIFIER → user-defined type ──
    else if (auto* ctx = dynamic_cast<LucisParser::TypeSpecContext*>(node)) {
        // Only classify if this is a plain IDENTIFIER typeSpec (user type)
        // and there's no other child rule (not pointer, array, fn, etc.)
        if (ctx->IDENTIFIER() && !ctx->LT() &&
            ctx->children.size() == 1) {
            classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Type);
        }
        // Generic type: Vec<int32> — the IDENTIFIER before LT
        else if (ctx->IDENTIFIER() && ctx->LT()) {
            classifyIdent(map, ctx->IDENTIFIER(), SemanticTokenType::Type);
        }
    }

    // ── self identifier usage inside extend instance methods ──
    else if (auto* ctx = dynamic_cast<LucisParser::IdentExprContext*>(node)) {
        auto* id = ctx->IDENTIFIER();
        if (id && id->getText() == "self") {
            classifyIdent(map, id, SemanticTokenType::Parameter);
        }
    }

    // ── #scope callback: funcName(args) or varName.methodName(args) ──
    else if (auto* ctx = dynamic_cast<LucisParser::ScopeCallbackContext*>(node)) {
        if (ctx->DOT()) {
            // dot-access: receiver variable + method name
            classifyIdent(map, ctx->IDENTIFIER(0), SemanticTokenType::Variable);
            classifyIdent(map, ctx->IDENTIFIER(1), SemanticTokenType::Method);
        } else {
            // plain call: function name
            classifyIdent(map, ctx->IDENTIFIER(0), SemanticTokenType::Function);
        }
    }

    // Recurse into children
    for (size_t i = 0; i < node->children.size(); ++i) {
        walkTree(map, node->children[i]);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Escape-sequence scanning for string/char semantic tokens
// ═══════════════════════════════════════════════════════════════════════

static bool isHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static bool isOct(char c) { return c >= '0' && c <= '7'; }

// Return the total length of the escape starting at s[i] (s[i] must be '\\').
static size_t escapeSeqLen(const std::string& s, size_t i) {
    if (i + 1 >= s.size()) return 2;
    char n = s[i + 1];
    switch (n) {
        case 'x': {
            size_t j = i + 2;
            while (j < s.size() && isHex(s[j]) && j - (i + 2) < 2) ++j;
            return j - i;
        }
        case 'u':
            return (i + 5 < s.size()) ? 6 : 2;
        case 'U':
            return (i + 9 < s.size()) ? 10 : 2;
        default:
            if (isOct(n)) {
                size_t j = i + 2;
                while (j < s.size() && isOct(s[j]) && j - (i + 1) < 3) ++j;
                return j - i;
            }
            return 2; // \<char>
    }
}

// Emit semantic sub-tokens for a string/char literal, splitting out escapes.
static void emitStringSubTokens(std::vector<RawSemanticToken>& out,
                                 uint32_t line, uint32_t col,
                                 const std::string& text, size_t lexerType) {
    // Determine prefix length (e.g. "c" for C_STR_LIT)
    size_t pre = (lexerType == LucisLexer::C_STR_LIT) ? 1 : 0;

    if (pre > 0) {
        emit(out, line, col, static_cast<uint32_t>(pre), SemanticTokenType::String);
        col += static_cast<uint32_t>(pre);
    }

    // opening quote
    emit(out, line, col, 1, SemanticTokenType::String);
    col += 1;

    size_t end = text.size() - 1; // position of closing quote
    for (size_t i = pre + 1; i < end; ) {
        if (text[i] == '\\') {
            size_t elen = escapeSeqLen(text, i);
            emit(out, line, col, static_cast<uint32_t>(elen), SemanticTokenType::EscapeSequence);
            i += elen;
            col += static_cast<uint32_t>(elen);
        } else {
            size_t seg = i;
            while (i < end && text[i] != '\\') ++i;
            uint32_t segLen = static_cast<uint32_t>(i - seg);
            emit(out, line, col, segLen, SemanticTokenType::String);
            col += segLen;
        }
    }

    // closing quote
    emit(out, line, col, 1, SemanticTokenType::String);
}

// ═══════════════════════════════════════════════════════════════════════
//  c_macro block: parse the opaque token text for semantic highlighting
// ═══════════════════════════════════════════════════════════════════════

// Helper: emit sub-tokens for a single line of C preprocessor content.
static void emitCMacroLineTokens(std::vector<RawSemanticToken>& out,
                                 uint32_t line, uint32_t baseCol,
                                 const std::string& text,
                                 uint32_t firstNonSpace) {
    // ── Preprocessor directive ──────────────────────────────────────
    if (firstNonSpace < text.size() && text[firstNonSpace] == '#') {
        // Work with the trimmed portion for all parsing
        std::string trimmed = text.substr(firstNonSpace);
        // Find end of directive keyword (e.g. "#define", "#ifdef", "#include")
        size_t kwEnd = 1; // skip '#'
        while (kwEnd < trimmed.size() && (std::isalnum(trimmed[kwEnd]) || trimmed[kwEnd] == '_'))
            kwEnd++;
        std::string directive = trimmed.substr(0, kwEnd);

        uint32_t docCol = baseCol + firstNonSpace;

        // All preprocessor directives are Macro tokens
        emit(out, line, docCol, static_cast<uint32_t>(kwEnd), SemanticTokenType::Macro);

        // Rest of line after directive keyword
        auto restStart = trimmed.find_first_not_of(" \t", kwEnd);
        if (restStart == std::string::npos) return;

        if (directive == "#define") {
            // #define NAME value or #define NAME(args) body
            auto nameStart = restStart;
            auto nameEnd = trimmed.find_first_of(" \t(", restStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            std::string macroName = trimmed.substr(nameStart, nameEnd - nameStart);
            if (!macroName.empty()) {
                emit(out, line, docCol + nameStart,
                     static_cast<uint32_t>(macroName.size()),
                     SemanticTokenType::Macro,
                     static_cast<uint32_t>(SemanticTokenMod::Declaration));

                if (nameEnd < trimmed.size() && trimmed[nameEnd] == '(') {
                    // Function-like macro: emit params
                    auto closeParen = trimmed.find(')', nameEnd);
                    if (closeParen != std::string::npos) {
                        emit(out, line, docCol + nameEnd, 1, SemanticTokenType::Operator);
                        size_t pos = nameEnd + 1;
                        // Emit individual parameters
                        for (size_t sp = nameEnd + 1; sp < closeParen;) {
                            if (std::isspace(trimmed[sp])) { sp++; pos++; continue; }
                            size_t paramStart = sp;
                            while (sp < closeParen && trimmed[sp] != ',' && !std::isspace(trimmed[sp])) sp++;
                            if (sp > paramStart) {
                                emit(out, line, docCol + pos,
                                     static_cast<uint32_t>(sp - paramStart),
                                     SemanticTokenType::Parameter,
                                     static_cast<uint32_t>(SemanticTokenMod::Declaration));
                                pos += sp - paramStart;
                            }
                            if (sp < closeParen && trimmed[sp] == ',') {
                                sp++; pos++;
                            }
                        }
                        emit(out, line, docCol + closeParen, 1, SemanticTokenType::Operator);
                        // Body after closing paren
                        auto bodyStart = trimmed.find_first_not_of(" \t", closeParen + 1);
                        if (bodyStart != std::string::npos) {
                            for (size_t i = bodyStart; i < trimmed.size();) {
                                if (std::isspace(trimmed[i])) { i++; continue; }
                                if (std::isdigit(trimmed[i]) || (trimmed[i] == '-' && i+1 < trimmed.size() && std::isdigit(trimmed[i+1]))) {
                                    size_t ns = i;
                                    if (trimmed[i] == '-') i++;
                                    while (i < trimmed.size() && (std::isxdigit(trimmed[i]) || trimmed[i] == '.'))
                                        i++;
                                    emit(out, line, docCol + ns, static_cast<uint32_t>(i - ns), SemanticTokenType::Number);
                                    continue;
                                }
                                if (trimmed[i] == '(' || trimmed[i] == ')' || trimmed[i] == '+' || trimmed[i] == '-' ||
                                    trimmed[i] == '*' || trimmed[i] == '/' || trimmed[i] == '&' || trimmed[i] == '|' ||
                                    trimmed[i] == '^' || trimmed[i] == '~' || trimmed[i] == '<' || trimmed[i] == '>') {
                                    size_t os = i;
                                    if (i+1 < trimmed.size() && ((trimmed[i] == '<' && trimmed[i+1] == '<') || (trimmed[i] == '>' && trimmed[i+1] == '>')))
                                        i++;
                                    i++;
                                    emit(out, line, docCol + os, static_cast<uint32_t>(i - os), SemanticTokenType::Operator);
                                    continue;
                                }
                                if (std::isalnum(trimmed[i]) || trimmed[i] == '_') {
                                    size_t is = i;
                                    while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_')) i++;
                                    emit(out, line, docCol + is, static_cast<uint32_t>(i - is), SemanticTokenType::Variable);
                                    continue;
                                }
                                i++;
                            }
                        }
                    }
                } else {
                    // Simple macro: emit value tokens
                    auto valStart = trimmed.find_first_not_of(" \t", nameEnd);
                    if (valStart != std::string::npos && valStart < trimmed.size()) {
                        if (valStart + 1 < trimmed.size() && trimmed[valStart] == '/' && trimmed[valStart+1] == '/')
                            return;
                        for (size_t i = valStart; i < trimmed.size();) {
                            if (std::isspace(trimmed[i])) { i++; continue; }
                            if (std::isdigit(trimmed[i]) || (trimmed[i] == '-' && i+1 < trimmed.size() && std::isdigit(trimmed[i+1]))) {
                                size_t ns = i;
                                if (trimmed[i] == '-') i++;
                                while (i < trimmed.size() && (std::isxdigit(trimmed[i]) || trimmed[i] == '.')) i++;
                                emit(out, line, docCol + ns, static_cast<uint32_t>(i - ns), SemanticTokenType::Number);
                                continue;
                            }
                            if (trimmed[i] == '+' || trimmed[i] == '-' || trimmed[i] == '*' || trimmed[i] == '/' ||
                                trimmed[i] == '&' || trimmed[i] == '|' || trimmed[i] == '^' || trimmed[i] == '~' ||
                                trimmed[i] == '<' || trimmed[i] == '>' || trimmed[i] == '(' || trimmed[i] == ')' ||
                                trimmed[i] == '?' || trimmed[i] == ':') {
                                size_t os = i;
                                if (i+1 < trimmed.size() && ((trimmed[i] == '<' && trimmed[i+1] == '<') || (trimmed[i] == '>' && trimmed[i+1] == '>')))
                                    i++;
                                i++;
                                emit(out, line, docCol + os, static_cast<uint32_t>(i - os), SemanticTokenType::Operator);
                                continue;
                            }
                            if (std::isalnum(trimmed[i]) || trimmed[i] == '_') {
                                size_t is = i;
                                while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_')) i++;
                                emit(out, line, docCol + is, static_cast<uint32_t>(i - is), SemanticTokenType::Variable);
                                continue;
                            }
                            i++;
                        }
                    }
                }
            }
        } else if (directive == "#ifdef" || directive == "#ifndef") {
            auto argStart = trimmed.find_first_not_of(" \t", kwEnd);
            if (argStart != std::string::npos) {
                auto argEnd = trimmed.find_first_of(" \t", argStart);
                if (argEnd == std::string::npos) argEnd = trimmed.size();
                emit(out, line, docCol + argStart, static_cast<uint32_t>(argEnd - argStart),
                     SemanticTokenType::Macro);
            }
        } else if (directive == "#if" || directive == "#elif") {
            auto exprStart = trimmed.find_first_not_of(" \t", kwEnd);
            if (exprStart != std::string::npos) {
                for (size_t i = exprStart; i < trimmed.size();) {
                    if (std::isspace(trimmed[i])) { i++; continue; }
                    if (std::isdigit(trimmed[i])) {
                        size_t ns = i;
                        while (i < trimmed.size() && (std::isxdigit(trimmed[i]) || trimmed[i] == 'x' || trimmed[i] == 'X')) i++;
                        emit(out, line, docCol + ns, static_cast<uint32_t>(i - ns), SemanticTokenType::Number);
                        continue;
                    }
                    if (trimmed[i] == '(' || trimmed[i] == ')' || trimmed[i] == '+' || trimmed[i] == '-' ||
                        trimmed[i] == '*' || trimmed[i] == '/' || trimmed[i] == '&' || trimmed[i] == '|' ||
                        trimmed[i] == '^' || trimmed[i] == '~' || trimmed[i] == '!' ||
                        trimmed[i] == '<' || trimmed[i] == '>' || trimmed[i] == '=') {
                        i++;
                        emit(out, line, docCol + i - 1, 1, SemanticTokenType::Operator);
                        continue;
                    }
                    if (std::isalnum(trimmed[i]) || trimmed[i] == '_') {
                        size_t is = i;
                        while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_')) i++;
                        emit(out, line, docCol + is, static_cast<uint32_t>(i - is), SemanticTokenType::Macro);
                        continue;
                    }
                    if (trimmed[i] == '"') {
                        size_t ss = i;
                        i++;
                        while (i < trimmed.size() && trimmed[i] != '"') { if (trimmed[i] == '\\') i++; i++; }
                        if (i < trimmed.size()) i++;
                        emit(out, line, docCol + ss, static_cast<uint32_t>(i - ss), SemanticTokenType::String);
                        continue;
                    }
                    i++;
                }
            }
        } else if (directive == "#else" || directive == "#endif") {
            // Just the directive keyword — already emitted
        } else if (directive == "#undef") {
            auto argStart = trimmed.find_first_not_of(" \t", kwEnd);
            if (argStart != std::string::npos) {
                auto argEnd = trimmed.find_first_of(" \t", argStart);
                if (argEnd == std::string::npos) argEnd = trimmed.size();
                emit(out, line, docCol + argStart, static_cast<uint32_t>(argEnd - argStart),
                     SemanticTokenType::Macro);
            }
        } else if (directive == "#include") {
            auto argStart = trimmed.find_first_not_of(" \t", kwEnd);
            if (argStart != std::string::npos && argStart < trimmed.size()) {
                if (trimmed[argStart] == '<' || trimmed[argStart] == '"') {
                    char closeChar = (trimmed[argStart] == '<') ? '>' : '"';
                    auto argEnd = trimmed.find(closeChar, argStart + 1);
                    if (argEnd == std::string::npos) argEnd = trimmed.size();
                    else argEnd++;
                    emit(out, line, docCol + argStart, static_cast<uint32_t>(argEnd - argStart),
                         SemanticTokenType::String);
                }
            }
        } else if (directive == "#pragma" || directive == "#error" || directive == "#warning" ||
                   directive == "#line" || directive == "#") {
            auto rest = trimmed.find_first_not_of(" \t", kwEnd);
            if (rest != std::string::npos)
                emit(out, line, docCol + rest, static_cast<uint32_t>(trimmed.size() - rest),
                     SemanticTokenType::String);
        }
        return;
    }

    // ── Non-directive line within c_macro block ─────────────────────
    for (size_t i = firstNonSpace; i < text.size();) {
        if (std::isspace(text[i])) { i++; continue; }
        if (text[i] == '"') {
            size_t ss = i; i++;
            while (i < text.size() && text[i] != '"') { if (text[i] == '\\') i++; i++; }
            if (i < text.size()) i++;
            emit(out, line, baseCol + ss, static_cast<uint32_t>(i - ss), SemanticTokenType::String);
            continue;
        }
        if (text[i] == '\'') {
            size_t cs = i; i++;
            while (i < text.size() && text[i] != '\'') { if (text[i] == '\\') i++; i++; }
            if (i < text.size()) i++;
            emit(out, line, baseCol + cs, static_cast<uint32_t>(i - cs), SemanticTokenType::String);
            continue;
        }
        if (std::isdigit(text[i]) || (text[i] == '-' && i+1 < text.size() && std::isdigit(text[i+1]))) {
            size_t ns = i;
            if (text[i] == '-') i++;
            while (i < text.size() && (std::isxdigit(text[i]) || text[i] == '.')) i++;
            emit(out, line, baseCol + ns, static_cast<uint32_t>(i - ns), SemanticTokenType::Number);
            continue;
        }
        if (text[i] == '(' || text[i] == ')' || text[i] == '{' || text[i] == '}' ||
            text[i] == '+' || text[i] == '-' || text[i] == '*' || text[i] == '/' ||
            text[i] == '=' || text[i] == ';' || text[i] == ',' || text[i] == ':' ||
            text[i] == '&' || text[i] == '|' || text[i] == '^' || text[i] == '~' ||
            text[i] == '<' || text[i] == '>' || text[i] == '!' || text[i] == '%' ||
            text[i] == '[' || text[i] == ']') {
            size_t os = i;
            if (i+1 < text.size() && ((text[i] == '<' && text[i+1] == '<') ||
                (text[i] == '>' && text[i+1] == '>') ||
                (text[i] == '-' && text[i+1] == '>') ||
                (text[i] == '+' && text[i+1] == '+') ||
                (text[i] == '-' && text[i+1] == '-')))
                i++;
            i++;
            emit(out, line, baseCol + os, static_cast<uint32_t>(i - os), SemanticTokenType::Operator);
            continue;
        }
        if (std::isalnum(text[i]) || text[i] == '_') {
            size_t is = i;
            while (i < text.size() && (std::isalnum(text[i]) || text[i] == '_')) i++;
            emit(out, line, baseCol + is, static_cast<uint32_t>(i - is), SemanticTokenType::Variable);
            continue;
        }
        i++;
    }
}

static void emitCMacroSubTokens(std::vector<RawSemanticToken>& out,
                                uint32_t line, uint32_t col,
                                const std::string& text) {
    // text = "c_macro { ... }"
    // Emit "c_macro" keyword
    emit(out, line, col, 7, SemanticTokenType::Keyword,
         static_cast<uint32_t>(SemanticTokenMod::Readonly));

    // Find opening brace
    auto brace = text.find('{');
    if (brace == std::string::npos) return;

    // Track position past "c_macro"
    uint32_t curLine = line;
    uint32_t curCol = col + 7;

    // Skip whitespace between "c_macro" and "{"
    for (size_t p = 7; p < brace; p++) {
        if (text[p] == '\n') { curLine++; curCol = 0; }
        else if (text[p] != '\r') curCol++;
    }

    // Emit "{"
    emit(out, curLine, curCol, 1, SemanticTokenType::Operator);
    uint32_t bracketLine = curLine;
    uint32_t bracketCol = curCol;

    // Content is text between first '{' and last '}'
    // Find the matching closing brace (the last one that balances)
    int depth = 0;
    size_t closeBrace = std::string::npos;
    for (size_t i = brace; i < text.size(); i++) {
        if (text[i] == '{') depth++;
        else if (text[i] == '}') {
            depth--;
            if (depth == 0) { closeBrace = i; break; }
        }
    }
    if (closeBrace == std::string::npos) {
        emit(out, bracketLine, bracketCol, 1, SemanticTokenType::Operator); // already emitted
        return;
    }

    std::string innerText = text.substr(brace + 1, closeBrace - brace - 1);

    // Track position inside the block
    curLine = bracketLine;
    curCol = bracketCol + 1; // after "{"

    std::istringstream stream(innerText);
    std::string innerLine;
    while (std::getline(stream, innerLine)) {
        // Check for line comment
        auto lineComment = innerLine.find("//");
        size_t blockCommentStart = innerLine.find("/*");
        size_t processEnd = innerLine.size();

        if (lineComment != std::string::npos)
            processEnd = std::min(processEnd, lineComment);
        // For block comments, we handle them below; still process tokens before them

        std::string beforeComment = innerLine.substr(0, processEnd);
        auto first = beforeComment.find_first_not_of(" \t\r");
        if (first != std::string::npos)
            emitCMacroLineTokens(out, curLine, curCol, beforeComment, static_cast<uint32_t>(first));

        // Emit line comment
        if (lineComment != std::string::npos) {
            emit(out, curLine, curCol + static_cast<uint32_t>(lineComment),
                 static_cast<uint32_t>(innerLine.size() - lineComment),
                 SemanticTokenType::Comment);
        }

        // Emit block comment
        if (blockCommentStart != std::string::npos) {
            // Block comment may span multiple lines
            size_t bcEnd = innerLine.find("*/", blockCommentStart + 2);
            if (bcEnd != std::string::npos) {
                bcEnd += 2;
                emit(out, curLine, curCol + static_cast<uint32_t>(blockCommentStart),
                     static_cast<uint32_t>(bcEnd - blockCommentStart),
                     SemanticTokenType::Comment);
            } else {
                // Block comment continues to next line — emit this portion
                emit(out, curLine, curCol + static_cast<uint32_t>(blockCommentStart),
                     static_cast<uint32_t>(innerLine.size() - blockCommentStart),
                     SemanticTokenType::Comment);
            }
        }

        curLine++;
        curCol = 0;
    }

    // Emit "}"
    // Find the line/col of closeBrace within the full text
    {
        uint32_t scanL = line;
        uint32_t scanC = col;
        for (size_t i = 0; i <= closeBrace && i < text.size(); i++) {
            if (i == closeBrace) {
                emit(out, scanL, scanC, 1, SemanticTokenType::Operator);
                break;
            }
            if (text[i] == '\n') { scanL++; scanC = 0; }
            else if (text[i] != '\r') scanC++;
        }
    }
}

// asm_b block — emit full sub-token highlighting for assembly content
// (multi-assembler: NASM, GAS/AT&T, MASM, WASM, etc.)

static std::unordered_set<std::string> asmInstructions;
static std::unordered_set<std::string> asmRegisters;
static std::unordered_set<std::string> asmDirectives;
static std::once_flag asmInitFlag;

static void initAsmSets() {
    std::call_once(asmInitFlag, []() {

    // ── General-purpose x86-64 instructions ──────────────────────────
    const char* instrs[] = {
        "mov", "push", "pop", "xchg", "cmove", "cmovne", "cmovb", "cmovae",
        "cmovbe", "cmova", "cmovl", "cmovge", "cmovle", "cmovg",
        "movsx", "movzx", "movsxd", "movd", "movq",
        "add", "sub", "mul", "imul", "div", "idiv",
        "inc", "dec", "neg", "not",
        "and", "or", "xor", "test",
        "shl", "shr", "sal", "sar", "rol", "ror", "rcl", "rcr",
        "shld", "shrd",
        "cmp", "cmpsb", "cmpsw", "cmpsd", "cmpsq",
        "jmp", "call", "ret", "retf", "retn",
        "ja", "jae", "jb", "jbe", "jc", "jcxz", "jecxz", "jrcxz",
        "je", "jg", "jge", "jl", "jle", "jna", "jnae", "jnb",
        "jnbe", "jnc", "jne", "jng", "jnge", "jnl", "jnle", "jno",
        "jnp", "jns", "jnz", "jo", "jp", "jpe", "jpo", "js", "jz",
        "loop", "loope", "loopne", "loopnz", "loopz",
        "nop", "pause", "lea", "lodsb", "lodsw", "lodsd", "lodsq",
        "stosb", "stosw", "stosd", "stosq",
        "movsb", "movsw", "movsd", "movsq",
        "scasb", "scasw", "scasd", "scasq",
        "rep", "repe", "repne", "repnz", "repz",
        "iret", "iretd", "iretq",
        "syscall", "sysret", "sysenter", "sysexit",
        "int", "int3", "into", "bound",
        "cli", "sti", "cld", "std",
        "lahf", "sahf", "pushf", "popf", "pushfd", "popfd", "pushfq", "popfq",
        "cbw", "cwde", "cdqe", "cwd", "cdq", "cqo",
        "daa", "das", "aaa", "aas", "aam", "aad",
        "xlat", "xlatb",
        "bsf", "bsr", "bt", "bts", "btr", "btc",
        "bswap",
        "sete", "setne", "setb", "setae", "setbe", "seta",
        "setl", "setge", "setle", "setg", "sets", "setns",
        "seto", "setno", "setp", "setnp",
        "cpuid", "rdtsc", "rdtscp", "rdmsr", "wrmsr",
        "rdrand", "rdseed",
        "xtest", "xbegin", "xend", "xabort",
        "xadd", "cmpxchg", "cmpxchg8b", "cmpxchg16b",
        "xacquire", "xrelease",
        "lfence", "mfence", "sfence",
        "tpause", "umwait", "umonitor",
        "clflush", "clflushopt", "clwb",
        "prefetchnta", "prefetcht0", "prefetcht1", "prefetcht2",
        "cldemote", "movdiri", "movdir64b",
        "encls", "enclu", "enclv",
        "invept", "invvpid", "invpcid",
        "vmcall", "vmfunc", "vmlaunch", "vmresume", "vmxoff",
        "vmread", "vmwrite",
        "vmclear", "vmptrld", "vmptrst",
        "getsec",
        "monitor", "mwait", "mwaitx",
        "clac", "stac",
        "wrfsbase", "wrgsbase", "rdfsbase", "rdgsbase",
        "adc", "sbb",
        // ── x87 FPU ──────────────────────────────────────────────────
        "fld", "fst", "fstp", "fild", "fist", "fistp", "fisttp",
        "fadd", "fsub", "fmul", "fdiv", "faddp", "fsubp", "fmulp", "fdivp",
        "fiadd", "fisub", "fimul", "fidiv",
        "fchs", "fabs", "fsqrt", "frndint",
        "fcom", "fcomp", "fcompp", "fucom", "fucomp", "fucompp",
        "ficom", "ficomp",
        "ftst", "fxam", "fldz", "fld1", "fldpi", "fldl2e", "fldl2t", "fldlg2", "fldln2",
        "fprem", "fprem1", "fscale", "fxch",
        "fincstp", "fdecstp",
        "fstenv", "fldenv", "fsave", "frstor", "fxsave", "fxrstor",
        "fxsave64", "fxrstor64",
        "fwait", "fnop", "fclex", "ffree", "finit", "fninit",
        "fcmovb", "fcmove", "fcmovbe", "fcmovu", "fcmovnb", "fcmovne", "fcmovnbe", "fcmovnu",
        "fnsave", "fnstcw", "fnstsw", "fnstenv",
        // ── MMX ───────────────────────────────────────────────────────
        "packsswb", "packssdw", "packuswb",
        "paddb", "paddw", "paddd", "paddq",
        "paddsb", "paddsw", "paddusb", "paddusw",
        "psubb", "psubw", "psubd", "psubq",
        "psubsb", "psubsw", "psubusb", "psubusw",
        "pmullw", "pmulhw", "pmulhuw", "pmaddwd",
        "pand", "por", "pxor", "pandn",
        "psllw", "pslld", "psllq", "psrlw", "psrld", "psrlq", "psraw", "psrad",
        "pcmpeqb", "pcmpeqw", "pcmpeqd", "pcmpgtb", "pcmpgtw", "pcmpgtd",
        "punpckhbw", "punpckhwd", "punpckhdq",
        "punpcklbw", "punpcklwd", "punpckldq",
        "emms",
        // ── SSE ───────────────────────────────────────────────────────
        "movaps", "movups", "movss", "movlps", "movhps", "movlhps", "movhlps",
        "movmskps", "movntps",
        "addps", "addss", "subps", "subss", "mulps", "mulss", "divps", "divss",
        "rcpps", "rcpss", "sqrtps", "sqrtss", "rsqrtps", "rsqrtss",
        "minps", "minss", "maxps", "maxss",
        "andps", "andnps", "orps", "xorps",
        "cmpps", "cmpss",
        "comiss", "ucomiss",
        "shufps", "unpcklps", "unpckhps",
        "cvtpi2ps", "cvtps2pi", "cvtsi2ss", "cvtss2si",
        "cvttps2pi", "cvttss2si",
        "pextrw", "pinsrw", "pmaxsw", "pmaxub", "pminsw", "pminub",
        "pmovmskb", "pmulhuw",
        "pshufw", "maskmovq",
        "ldmxcsr", "stmxcsr",
        // ── SSE2 ──────────────────────────────────────────────────────
        "movapd", "movupd", "movsd", "movlpd", "movhpd", "movmskpd",
        "addpd", "addsd", "subpd", "subsd", "mulpd", "mulsd",
        "divpd", "divsd", "sqrtpd", "sqrtsd",
        "minpd", "minsd", "maxpd", "maxsd",
        "andpd", "andnpd", "orpd", "xorpd",
        "cmppd", "cmpsd",
        "comisd", "ucomisd",
        "shufpd", "unpcklpd", "unpckhpd",
        "cvtpd2pi", "cvtpi2pd", "cvtsi2sd", "cvtsd2si",
        "cvttpd2pi", "cvttsd2si",
        "cvtpd2dq", "cvtdq2pd", "cvtps2dq", "cvtdq2ps",
        "cvttpd2dq", "cvttps2dq",
        "cvtss2sd", "cvtsd2ss",
        "movdqa", "movdqu", "movntdq", "movnti", "movntpd",
        "pmuludq",
        "pshuflw", "pshufhw", "pshufd",
        "pclmulqdq",
        "punpcklqdq", "punpckhqdq",
        "maskmovdqu",
        // ── SSE3 ──────────────────────────────────────────────────────
        "addsubps", "addsubpd", "movsldup", "movshdup", "movddup",
        "haddps", "haddpd", "hsubps", "hsubpd",
        "lddqu",
        // ── SSSE3 ─────────────────────────────────────────────────────
        "pshufb", "phaddw", "phaddd", "phaddsw",
        "phsubw", "phsubd", "phsubsw",
        "psignb", "psignw", "psignd",
        "pmulhrsw", "pabsb", "pabsw", "pabsd",
        "palignr",
        // ── SSE4.1 ────────────────────────────────────────────────────
        "pmulld", "phminposuw", "pminsb", "pminsd", "pminuw", "pminud",
        "pmaxsb", "pmaxsd", "pmaxuw", "pmaxud",
        "pmovsxbw", "pmovsxbd", "pmovsxbq",
        "pmovsxwd", "pmovsxwq", "pmovsxdq",
        "pmovzxbw", "pmovzxbd", "pmovzxbq",
        "pmovzxwd", "pmovzxwq", "pmovzxdq",
        "pcmpeqq", "packusdw",
        "pmuldq",
        "insertps", "extractps",
        "pextrb", "pextrd", "pextrq",
        "pinsrb", "pinsrd", "pinsrq",
        "roundps", "roundpd", "roundss", "roundsd",
        "dpps", "dppd",
        "mpsadbw", "blendps", "blendpd", "blendvps", "blendvpd",
        "ptest", "pcmpgtq",
        // ── SSE4.2 ────────────────────────────────────────────────────
        "pcmpestri", "pcmpestrm", "pcmpistri", "pcmpistrm",
        "crc32", "popcnt",
        // ── AES + SHA ─────────────────────────────────────────────────
        "aesenc", "aesenclast", "aesdec", "aesdeclast",
        "aesimc", "aeskeygenassist",
        "sha1rnds4", "sha1nexte", "sha1msg1", "sha1msg2",
        "sha256rnds2", "sha256msg1", "sha256msg2",
        // ── AVX ───────────────────────────────────────────────────────
        "vaddps", "vaddpd", "vaddss", "vaddsd",
        "vsubps", "vsubpd", "vsubss", "vsubsd",
        "vmulps", "vmulpd", "vmulss", "vmulsd",
        "vdivps", "vdivpd", "vdivss", "vdivsd",
        "vsqrtps", "vsqrtpd", "vsqrtss", "vsqrtsd",
        "vrsqrtps", "vrsqrtss", "vrcpps", "vrcpss",
        "vminps", "vminpd", "vminss", "vminsd",
        "vmaxps", "vmaxpd", "vmaxss", "vmaxsd",
        "vandps", "vandpd", "vandnps", "vandnpd",
        "vorps", "vorpd", "vxorps", "vxorpd",
        "vhaddps", "vhaddpd", "vhsubps", "vhsubpd",
        "vaddsubps", "vaddsubpd",
        "vmovaps", "vmovapd", "vmovups", "vmovupd",
        "vmovss", "vmovsd",
        "vmovdqa", "vmovdqu", "vmovntps", "vmovntpd", "vmovntdq",
        "vpermilps", "vpermilpd", "vperm2f128",
        "vinsertps", "vextractps",
        "vinsertf128", "vextractf128", "vinserti128", "vextracti128",
        "vblendps", "vblendpd", "vblendvps", "vblendvpd",
        "vdpps", "vdppd",
        "vroundps", "vroundpd", "vroundss", "vroundsd",
        "vtestps", "vtestpd",
        "vzeroupper", "vzeroall",
        "vbroadcastss", "vbroadcastsd", "vbroadcastf128",
        "vmaskmovps", "vmaskmovpd",
        "vldmxcsr", "vstmxcsr",
        // ── AVX2 ──────────────────────────────────────────────────────
        "vpermq", "vpermd", "vpermps", "vpermpd",
        "vperm2i128",
        "vbroadcastsi128",
        "vpblendd", "vpblendvb",
        "vgatherdd", "vgatherdq", "vgatherqd", "vgatherqq",
        "vpgatherdd", "vpgatherdq", "vpgatherqd", "vpgatherqq",
        "vpmaskmovd", "vpmaskmovq",
        "vpsllvd", "vpsllvq", "vpsrlvd", "vpsrlvq", "vpsravd",
        "vpaddb", "vpaddw", "vpaddd", "vpaddq",
        "vpsubb", "vpsubw", "vpsubd", "vpsubq",
        "vpmullw", "vpmulld", "vpmuludq", "vpmuldq",
        "vpand", "vpor", "vpxor", "vpandn",
        "vpsllw", "vpslld", "vpsllq", "vpsrlw", "vpsrld", "vpsrlq", "vpsraw", "vpsrad",
        "vpcmpeqb", "vpcmpeqw", "vpcmpeqd", "vpcmpeqq",
        "vpcmpgtb", "vpcmpgtw", "vpcmpgtd", "vpcmpgtq",
        // ── AVX-512 ───────────────────────────────────────────────────
        "vpternlogd", "vpternlogq",
        "vblendmps", "vblendmpd", "vpblendmd", "vpblendmq",
        "vcompressps", "vcompresspd", "vpcompressd", "vpcompressq",
        "vexpandps", "vexpandpd", "vpexpandd", "vpexpandq",
        "vconflictps", "vconflictd", "vconflictq",
        "vplzcntd", "vplzcntq",
        "vpopcntd", "vpopcntq",
        "vpermw", "vpermt2b", "vpermt2w", "vpermt2d", "vpermt2q",
        "vpermb",
        "vpmovdb", "vpmovdw", "vpmovqb", "vpmovqw", "vpmovqd",
        "vpmovsdb", "vpmovsdw", "vpmovsqb", "vpmovsqw", "vpmovsqd",
        "vpmovusdb", "vpmovusdw", "vpmovusqb", "vpmovusqw", "vpmovusqd",
        "vprold", "vprolq", "vprolvd", "vprolvq",
        "vprord", "vprorq", "vprorvd", "vprorvq",
        "vpsravw", "vpsllvw", "vpsrlvw",
        "vscalefps", "vscalefpd", "vscalefss", "vscalefsd",
        "vmovdqa32", "vmovdqa64", "vmovdqu32", "vmovdqu64",
        "vgetexpps", "vgetexppd", "vgetexpss", "vgetexpsd",
        "vgetmantps", "vgetmantpd", "vgetmantss", "vgetmantsd",
        "vfixupimmps", "vfixupimmpd", "vfixupimmss", "vfixupimmsd",
        "vreduceps", "vreducepd", "vreducess", "vreducesd",
        "vrangeps", "vrangepd", "vrangess", "vrangesd",
        "valignd", "valignq",
        "vdbpsadbw",
        // ── BMI / ADX ────────────────────────────────────────────────
        "andn", "bextr", "blsi", "blsmsk", "blsr",
        "tzcnt", "lzcnt",
        "mulx", "rorx", "sarx", "shlx", "shrx",
        "pdep", "pext",
        "adox", "adcx",
    };
    for (auto* s : instrs) asmInstructions.insert(s);

    // ── Registers (x86-64 architecture - same for all assemblers) ─────
    const char* regs[] = {
        "rax", "rbx", "rcx", "rdx", "rdi", "rsi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ebx", "ecx", "edx", "edi", "esi", "ebp", "esp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
        "al", "bl", "cl", "dl", "sil", "dil", "bpl", "spl",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
        "ah", "bh", "ch", "dh",
        "rip", "eip",
        "cs", "ds", "es", "fs", "gs", "ss",
        "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7",
        "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
        "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
        "xmm16", "xmm17", "xmm18", "xmm19", "xmm20", "xmm21", "xmm22", "xmm23",
        "xmm24", "xmm25", "xmm26", "xmm27", "xmm28", "xmm29", "xmm30", "xmm31",
        "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
        "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
        "ymm16", "ymm17", "ymm18", "ymm19", "ymm20", "ymm21", "ymm22", "ymm23",
        "ymm24", "ymm25", "ymm26", "ymm27", "ymm28", "ymm29", "ymm30", "ymm31",
        "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7",
        "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15",
        "zmm16", "zmm17", "zmm18", "zmm19", "zmm20", "zmm21", "zmm22", "zmm23",
        "zmm24", "zmm25", "zmm26", "zmm27", "zmm28", "zmm29", "zmm30", "zmm31",
        "k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7",
        "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7", "cr8",
        "dr0", "dr1", "dr2", "dr3", "dr4", "dr5", "dr6", "dr7",
        "tr0", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7",
        "bnd0", "bnd1", "bnd2", "bnd3",
        "tmm0", "tmm1", "tmm2", "tmm3", "tmm4", "tmm5", "tmm6", "tmm7",
        "tile0", "tile1", "tile2", "tile3", "tile4", "tile5", "tile6", "tile7",
        "pkru",
        "mxcsr",
    };
    for (auto* s : regs) asmRegisters.insert(s);

    // ── Assembler directives (NASM, GAS, MASM, WASM) ─────────────────
    const char* dirs[] = {
        "section", "segment", "global", "extern", "common",
        "align", "bits", "default", "cpu",
        "absolute", "virtual", "group",
        "struc", "endstruc", "istruc", "at",
        "macro", "endm", "endmacro",
        "org", "origin",
        "include", "incbin",
        "%define", "%undef", "%assign", "%strlen", "%substr",
        "%if", "%elif", "%else", "%endif", "%ifdef", "%ifndef",
        "%ifmacro", "%ifnmacro", "%ifctoken", "%ifnctoken",
        "%macro", "%imacro", "%endmacro",
        "%rotate", "%rep", "%endrep", "%exitrep",
        "%push", "%pop", "%repl",
        "%stacksize", "%local",
        "%xdefine", "%ixdefine",
        "%pathsearch", "%fatal", "%error", "%warning",
        ".section", ".text", ".data", ".bss", ".globl", ".global",
        ".extern", ".type", ".size", ".align", ".p2align", ".balign",
        ".byte", ".word", ".short", ".int", ".long", ".quad", ".octa",
        ".float", ".single", ".double", ".tfloat",
        ".ascii", ".asciz", ".string", ".zero", ".skip", ".space",
        ".fill", ".org", ".lcomm", ".comm",
        ".struct", ".union",
        ".macro", ".endm", ".exitm",
        ".irp", ".irpc", ".rept", ".endr",
        ".if", ".else", ".endif", ".ifdef", ".ifndef", ".ifb", ".ifnb",
        ".ifc", ".ifnc", ".ifeqs", ".ifnes",
        ".include", ".incbin",
        ".set", ".equ", ".equiv", ".eqv",
        ".file", ".loc", ".cfi_startproc", ".cfi_endproc",
        ".cfi_def_cfa", ".cfi_offset", ".cfi_rel_offset",
        ".cfi_register", ".cfi_restore", ".cfi_remember_state",
        ".cfi_restore_state", ".cfi_same_value",
        ".cfi_signal_frame", ".cfi_window_save",
        ".cfi_b_key_frame",
        ".ident", ".note", ".pushsection", ".popsection", ".previous",
        ".altmacro", ".noaltmacro",
        ".att_syntax", ".intel_syntax",
        ".code16", ".code16gcc", ".code32", ".code64",
        ".arch", ".cpu",
        ".hidden", ".protected", ".internal", ".weak", ".local",
        ".thumb", ".thumb_func", ".syntax",
        ".code", ".data", ".stack", ".const",
        "assume", "public", "proc", "endp", "proto",
        "model", "flat", "tiny", "small", "medium", "compact", "large", "huge",
        ".386", ".486", ".586", ".686", ".xmm",
        ".mmx", ".k3d",
        "offset", "ptr",
        "db", "dw", "dd", "dq", "dt", "do", "dy", "dz",
        "resb", "resw", "resd", "resq", "rest", "reso", "resy", "resz",
        "equ", "times", "dup",
        "byte", "word", "dword", "qword", "tbyte", "oword", "yword", "zword",
        "use16", "use32", "use64",
    };
    for (auto* s : dirs) asmDirectives.insert(s);
    }); }

// Emit sub-tokens for a line of assembly content.
// Handles NASM, GAS (AT&T/Intel), MASM, WASM syntaxes.
static void emitAsmLineTokens(std::vector<RawSemanticToken>& out,
                               uint32_t line, uint32_t baseCol,
                               const std::string& text,
                               uint32_t firstNonSpace) {
    std::string t = text.substr(firstNonSpace);
    uint32_t col0 = baseCol + firstNonSpace;

    size_t i = 0;
    while (i < t.size()) {
        if (std::isspace(static_cast<unsigned char>(t[i]))) { i++; continue; }

        // Comment styles: ; (NASM/MASM/GAS), # (GAS), // (GAS C-style), /* */ (GAS)
        if (t[i] == ';') {
            emit(out, line, col0 + i, static_cast<uint32_t>(t.size() - i),
                 SemanticTokenType::Comment);
            return;
        }
        if (t[i] == '#') {
            emit(out, line, col0 + i, static_cast<uint32_t>(t.size() - i),
                 SemanticTokenType::Comment);
            return;
        }
        if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '/') {
            emit(out, line, col0 + i, static_cast<uint32_t>(t.size() - i),
                 SemanticTokenType::Comment);
            return;
        }
        if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '*') {
            size_t close = t.find("*/", i + 2);
            if (close != std::string::npos)
                emit(out, line, col0 + i, static_cast<uint32_t>(close - i + 2),
                     SemanticTokenType::Comment);
            else
                emit(out, line, col0 + i, static_cast<uint32_t>(t.size() - i),
                     SemanticTokenType::Comment);
            return;
        }

        // Strings: "..." or '...'
        if (t[i] == '"' || t[i] == '\'') {
            char q = t[i];
            size_t start = i;
            i++;
            while (i < t.size() && t[i] != q) {
                if (t[i] == '\\' && i + 1 < t.size()) i += 2;
                else i++;
            }
            if (i < t.size()) i++;
            emit(out, line, col0 + start, static_cast<uint32_t>(i - start),
                 SemanticTokenType::String);
            continue;
        }

        // Numbers: decimal, hex (0x, $, h suffix), octal (0o, q suffix), binary (0b, b suffix)
        if (std::isdigit(static_cast<unsigned char>(t[i])) ||
            t[i] == '$' ||
            (t[i] == '.' && i + 1 < t.size() && std::isdigit(static_cast<unsigned char>(t[i+1])))) {
            size_t start = i;
            if (t[i] == '$') {
                i++;
                while (i < t.size() && std::isxdigit(static_cast<unsigned char>(t[i]))) i++;
            } else if (i + 2 < t.size() && t[i] == '0') {
                char nxt = t[i+1];
                if (nxt == 'x' || nxt == 'X') {
                    i += 2;
                    while (i < t.size() && std::isxdigit(static_cast<unsigned char>(t[i]))) i++;
                } else if (nxt == 'b' || nxt == 'B') {
                    i += 2;
                    while (i < t.size() && (t[i] == '0' || t[i] == '1')) i++;
                } else if (nxt == 'o' || nxt == 'O') {
                    i += 2;
                    while (i < t.size() && t[i] >= '0' && t[i] <= '7') i++;
                } else {
                    while (i < t.size() && (std::isxdigit(static_cast<unsigned char>(t[i])) || t[i] == '.')) i++;
                }
            } else {
                while (i < t.size() && (std::isxdigit(static_cast<unsigned char>(t[i])) || t[i] == '.')) i++;
                // Check for suffix radix: h (hex), o/q (octal), b (binary), d (decimal)
                if (i < t.size() && (t[i] == 'h' || t[i] == 'H' ||
                    t[i] == 'o' || t[i] == 'O' || t[i] == 'q' || t[i] == 'Q' ||
                    t[i] == 'b' || t[i] == 'B' || t[i] == 'd' || t[i] == 'D'))
                    i++;
            }
            emit(out, line, col0 + start, static_cast<uint32_t>(i - start),
                 SemanticTokenType::Number);
            continue;
        }

        // Identifiers, labels, keywords
        if (std::isalpha(static_cast<unsigned char>(t[i])) || t[i] == '_' ||
            t[i] == '.' || t[i] == '@' || t[i] == '?' ||
            (t[i] == '%' && i + 1 < t.size() && 
             (std::isalnum(static_cast<unsigned char>(t[i+1])) || t[i+1] == '_' || t[i+1] == '%'))) {
            size_t idStart = i;
            i++;
            while (i < t.size() && (std::isalnum(static_cast<unsigned char>(t[i])) ||
                   t[i] == '_' || t[i] == '.' || t[i] == '@' || t[i] == '?' || t[i] == '$'))
                i++;

            std::string word = t.substr(idStart, i - idStart);

            // Check if followed by ':' (label)
            size_t after = i;
            while (after < t.size() && std::isspace(static_cast<unsigned char>(t[after])))
                after++;
            if (after < t.size() && t[after] == ':') {
                emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                     SemanticTokenType::Function,
                     static_cast<uint32_t>(SemanticTokenMod::Declaration));
                emit(out, line, col0 + after, 1, SemanticTokenType::Operator);
                i = after + 1;
                continue;
            }

            // Classify: check directives FIRST with % preserved (for NASM %define etc.)
            if (asmDirectives.count(word) > 0) {
                emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                     SemanticTokenType::Macro);
            } else {
                // Strip GAS %reg prefix for register/instruction lookup
                std::string lookupWord = word;
                size_t pctCount = 0;
                while (pctCount < lookupWord.size() && lookupWord[pctCount] == '%')
                    pctCount++;
                if (pctCount > 0)
                    lookupWord = lookupWord.substr(pctCount);

                if ((!lookupWord.empty() && lookupWord[0] == '.') ||
                    asmDirectives.count(lookupWord) > 0) {
                    emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                         SemanticTokenType::Macro);
                } else if (asmInstructions.count(lookupWord) > 0) {
                    emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                         SemanticTokenType::Keyword);
                } else if (asmRegisters.count(lookupWord) > 0) {
                    emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                         SemanticTokenType::Variable);
                } else {
                    emit(out, line, col0 + idStart, static_cast<uint32_t>(i - idStart),
                         SemanticTokenType::Variable);
                }
            }
            continue;
        }

        // Operators and punctuation
        if (t[i] == ':' || t[i] == ',' || t[i] == '[' || t[i] == ']' ||
            t[i] == '+' || t[i] == '-' || t[i] == '*' || t[i] == '/' ||
            t[i] == '%' || t[i] == '~' || t[i] == '&' || t[i] == '|' ||
            t[i] == '^' || t[i] == '(' || t[i] == ')' || t[i] == '!' ||
            t[i] == '=' || t[i] == '$' || t[i] == '@' || t[i] == '?' ||
            t[i] == '{' || t[i] == '}') {
            size_t opLen = 1;
            if (i + 1 < t.size()) {
                char c1 = t[i], c2 = t[i+1];
                if ((c1 == '<' && c2 == '<') || (c1 == '>' && c2 == '>') ||
                    (c1 == '-' && c2 == '>') || (c1 == '<' && c2 == '>') ||
                    (c1 == '=' && c2 == '=') || (c1 == '!' && c2 == '=') ||
                    (c1 == '&' && c2 == '&') || (c1 == '|' && c2 == '|') ||
                    (c1 == '+' && c2 == '+') || (c1 == '-' && c2 == '-'))
                    opLen = 2;
            }
            emit(out, line, col0 + i, opLen, SemanticTokenType::Operator);
            i += opLen;
            continue;
        }

        i++;
    }
}

static void emitAsmBSubTokens(std::vector<RawSemanticToken>& out,
                               uint32_t line, uint32_t col,
                               const std::string& text) {
    initAsmSets();

    // Emit "asm_b" keyword
    emit(out, line, col, 5, SemanticTokenType::Keyword,
         static_cast<uint32_t>(SemanticTokenMod::Readonly));

    // Find filename between quotes: asm_b "filename" { ... }
    size_t firstQuote = text.find('"', 6);
    if (firstQuote == std::string::npos) return;
    size_t secondQuote = text.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) return;

    // Track position past "asm_b" keyword
    uint32_t curLine = line;
    uint32_t curCol = col;  // We already emitted asm_b, but we need col for position tracking

    // Advance past "asm_b"
    for (size_t p = 0; p < 5; p++) {
        if (text[p] == '\n') { curLine++; curCol = 0; }
        else if (text[p] != '\r') curCol++;
    }

    // Find opening brace
    size_t brace = text.find('{', secondQuote);
    if (brace == std::string::npos) return;

    // Advance past filename and whitespace to reach '{'
    for (size_t p = 5; p < brace; p++) {
        if (text[p] == '\n') { curLine++; curCol = 0; }
        else if (text[p] != '\r') curCol++;
    }

    // Emit "{" as operator
    emit(out, curLine, curCol, 1, SemanticTokenType::Operator);
    uint32_t bracketLine = curLine;
    uint32_t bracketCol = curCol;

    // Find matching closing brace
    int depth = 0;
    size_t closeBrace = std::string::npos;
    for (size_t i = brace; i < text.size(); i++) {
        if (text[i] == '{') depth++;
        else if (text[i] == '}') {
            depth--;
            if (depth == 0) { closeBrace = i; break; }
        }
    }
    if (closeBrace == std::string::npos) return;

    // Extract inner content between braces
    std::string innerText = text.substr(brace + 1, closeBrace - brace - 1);

    curLine = bracketLine;
    curCol = bracketCol + 1;

    // Process each line
    std::istringstream stream(innerText);
    std::string innerLine;
    while (std::getline(stream, innerLine)) {
        // Find comment start: need to scan around strings
        size_t semi = std::string::npos;
        size_t hash = std::string::npos;
        size_t slsl = std::string::npos;

        bool indq = false, insq = false;
        for (size_t si = 0; si < innerLine.size(); si++) {
            if (indq) {
                if (innerLine[si] == '"' && (si == 0 || innerLine[si-1] != '\\'))
                    indq = false;
                continue;
            }
            if (insq) {
                if (innerLine[si] == '\'' && (si == 0 || innerLine[si-1] != '\\'))
                    insq = false;
                continue;
            }
            if (innerLine[si] == '"') { indq = true; continue; }
            if (innerLine[si] == '\'') { insq = true; continue; }
            if (semi == std::string::npos && innerLine[si] == ';') semi = si;
            if (hash == std::string::npos && innerLine[si] == '#') hash = si;
            if (slsl == std::string::npos && si + 1 < innerLine.size() &&
                innerLine[si] == '/' && innerLine[si+1] == '/')
                slsl = si;
        }

        size_t commentStart = std::string::npos;
        if (semi != std::string::npos) commentStart = semi;
        if (hash != std::string::npos && hash < commentStart) commentStart = hash;
        if (slsl != std::string::npos && slsl < commentStart) commentStart = slsl;

        size_t processEnd = (commentStart != std::string::npos) ? commentStart : innerLine.size();
        std::string codePart = innerLine.substr(0, processEnd);

        auto first = codePart.find_first_not_of(" \t");
        if (first != std::string::npos)
            emitAsmLineTokens(out, curLine, curCol, codePart, static_cast<uint32_t>(first));

        if (commentStart != std::string::npos) {
            emit(out, curLine, curCol + static_cast<uint32_t>(commentStart),
                 static_cast<uint32_t>(innerLine.size() - commentStart),
                 SemanticTokenType::Comment);
        }

        curLine++;
        curCol = 0;
    }

    // Emit closing "}"
    {
        uint32_t sl = line, sc = col;
        for (size_t ci = 0; ci <= closeBrace && ci < text.size(); ci++) {
            if (ci == closeBrace) {
                emit(out, sl, sc, 1, SemanticTokenType::Operator);
                break;
            }
            if (text[ci] == '\n') { sl++; sc = 0; }
            else if (text[ci] != '\r') sc++;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Collect comments (skipped by ANTLR lexer, so we scan manually)
// ═══════════════════════════════════════════════════════════════════════

static void collectComments(std::vector<RawSemanticToken>& out,
                             const std::string& source) {
    uint32_t line = 0;
    uint32_t col  = 0;
    size_t i = 0;

    while (i < source.size()) {
        if (source[i] == '\n') {
            ++line; col = 0; ++i;
            continue;
        }
        if (source[i] == '\r') {
            ++line; col = 0; ++i;
            if (i < source.size() && source[i] == '\n') ++i;
            continue;
        }

        // Line comment: // ...
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            uint32_t startCol = col;
            size_t start = i;
            while (i < source.size() && source[i] != '\n' && source[i] != '\r')
                ++i;
            uint32_t len = static_cast<uint32_t>(i - start);
            emit(out, line, startCol, len, SemanticTokenType::Comment);
            col += len;
            continue;
        }

        // Block comment: /* ... */
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            uint32_t startLine = line;
            uint32_t startCol  = col;
            size_t start = i;
            i += 2; col += 2;

            while (i < source.size()) {
                if (source[i] == '\n') {
                    // Emit the current line portion of the comment
                    uint32_t len = static_cast<uint32_t>(i - start);
                    emit(out, startLine, startCol, len, SemanticTokenType::Comment);
                    ++line; col = 0; ++i;
                    start = i;
                    startLine = line;
                    startCol = 0;
                } else if (source[i] == '\r') {
                    uint32_t len = static_cast<uint32_t>(i - start);
                    emit(out, startLine, startCol, len, SemanticTokenType::Comment);
                    ++line; col = 0; ++i;
                    if (i < source.size() && source[i] == '\n') ++i;
                    start = i;
                    startLine = line;
                    startCol = 0;
                } else if (i + 1 < source.size() &&
                           source[i] == '*' && source[i + 1] == '/') {
                    i += 2; col += 2;
                    uint32_t len = static_cast<uint32_t>(i - start);
                    emit(out, startLine, startCol, len, SemanticTokenType::Comment);
                    break;
                } else {
                    ++col; ++i;
                }
            }
            continue;
        }

        // Skip string and char literals (so we don't falsely detect // inside them)
        if (source[i] == '"') {
            ++i; ++col;
            while (i < source.size() && source[i] != '"' && source[i] != '\n') {
                if (source[i] == '\\') { ++i; ++col; }
                ++i; ++col;
            }
            if (i < source.size() && source[i] == '"') { ++i; ++col; }
            continue;
        }
        if (source[i] == '\'') {
            ++i; ++col;
            while (i < source.size() && source[i] != '\'' && source[i] != '\n') {
                if (source[i] == '\\') { ++i; ++col; }
                ++i; ++col;
            }
            if (i < source.size() && source[i] == '\'') { ++i; ++col; }
            continue;
        }

        ++col; ++i;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Main tokenize
// ═══════════════════════════════════════════════════════════════════════

std::vector<uint32_t> SemanticTokensProvider::tokenize(const std::string& source,
                                                        ParseResult* preParsed) {
    if (source.empty()) return {};

    std::vector<RawSemanticToken> raw;

    // 1. Parse with ANTLR
    ParseResult localParseStorage;
    auto& pr = preParsed ? *preParsed : (localParseStorage = Parser::parseString(source), localParseStorage);
    if (!pr.tree) return {};

    // 2. Walk parse tree to classify identifiers by context
    IdentMap identMap;
    walkTree(identMap, pr.tree);

    // 3. Walk lexer tokens and emit semantic tokens
    pr.tokens->fill();
    for (auto* tok : pr.tokens->getTokens()) {
        if (tok->getType() == antlr4::Token::EOF) break;

        uint32_t line = static_cast<uint32_t>(tok->getLine()) - 1; // ANTLR is 1-based
        uint32_t col  = static_cast<uint32_t>(tok->getCharPositionInLine());
        uint32_t len  = static_cast<uint32_t>(tok->getText().size());
        size_t type   = tok->getType();

        // Keywords
        if (isKeyword(type)) {
            emit(raw, line, col, len, SemanticTokenType::Keyword);
        }
        // Primitive types
        else if (isPrimitiveType(type)) {
            emit(raw, line, col, len, SemanticTokenType::Type,
                 static_cast<uint32_t>(SemanticTokenMod::DefaultLib));
        }
        // Literals
        else if (type == LucisLexer::INT_LIT || type == LucisLexer::FLOAT_LIT ||
                 type == LucisLexer::HEX_LIT || type == LucisLexer::OCT_LIT ||
                 type == LucisLexer::BIN_LIT ||
                 type == LucisLexer::SUFFIXED_INT ||
                 type == LucisLexer::SUFFIXED_HEX ||
                 type == LucisLexer::SUFFIXED_OCT ||
                 type == LucisLexer::SUFFIXED_BIN ||
                 type == LucisLexer::SUFFIXED_FLOAT ||
                 type == LucisLexer::SUFFIXED_DOT_FLOAT ||
                 type == LucisLexer::SUFFIXED_INT_FLOAT ||
                 type == LucisLexer::SUFFIXED_FLOAT_INT) {
            emit(raw, line, col, len, SemanticTokenType::Number);
        }
        else if (type == LucisLexer::BOOL_LIT) {
            emit(raw, line, col, len, SemanticTokenType::Keyword);
        }
        else if (type == LucisLexer::STR_LIT || type == LucisLexer::C_STR_LIT ||
                 type == LucisLexer::CHAR_LIT) {
            emitStringSubTokens(raw, line, col, tok->getText(), type);
        }
        // Operators
        else if (isOperator(type)) {
            emit(raw, line, col, len, SemanticTokenType::Operator);
        }
        // Include directives
        else if (type == LucisLexer::INCLUDE_SYS || type == LucisLexer::INCLUDE_LOCAL) {
            emit(raw, line, col, len, SemanticTokenType::Macro);
        }
        // c_macro block — emit semantic sub-tokens for content inside
        else if (type == LucisLexer::C_MACRO_BLOCK) {
            emitCMacroSubTokens(raw, line, col, tok->getText());
        }
        // asm_b block — emit full sub-token highlighting for assembly content
        else if (type == LucisLexer::ASM_B_BLOCK) {
            emitAsmBSubTokens(raw, line, col, tok->getText());
        }
        // Identifiers — look up in the context map
        else if (type == LucisLexer::IDENTIFIER) {
            std::string k = std::to_string(line) + ":" + std::to_string(col);
            auto it = identMap.find(k);
            if (it != identMap.end()) {
                emit(raw, line, col, len, it->second.type, it->second.modifiers);
            } else {
                // Unclassified identifier → default to variable
                emit(raw, line, col, len, SemanticTokenType::Variable);
            }
        }
    }

    // 4. Collect comments (skipped by ANTLR)
    collectComments(raw, source);

    // 5. Sort by position
    std::sort(raw.begin(), raw.end(), [](const RawSemanticToken& a,
                                          const RawSemanticToken& b) {
        return a.line < b.line || (a.line == b.line && a.col < b.col);
    });

    // 6. Delta-encode
    std::vector<uint32_t> encoded;
    encoded.reserve(raw.size() * 5);
    uint32_t prevLine = 0;
    uint32_t prevCol  = 0;

    for (auto& tok : raw) {
        uint32_t deltaLine = tok.line - prevLine;
        uint32_t deltaCol  = (deltaLine == 0) ? tok.col - prevCol : tok.col;
        encoded.push_back(deltaLine);
        encoded.push_back(deltaCol);
        encoded.push_back(tok.length);
        encoded.push_back(tok.type);
        encoded.push_back(tok.modifiers);
        prevLine = tok.line;
        prevCol  = tok.col;
    }

    return encoded;
}
