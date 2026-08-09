// Shared file-local helpers for the IRGen translation units.
// Extracted from the monolithic IRGen.cpp so each TU compiles independently.
// All functions are inline (header-only); no TU-local state.
#pragma once

#include "IRBuilder/IRGen.h"

#include "attributes/TargetInfo.h"
#include "attributes/CfgPredicate.h"
#include "attributes/Attribute.h"

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Type.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// ── Debug info helper macro ──────────────────────────────────────────────────
#define SET_DBG_LOC(ctx)                                                          \
    do {                                                                           \
        if (dbgBuilder_ && (ctx)) {                                                \
            auto* _tok = (ctx)->getStart();                                        \
            if (_tok && currentDbgScope_)                                          \
                builder_->SetCurrentDebugLocation(                                  \
                    llvm::DILocation::get(*context_, _tok->getLine(),               \
                        _tok->getCharPositionInLine() + 1, currentDbgScope_));     \
        }                                                                          \
    } while(0)

// ── Cfg predicate parsing helpers (mirrors Checker.cpp logic) ───────────────
inline CfgPredicate irParseCfgAttrArg(LucisParser::AttrArgContext* arg) {
    CfgPredicate pred;
    if (!arg) return pred;
    if (arg->ASSIGN()) {
        pred.type = CfgPredicate::KeyValue;
        if (arg->IDENTIFIER())
            pred.name = arg->IDENTIFIER()->getText();
        if (auto* val = arg->attrArg()) {
            if (val->STR_LIT()) {
                auto raw = val->STR_LIT()->getText();
                if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
                    pred.stringValue = raw.substr(1, raw.size() - 2);
                else
                    pred.stringValue = raw;
            } else if (val->IDENTIFIER()) {
                pred.stringValue = val->IDENTIFIER()->getText();
            }
        }
        return pred;
    }
    if (arg->LPAREN()) {
        pred.type = CfgPredicate::Call;
        if (arg->IDENTIFIER())
            pred.name = arg->IDENTIFIER()->getText();
        if (auto* argList = arg->attrArgList()) {
            for (auto* child : argList->attrArg())
                pred.args.push_back(irParseCfgAttrArg(child));
        }
        return pred;
    }
    if (arg->IDENTIFIER()) {
        pred.type = CfgPredicate::Ident;
        pred.name = arg->IDENTIFIER()->getText();
        return pred;
    }
    return pred;
}

inline CfgPredicate irParseCfgFromAttribute(const Attribute& attr) {
    if (!attr.rawCtx) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }
    auto* listCtx = attr.rawCtx->getRuleContext<LucisParser::AttrArgListContext>(0);
    if (!listCtx) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }
    auto args = listCtx->attrArg();
    if (args.empty()) {
        CfgPredicate p;
        p.type = CfgPredicate::Ident;
        p.name = "true";
        return p;
    }
    if (args.size() == 1)
        return irParseCfgAttrArg(args[0]);
    CfgPredicate allPred;
    allPred.type = CfgPredicate::Call;
    allPred.name = "all";
    for (auto* a : args)
        allPred.args.push_back(irParseCfgAttrArg(a));
    return allPred;
}

inline bool irDeclActive(LucisParser::AttributeListContext* attrs,
                          const std::string& targetTriple,
                          bool debugOutput) {
    if (!attrs) return true;
    TargetInfo target(targetTriple, debugOutput);
    for (auto* a : attrs->attribute()) {
        if (!a->IDENTIFIER() || a->IDENTIFIER()->getText() != "cfg") continue;
        Attribute attr;
        attr.name = "cfg";
        attr.rawCtx = a;
        auto pred = irParseCfgFromAttribute(attr);
        if (!pred.evaluate(target))
            return false;
    }
    return true;
}

// Evaluate enum discriminant expression (already validated by Checker).
inline uint64_t evalEnumDiscExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return 0;
    // Unwrap casts and parentheses
    while (true) {
        if (auto* c = dynamic_cast<LucisParser::CastExprContext*>(expr)) {
            expr = c->expression();
        } else if (auto* p = dynamic_cast<LucisParser::ParenExprContext*>(expr)) {
            expr = p->expression();
        } else {
            break;
        }
    }
    // Plain integer literal
    if (auto* lit = dynamic_cast<LucisParser::IntLitExprContext*>(expr)) {
        auto s = lit->INT_LIT()->getText();
        s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
        return std::stoull(s);
    }
    // Suffixed integer literal
    auto stripSuffix = [](const std::string& text) -> std::optional<uint64_t> {
        static const std::vector<std::string> suffixes = {
            "i8", "i16", "i32", "i64", "i128", "iinf", "isize",
            "u8", "u16", "u32", "u64", "u128", "usize"
        };
        for (auto& suf : suffixes) {
            if (text.size() > suf.size() &&
                text.compare(text.size() - suf.size(), suf.size(), suf) == 0) {
                auto num = text.substr(0, text.size() - suf.size());
                num.erase(std::remove(num.begin(), num.end(), '_'), num.end());
                return std::stoull(num);
            }
        }
        return std::nullopt;
    };
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr)) {
        auto v = stripSuffix(si->SUFFIXED_INT()->getText());
        if (v) return *v;
    }
    if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr)) {
        auto v = stripSuffix(sh->SUFFIXED_HEX()->getText());
        if (v) return *v;
    }
    if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr)) {
        auto v = stripSuffix(so->SUFFIXED_OCT()->getText());
        if (v) return *v;
    }
    if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr)) {
        auto v = stripSuffix(sb->SUFFIXED_BIN()->getText());
        if (v) return *v;
    }
    return 0;
}

struct UnwrapCatchPatternInfo {
    const EnumVariantInfo* okVariant = nullptr;
    const EnumVariantInfo* errVariant = nullptr;
};

inline const TypeInfo* singlePayloadType(const EnumVariantInfo& variant) {
    if (variant.payloadFields.size() != 1) return nullptr;
    return variant.payloadFields[0].typeInfo;
}

// Check if type is or inherits from a type with the given name
inline bool isOrInheritsFrom(const TypeInfo* ti, const std::string& name) {
    for (auto* t = ti; t; t = t->parentType)
        if (t->name == name) return true;
    return false;
}

inline bool classifyUnwrapCatchEnum(const TypeInfo* enumType,
                                    UnwrapCatchPatternInfo& out,
                                    std::string& reason) {
    if (!enumType || enumType->kind != TypeKind::Enum) {
        reason = "unwrap-catch requires an enum expression";
        return false;
    }

    if (enumType->enumVariantInfos.size() != 2) {
        reason = "unwrap-catch requires enum '" + enumType->name +
                 "' to have exactly 2 variants (success + error)";
        return false;
    }

    for (const auto& variant : enumType->enumVariantInfos) {
        bool looksLikeErr = variant.payloadFields.empty()
            || variant.isError
            || variant.name == "Err"
            || (variant.payloadFields.size() == 1
                && variant.payloadFields[0].typeInfo
                && isOrInheritsFrom(variant.payloadFields[0].typeInfo, "Error"));

        if (looksLikeErr) {
            if (out.errVariant) {
                reason = "unwrap-catch requires exactly one error variant in enum '" +
                         enumType->name + "'";
                return false;
            }
            out.errVariant = &variant;
        } else {
            if (variant.payloadFields.size() != 1) {
                reason = "unwrap-catch requires success variant in enum '" +
                         enumType->name + "' to have exactly one payload field";
                return false;
            }
            if (out.okVariant) {
                reason = "unwrap-catch requires exactly one success variant in enum '" +
                         enumType->name + "'";
                return false;
            }
            out.okVariant = &variant;
        }
    }

    if (!out.errVariant || !out.okVariant) {
        reason = "unwrap-catch requires one error variant and one success variant in enum '" +
                 enumType->name + "'";
        return false;
    }

    return true;
}

inline bool inferArraySizesFromLLVMType(llvm::Type* ty,
                                        unsigned expectedDims,
                                        std::vector<unsigned>& outSizes) {
    outSizes.clear();
    auto* cur = ty;
    for (unsigned d = 0; d < expectedDims; d++) {
        auto* arrTy = llvm::dyn_cast<llvm::ArrayType>(cur);
        if (!arrTy) return false;
        outSizes.push_back(static_cast<unsigned>(arrTy->getNumElements()));
        cur = arrTy->getElementType();
    }
    return true;
}

inline bool isBorrowedStringExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return false;
    if (dynamic_cast<LucisParser::StrLitExprContext*>(expr)) return true;
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto name = ident->IDENTIFIER()->getText();
            return name == "fromCStr" || name == "fromCStrLen";
        }
    }
    return false;
}

inline std::string opInternalName(LucisParser::OperatorNameContext* op) {
    if (op->PLUS())    return "opAdd";
    if (op->MINUS())   return "opSub";
    if (op->STAR())    return "opMul";
    if (op->SLASH())   return "opDiv";
    if (op->PERCENT()) return "opMod";
    if (op->EQ())      return "opEq";
    if (op->NEQ())     return "opNe";
    if (op->GT().size() == 2) return "opShr";
    if (op->GT().size() == 1) return "opGt";
    if (op->LT())      return "opLt";
    if (op->LTE())     return "opLe";
    if (op->GTE())     return "opGe";
    if (op->AMPERSAND()) return "opBitAnd";
    if (op->PIPE())    return "opBitOr";
    if (op->CARET())   return "opBitXor";
    if (op->LSHIFT())  return "opShl";
    if (op->LAND())    return "opAnd";
    if (op->LOR())     return "opOr";
    if (op->NOT())     return "opNot";
    if (op->TILDE())   return "opBitNot";
    if (op->INCR())    return "opIncr";
    if (op->DECR())    return "opDecr";
    if (op->LBRACKET() && op->RBRACKET()) return "opIndex";
    if (op->LPAREN()  && op->RPAREN())    return "opCall";
    return "opUnknown";
}

// Shared helper: split suffix from literal text, return suffix and base text.
inline std::string stripUnderscores(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s)
        if (c != '_') r += c;
    return r;
}

inline std::string splitSuffix(const std::string& text, std::string& suffix) {
    auto clean = stripUnderscores(text);
    static const std::vector<std::string> suffixes = {
        "i8", "i16", "i32", "i64", "i128", "iinf", "isize",
        "u8", "u16", "u32", "u64", "u128", "usize",
        "f32", "f64", "f80", "f128"
    };
    for (auto& suf : suffixes) {
        if (clean.size() > suf.size() &&
            clean.compare(clean.size() - suf.size(), suf.size(), suf) == 0) {
            suffix = suf;
            return clean.substr(0, clean.size() - suf.size());
        }
    }
    suffix.clear();
    return clean;
}
