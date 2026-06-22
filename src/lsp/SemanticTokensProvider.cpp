#include "lsp/SemanticTokensProvider.h"
#include "parser/Parser.h"
#include "generated/LucisLexer.h"
#include "generated/LucisParser.h"

#include <algorithm>
#include <regex>

#include <unordered_set>
#include "imports/ImportResolver.h"
#include "types/BuiltinRegistry.h"

// ═══════════════════════════════════════════════════════════════════════
//  Legend
// ═══════════════════════════════════════════════════════════════════════

const std::vector<std::string>& SemanticTokensProvider::tokenTypes() {
    static const std::vector<std::string> types = {
        "namespace", "type", "struct", "enum", "enumMember",
        "function", "method", "parameter", "variable", "property",
        "keyword", "comment", "string", "number", "operator", "macro"
    };
    return types;
}

const std::vector<std::string>& SemanticTokensProvider::tokenModifiers() {
    static const std::vector<std::string> mods = {
        "declaration", "definition", "readonly", "static", "defaultLibrary"
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
case LucisLexer::INTEL:
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
        case LucisLexer::NULLCOAL: case LucisLexer::SPREAD:
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
        if (!ctx->IDENTIFIER().empty())
            classifyIdent(map, ctx->IDENTIFIER(0), SemanticTokenType::Function,
                          static_cast<uint32_t>(SemanticTokenMod::Declaration) |
                          static_cast<uint32_t>(SemanticTokenMod::Definition));
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
            emit(raw, line, col, len, SemanticTokenType::String);
        }
        // Operators
        else if (isOperator(type)) {
            emit(raw, line, col, len, SemanticTokenType::Operator);
        }
        // Include directives
        else if (type == LucisLexer::INCLUDE_SYS || type == LucisLexer::INCLUDE_LOCAL) {
            emit(raw, line, col, len, SemanticTokenType::Macro);
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
