#include "lsp/TypeChainResolver.h"
#include "lsp/ProjectContext.h"
#include "ffi/CHeaderResolver.h"
#include "types/BuiltinRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "types/TypeRegistry.h"
#include "namespace/ModuleRegistry.h"

#include <algorithm>

namespace {

std::string safeText(antlr4::tree::TerminalNode* n) {
    return n ? n->getText() : "";
}
std::string safeText(antlr4::ParserRuleContext* ctx) {
    return ctx ? ctx->getText() : "";
}

std::string normalizeExtBaseName(const std::string& name) {
    if (name == "vec") return "Vec";
    if (name == "map") return "Map";
    if (name == "set") return "Set";
    return name;
}

// Parse "Type<A, B>" → base "Type", args {A, B}
bool parseGeneric(const std::string& t, std::string& base,
                  std::vector<std::string>& args) {
    auto open = t.find('<');
    if (open == std::string::npos) return false;
    auto close = t.rfind('>');
    if (close == std::string::npos || close <= open) return false;
    base = t.substr(0, open);
    std::string inner = t.substr(open + 1, close - open - 1);
    std::string cur;
    int depth = 0;
    for (char c : inner) {
        if (c == '<') { depth++; cur.push_back(c); }
        else if (c == '>') { depth--; cur.push_back(c); }
        else if (c == ',' && depth == 0) { args.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) args.push_back(cur);
    return true;
}

std::string substituteParams(const std::string& type,
                             const std::unordered_map<std::string, std::string>& subst) {
    std::string out;
    size_t i = 0;
    while (i < type.size()) {
        char c = type[i];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < type.size() &&
                   (std::isalnum(static_cast<unsigned char>(type[j])) || type[j] == '_'))
                j++;
            std::string id = type.substr(i, j - i);
            auto it = subst.find(id);
            out += (it != subst.end()) ? it->second : id;
            i = j;
        } else {
            out.push_back(c);
            i++;
        }
    }
    return out;
}

std::string unwrapIndexed(const std::string& type) {
    if (type == "string") return "char";
    if (type.size() > 2 && type[0] == '[') {
        auto close = type.find(']');
        if (close != std::string::npos && close + 1 < type.size())
            return type.substr(close + 1);
        if (close != std::string::npos && close + 1 == type.size())
            return type.substr(1, close - 1);  // unsized []T
    }
    if (!type.empty() && type[0] == '*') return type.substr(1);
    // vec<T> → T, set<T> → T, map<K,V> → V, Option<T> → T, Result<T,E> → T
    std::string base;
    std::vector<std::string> args;
    if (parseGeneric(type, base, args)) {
        if (!args.empty()) {
            if (base == "map" || base == "Map") return args.back();
            return args[0];
        }
    }
    return "";
}

std::string typeSpecText(LucisParser::TypeSpecContext* ts) {
    return ts ? ts->getText() : "";
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Public entry
// ═══════════════════════════════════════════════════════════════════════

static std::string resolveFieldType(const std::string& receiverType,
                                    const std::string& fieldName,
                                    const ChainResolveCtx& ctx);
static std::string resolveMethodReturn(const std::string& receiverType,
                                       const std::string& methodName,
                                       const ChainResolveCtx& ctx);

static std::string resolveChainImpl(LucisParser::ExpressionContext* expr,
                                    const ChainResolveCtx& ctx) {
    if (!expr) return "";

    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = safeText(id->IDENTIFIER());
        auto it = ctx.locals.find(name);
        if (it != ctx.locals.end()) return it->second;
        return "";
    }

    if (auto* lit = dynamic_cast<LucisParser::IntLitExprContext*>(expr)) {
        (void)lit;
        return "int";
    }
    if (dynamic_cast<LucisParser::FloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr))
        return "float";
    if (auto* sl = dynamic_cast<LucisParser::StrLitExprContext*>(expr)) {
        (void)sl;
        return "string";
    }
    if (dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return "bool";

    // (expr)
    if (auto* par = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveChainImpl(par->expression(), ctx);

    // *expr — deref
    if (auto* de = dynamic_cast<LucisParser::DerefExprContext*>(expr)) {
        auto inner = resolveChainImpl(de->expression(), ctx);
        if (!inner.empty() && inner[0] == '*') return inner.substr(1);
        return inner;
    }

    // a[i]
    if (auto* ix = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        auto base = resolveChainImpl(ix->expression(0), ctx);
        return unwrapIndexed(base);
    }

    // t.0 — tuple index
    if (auto* ti = dynamic_cast<LucisParser::TupleIndexExprContext*>(expr)) {
        auto base = resolveChainImpl(ti->expression(), ctx);
        int idx = 0;
        try {
            idx = std::stoi(ti->INT_LIT()->getText());
        } catch (...) {
            return "";
        }
        std::string b;
        std::vector<std::string> args;
        if (parseGeneric(base, b, args)) {
            if (b == "tuple" || b == "Tuple")
                if (idx >= 0 && static_cast<size_t>(idx) < args.size())
                    return args[idx];
        }
        return "";
    }

    // a.b — field access
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto base = resolveChainImpl(fa->expression(), ctx);
        return resolveFieldType(base, safeText(fa->IDENTIFIER()), ctx);
    }

    // a->b — arrow access
    if (auto* aa = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        auto base = resolveChainImpl(aa->expression(), ctx);
        if (!base.empty() && base[0] == '*') base = base.substr(1);
        return resolveFieldType(base, safeText(aa->IDENTIFIER()), ctx);
    }

    // a.b(...) — method call
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        auto base = resolveChainImpl(mc->expression(), ctx);
        return resolveMethodReturn(base, safeText(mc->IDENTIFIER()), ctx);
    }

    // a->b(...) — arrow method call
    if (auto* amc = dynamic_cast<LucisParser::ArrowMethodCallExprContext*>(expr)) {
        auto base = resolveChainImpl(amc->expression(), ctx);
        if (!base.empty() && base[0] == '*') base = base.substr(1);
        return resolveMethodReturn(base, safeText(amc->IDENTIFIER()), ctx);
    }

    // f(...) — free function call
    if (auto* fc = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        std::string name;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(fc->expression()))
            name = safeText(ident->IDENTIFIER());
        auto it = ctx.globals.find(name);
        if (it != ctx.globals.end()) return it->second;
        if (ctx.tree) {
            for (auto* tld : ctx.tree->topLevelDecl()) {
                if (auto* fn = tld->functionDecl()) {
                    if (safeText(fn->IDENTIFIER(0)) == name)
                        if (auto* rt = fn->typeSpec())
                            return typeSpecText(rt);
                }
            }
        }
        return "";
    }

    return "";
}

// Resolve a struct/union field by name (same-file, cross-file, C, builtin).
static std::string resolveFieldType(const std::string& receiverType,
                                    const std::string& fieldName,
                                    const ChainResolveCtx& ctx) {
    if (receiverType.empty() || fieldName.empty()) return "";

    std::string base = receiverType;
    std::vector<std::string> args;
    parseGeneric(receiverType, base, args);

    std::unordered_map<std::string, std::string> subst;
    auto bindTypeParams = [&](auto* decl) {
        if (!args.empty() && decl->typeParamList()) {
            auto tps = decl->typeParamList()->typeParam();
            for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
                auto ids = tps[i]->IDENTIFIER();
                if (!ids.empty())
                    subst[ids[0]->getText()] = args[i];
            }
        }
    };

    // Same-file struct/union
    if (ctx.tree) {
        for (auto* tld : ctx.tree->topLevelDecl()) {
            if (auto* sd = tld->structDecl()) {
                if (safeText(sd->IDENTIFIER()) != base) continue;
                bindTypeParams(sd);
                for (auto* field : sd->structField()) {
                    if (safeText(field->IDENTIFIER()) == fieldName)
                        return substituteParams(typeSpecText(field->typeSpec()), subst);
                }
            }
            if (auto* ud = tld->unionDecl()) {
                if (safeText(ud->IDENTIFIER()) != base) continue;
                bindTypeParams(ud);
                for (auto* field : ud->unionField()) {
                    if (safeText(field->IDENTIFIER()) == fieldName)
                        return substituteParams(typeSpecText(field->typeSpec()), subst);
                }
            }
        }
    }

    // Cross-file struct/union
    if (ctx.project && ctx.project->isValid()) {
        for (auto& ns : ctx.project->registry().allModules()) {
            auto* sym = ctx.project->registry().findSymbol(ns, base);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Struct) {
                auto* decl = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
                if (!decl) continue;
                bindTypeParams(decl);
                for (auto* field : decl->structField()) {
                    if (safeText(field->IDENTIFIER()) == fieldName)
                        return substituteParams(typeSpecText(field->typeSpec()), subst);
                }
            }
            if (sym->kind == ExportedSymbol::Union) {
                auto* decl = dynamic_cast<LucisParser::UnionDeclContext*>(sym->decl);
                if (!decl) continue;
                bindTypeParams(decl);
                for (auto* field : decl->unionField()) {
                    if (safeText(field->IDENTIFIER()) == fieldName)
                        return substituteParams(typeSpecText(field->typeSpec()), subst);
                }
            }
        }
    }

    // C struct
    if (ctx.bindings) {
        if (auto* cs = ctx.bindings->findStruct(base)) {
            for (auto& field : cs->fields) {
                if (field.name == fieldName && field.typeInfo)
                    return field.typeInfo->name;
            }
        }
    }

    return "";
}

// Resolve a method's return type by name (extend blocks, builtin methods,
// extended types like Vec/Map).
static std::string resolveMethodReturn(const std::string& receiverType,
                                       const std::string& methodName,
                                       const ChainResolveCtx& ctx) {
    if (receiverType.empty() || methodName.empty()) return "";

    std::string base = receiverType;
    std::vector<std::string> args;
    parseGeneric(receiverType, base, args);

    auto resolvePlaceholders = [&](const std::string& raw) -> std::string {
        if (raw == "_self") return receiverType;
        if (raw == "_elem") return args.empty() ? "" : args[0];
        if (raw == "_key")  return args.size() >= 2 ? args[0] : "";
        if (raw == "_val")  return args.size() >= 2 ? args[1] : (args.empty() ? "" : args[0]);
        return raw;
    };

    // Same-file extend blocks (also covers methods on the struct itself).
    if (ctx.tree) {
        for (auto* tld : ctx.tree->topLevelDecl()) {
            auto* eb = tld->extendDecl();
            if (!eb) continue;
            if (safeText(eb->IDENTIFIER()) != base) continue;
            for (auto* m : eb->extendMethod()) {
                if (safeText(m->IDENTIFIER(0)) != methodName) continue;
                if (m->typeSpec())
                    return resolvePlaceholders(typeSpecText(m->typeSpec()));
            }
        }
    }

    // Cross-file extend blocks.
    if (ctx.project && ctx.project->isValid()) {
        for (auto& ns : ctx.project->registry().allModules()) {
            auto* sym = ctx.project->registry().findSymbol(ns, base);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::ExtendBlock) {
                auto* eb = dynamic_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                if (!eb) continue;
                for (auto* m : eb->extendMethod()) {
                    if (safeText(m->IDENTIFIER(0)) == methodName)
                        if (m->typeSpec())
                            return resolvePlaceholders(typeSpecText(m->typeSpec()));
                }
            }
        }
    }

    // Extended type methods (Vec/Map/Set...)
    if (ctx.extTypeReg) {
        if (auto* extDesc = ctx.extTypeReg->lookup(normalizeExtBaseName(base))) {
            for (auto& md : extDesc->methods) {
                if (md.name != methodName) continue;
                return resolvePlaceholders(md.returnType);
            }
        }
    }

    // Built-in primitive methods (string.concat, int.toString, ...)
    if (ctx.typeReg && ctx.methodReg) {
        if (auto* typeInfo = ctx.typeReg->lookup(base)) {
            for (auto* md : ctx.methodReg->methodsFor(typeInfo->kind)) {
                if (md->name == methodName)
                    return resolvePlaceholders(md->returnType);
            }
        }
    }

    return "";
}

std::string resolveChainType(LucisParser::ExpressionContext* expr,
                             const ChainResolveCtx& ctx) {
    return resolveChainImpl(expr, ctx);
}