#include "checkers/Checker.h"
#include "generated/LucisLexer.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"
#include "parser/Parser.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

// In Checker.cpp, ensure we update any places where IntrinsicRegistry is initialized or used.
// Checker constructor:
Checker::Checker()
    : intrinsicRegistry_(typeRegistry_) {
    registerGlobalBuiltins();
    registerBuiltinAttributes(attrRegistry_);
}

static const EnumVariantInfo* findEnumVariantInfo(const TypeInfo* enumType,
                                                  const std::string& variantName) {
    if (!enumType) return nullptr;

    for (const auto& info : enumType->enumVariantInfos) {
        if (info.name == variantName) return &info;
    }

    return nullptr;
}

struct UnwrapCatchPatternInfo {
    const EnumVariantInfo* okVariant = nullptr;
    const EnumVariantInfo* errVariant = nullptr;
};

static const TypeInfo* singlePayloadType(const EnumVariantInfo& variant) {
    if (variant.payloadFields.size() != 1) return nullptr;
    return variant.payloadFields[0].typeInfo;
}

static bool classifyUnwrapCatchEnum(const TypeInfo* enumType,
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
        auto* payloadTI = singlePayloadType(variant);
        if (!payloadTI) {
            reason = "unwrap-catch requires each variant in enum '" + enumType->name +
                     "' to have exactly one payload field";
            return false;
        }

        // "error" variant: #[error] attribute or naming convention
        bool isErrName = variant.isError ||
                         variant.name == "Err" || variant.name == "Error" ||
                         variant.name == "Failure" || variant.name == "Fail" ||
                         variant.name == "None";
        if (isErrName || payloadTI->name == "Error") {
            if (out.errVariant) {
                reason = "unwrap-catch requires exactly one error variant in enum '" +
                         enumType->name + "'";
                return false;
            }
            out.errVariant = &variant;
        } else {
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

static std::string ownershipDiagnosticCode(const std::string& msg) {
    if (msg.find("already moved") != std::string::npos)
        return "OWN001";
    if (msg.find("consumes ownership") != std::string::npos)
        return "OWN002";
    if (msg.find("borrowed") != std::string::npos && msg.find("ownership") != std::string::npos)
        return "OWN003";
    if (msg.find("double") != std::string::npos && msg.find("free") != std::string::npos)
        return "OWN004";
    return "";
}

// ═══════════════════════════════════════════════════════════════════════
//  Error helpers — attach line:col and full range to every diagnostic
// ═══════════════════════════════════════════════════════════════════════

void Checker::emitDiag(antlr4::Token* start, antlr4::Token* stop,
                        Diagnostic::Severity sev, const std::string& msg) {
    if (!start) {
        errors_.push_back(msg);
        Diagnostic d;
        d.severity = sev;
        d.message  = msg;
        d.code     = ownershipDiagnosticCode(msg);
        diagnostics_.push_back(std::move(d));
        return;
    }

    // CLI string format (1-based)
    auto line = start->getLine();
    auto col  = start->getCharPositionInLine() + 1;
    std::string prefix = (sev == Diagnostic::Warning) ? "warning: " : "";
    errors_.push_back(std::to_string(line) + ":" +
                      std::to_string(col) + ": " + prefix + msg);

    // Structured diagnostic (0-based)
    Diagnostic d;
    d.severity = sev;
    d.message  = msg;
    d.code     = ownershipDiagnosticCode(msg);
    d.line     = (line > 0) ? line - 1 : 0;
    d.col      = start->getCharPositionInLine();

    if (stop && stop->getLine() > 0) {
        d.endLine = stop->getLine() - 1;
        d.endCol  = stop->getCharPositionInLine() + stop->getText().size();
    } else {
        d.endLine = d.line;
        d.endCol  = d.col + start->getText().size();
    }

    // Ensure endCol > col for single-token ranges
    if (d.endLine == d.line && d.endCol <= d.col)
        d.endCol = d.col + 1;

    diagnostics_.push_back(std::move(d));
}

void Checker::error(antlr4::ParserRuleContext* ctx, const std::string& msg) {
    if (ctx && ctx->getStart()) {
        emitDiag(ctx->getStart(), ctx->getStop(), Diagnostic::Error, msg);
    } else {
        emitDiag(nullptr, nullptr, Diagnostic::Error, msg);
    }
}

void Checker::warning(antlr4::ParserRuleContext* ctx, const std::string& msg) {
    if (ctx && ctx->getStart()) {
        emitDiag(ctx->getStart(), ctx->getStop(), Diagnostic::Warning, msg);
    } else {
        emitDiag(nullptr, nullptr, Diagnostic::Warning, msg);
    }
}

void Checker::errorToken(antlr4::Token* start, antlr4::Token* stop,
                          const std::string& msg) {
    emitDiag(start, stop, Diagnostic::Error, msg);
}

void Checker::warningToken(antlr4::Token* start, antlr4::Token* stop,
                            const std::string& msg) {
    emitDiag(start, stop, Diagnostic::Warning, msg);
}

std::optional<uint64_t> Checker::tryEvalUSizeExpr(LucisParser::ExpressionContext* expr) const {
    auto range = tryEvalUSizeRangeExpr(expr);
    if (!range) return std::nullopt;
    if (range->first != range->second) return std::nullopt;
    return range->first;
}

std::optional<std::pair<uint64_t, uint64_t>>
Checker::tryEvalUSizeRangeExpr(LucisParser::ExpressionContext* expr) const {
    if (!expr) return std::nullopt;

    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr))
        return tryEvalUSizeRangeExpr(cast->expression());

    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return tryEvalUSizeRangeExpr(paren->expression());

    if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(expr)) {
        try {
            auto s = intLit->INT_LIT()->getText();
            s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
            if (s.empty() || s[0] == '-') return std::nullopt;
            auto v = static_cast<uint64_t>(std::stoull(s));
            return std::make_pair(v, v);
        } catch (...) {
            return std::nullopt;
        }
    }

    // Suffixed integer literals: strip suffix, parse numeric part
    auto tryEvalSuffixed = [](const std::string& text)
        -> std::optional<std::pair<uint64_t, uint64_t>> {
        static const std::vector<std::string> suffixes = {
            "i8", "i16", "i32", "i64", "i128", "iinf", "isize",
            "u8", "u16", "u32", "u64", "u128", "usize"
        };
        for (auto& suf : suffixes) {
            if (text.size() > suf.size() &&
                text.compare(text.size() - suf.size(), suf.size(), suf) == 0) {
                try {
                    auto num = text.substr(0, text.size() - suf.size());
                    num.erase(std::remove(num.begin(), num.end(), '_'), num.end());
                    if (num.empty() || num[0] == '-') return std::nullopt;
                    auto v = static_cast<uint64_t>(std::stoull(num));
                    return std::make_pair(v, v);
                } catch (...) {
                    return std::nullopt;
                }
            }
        }
        return std::nullopt;
    };
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr))
        return tryEvalSuffixed(si->SUFFIXED_INT()->getText());
    if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr))
        return tryEvalSuffixed(sh->SUFFIXED_HEX()->getText());
    if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr))
        return tryEvalSuffixed(so->SUFFIXED_OCT()->getText());
    if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr))
        return tryEvalSuffixed(sb->SUFFIXED_BIN()->getText());

    // Float-with-integer-suffix literals (e.g., 4.2u64) are not range-evaluable
    if (dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr))
        return std::nullopt;

    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end() && it->second.hasKnownUSizeRange)
            return std::make_pair(it->second.minUSize, it->second.maxUSize);
        return std::nullopt;
    }

    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto exprs = add->expression();
        if (exprs.size() != 2) return std::nullopt;

        auto lhs = tryEvalUSizeRangeExpr(exprs[0]);
        auto rhs = tryEvalUSizeRangeExpr(exprs[1]);
        if (!lhs || !rhs) return std::nullopt;

        auto opText = add->op->getText();
        if (opText == "+") {
            if (lhs->first > std::numeric_limits<uint64_t>::max() - rhs->first) return std::nullopt;
            if (lhs->second > std::numeric_limits<uint64_t>::max() - rhs->second) return std::nullopt;
            return std::make_pair(lhs->first + rhs->first, lhs->second + rhs->second);
        }

        if (opText == "-") {
            if (lhs->first < rhs->second) return std::nullopt;
            return std::make_pair(lhs->first - rhs->second, lhs->second - rhs->first);
        }

        return std::nullopt;
    }

    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto exprs = mul->expression();
        if (exprs.size() != 2) return std::nullopt;
        if (mul->op->getText() != "*") return std::nullopt;

        auto lhs = tryEvalUSizeRangeExpr(exprs[0]);
        auto rhs = tryEvalUSizeRangeExpr(exprs[1]);
        if (!lhs || !rhs) return std::nullopt;

        if (lhs->first > 0 && rhs->first > std::numeric_limits<uint64_t>::max() / lhs->first)
            return std::nullopt;
        if (lhs->second > 0 && rhs->second > std::numeric_limits<uint64_t>::max() / lhs->second)
            return std::nullopt;

        return std::make_pair(lhs->first * rhs->first, lhs->second * rhs->second);
    }

    return std::nullopt;
}

std::optional<uint64_t> Checker::tryGetCStringLiteralLen(LucisParser::ExpressionContext* expr) const {
    if (!expr) return std::nullopt;

    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr))
        return tryGetCStringLiteralLen(cast->expression());

    auto decodeLen = [](const std::string& tok, const std::string& prefix) -> std::optional<uint64_t> {
        if (tok.size() < prefix.size() + 2) return std::nullopt;
        if (tok.rfind(prefix, 0) != 0) return std::nullopt;
        if (tok.back() != '"') return std::nullopt;

        uint64_t len = 0;
        for (size_t i = prefix.size() + 1; i + 1 < tok.size();) {
            if (tok[i] == '\\') {
                if (i + 1 >= tok.size() - 1) return std::nullopt;
                char next = tok[i + 1];
                switch (next) {
                    case 'n': case 'r': case 't': case '\\':
                    case '"': case '\'': case 'a': case 'b':
                    case 'f': case 'v': case 'e': case 'E':
                    case '?':
                        len += 1;
                        i += 2;
                        break;
                    case 'x':
                        if (i + 3 >= tok.size()) return std::nullopt;
                        len += 1;
                        i += 4;
                        break;
                    case 'u':
                        if (i + 5 >= tok.size()) return std::nullopt;
                        {
                            auto hex = tok.substr(i + 2, 4);
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            if (cp <= 0x7F) len += 1;
                            else if (cp <= 0x7FF) len += 2;
                            else if (cp <= 0xFFFF) len += 3;
                            else len += 4;
                        }
                        i += 6;
                        break;
                    case 'U':
                        if (i + 9 >= tok.size()) return std::nullopt;
                        {
                            auto hex = tok.substr(i + 2, 8);
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            if (cp <= 0x7F) len += 1;
                            else if (cp <= 0x7FF) len += 2;
                            else if (cp <= 0xFFFF) len += 3;
                            else len += 4;
                        }
                        i += 10;
                        break;
                    default:
                        if (next >= '0' && next <= '7') {
                            len += 1;
                            i += 2;
                            int octCnt = 1;
                            while (octCnt < 3 && i + 1 < tok.size() && tok[i] >= '0' && tok[i] <= '7') {
                                ++i;
                                ++octCnt;
                            }
                        } else {
                            len += 1;
                            i += 2;
                        }
                        break;
                }
            } else {
                len += 1;
                i += 1;
            }
        }
        return len;
    };

    if (auto* cstr = dynamic_cast<LucisParser::CStrLitExprContext*>(expr))
        return decodeLen(cstr->C_STR_LIT()->getText(), "c");

    if (auto* str = dynamic_cast<LucisParser::StrLitExprContext*>(expr))
        return decodeLen(str->STR_LIT()->getText(), "");

    // Backtick strings — raw content (no escape processing)
    // Extract content between backticks, handling prefix characters.
    auto backtickContentLen = [](const std::string& tok) -> std::optional<size_t> {
        // tok includes the delimiters; find first and last backtick
        auto first = tok.find('`');
        if (first == std::string::npos) return std::nullopt;
        auto last = tok.rfind('`');
        if (last == std::string::npos || last <= first) return std::nullopt;
        // Content is between first and last backtick
        auto content = tok.substr(first + 1, last - first - 1);
        // Replace escaped backticks (`` → `) for byte-length
        size_t len = 0;
        for (size_t i = 0; i < content.size(); i++) {
            if (content[i] == '`' && i + 1 < content.size() && content[i + 1] == '`') {
                len += 1;
                i += 1;
            } else {
                len += 1;
            }
        }
        return len;
    };

    if (auto* bt = dynamic_cast<LucisParser::BtickExprContext*>(expr))
        return backtickContentLen(bt->BTICK()->getText());
    if (auto* bt = dynamic_cast<LucisParser::RawBtickExprContext*>(expr))
        return backtickContentLen(bt->RAW_BTICK()->getText());
    if (auto* bt = dynamic_cast<LucisParser::IntBtickExprContext*>(expr))
        return backtickContentLen(bt->INT_BTICK()->getText());
    if (auto* bt = dynamic_cast<LucisParser::ShellBtickExprContext*>(expr))
        return backtickContentLen(bt->SHELL_BTICK()->getText());
    if (auto* bt = dynamic_cast<LucisParser::CmptBtickExprContext*>(expr))
        return backtickContentLen(bt->CMPT_BTICK()->getText());

    return std::nullopt;
}

Checker::VarInfo* Checker::resolveTrackedVarFromExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return nullptr;

    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr))
        return resolveTrackedVarFromExpr(cast->expression());

    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end()) return &it->second;
    }

    return nullptr;
}

void Checker::resetTrackedBufferInfo(VarInfo& vi) {
    vi.hasBufferCapacity = false;
    vi.bufferCapacity = 0;
    vi.hasKnownCStringLen = false;
    vi.cstringLen = 0;
    vi.pointerEscaped = false;
}

void Checker::resetTrackedNumericInfo(VarInfo& vi) {
    vi.hasKnownUSizeRange = false;
    vi.minUSize = 0;
    vi.maxUSize = 0;
}

void Checker::trackVarBufferFromExpr(const std::string& varName,
                                     LucisParser::ExpressionContext* expr,
                                     const TypeInfo* declaredType) {
    auto it = locals_.find(varName);
    if (it == locals_.end()) return;

    auto* vi = &it->second;
    resetTrackedBufferInfo(*vi);

    if (!declaredType || declaredType->kind != TypeKind::Pointer) return;

    if (auto* src = resolveTrackedVarFromExpr(expr)) {
        if (src != vi && src->type && src->type->kind == TypeKind::Pointer) {
            vi->hasBufferCapacity = src->hasBufferCapacity;
            vi->bufferCapacity = src->bufferCapacity;
            vi->hasKnownCStringLen = src->hasKnownCStringLen;
            vi->cstringLen = src->cstringLen;
            vi->pointerEscaped = src->pointerEscaped;
            return;
        }
    }

    if (auto cap = tryGetCStringLiteralLen(expr)) {
        vi->hasBufferCapacity = true;
        vi->bufferCapacity = *cap + 1;
        vi->hasKnownCStringLen = true;
        vi->cstringLen = *cap;
        return;
    }

    LucisParser::ExpressionContext* probe = expr;
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(probe))
        probe = cast->expression();

    auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(probe);
    if (!call) return;

    std::string calleeName;
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression()))
        calleeName = ident->IDENTIFIER()->getText();
    if (calleeName.empty()) return;

    std::vector<LucisParser::ExpressionContext*> args;
    if (auto* argList = call->argList())
        args = argList->expression();

    if (calleeName == "malloc" && args.size() >= 1) {
        if (auto n = tryEvalUSizeExpr(args[0])) {
            vi->hasBufferCapacity = true;
            vi->bufferCapacity = *n;
        }
        return;
    }

    if (calleeName == "calloc" && args.size() >= 2) {
        auto count = tryEvalUSizeExpr(args[0]);
        auto size  = tryEvalUSizeExpr(args[1]);
        if (count && size && *count <= std::numeric_limits<uint64_t>::max() / *size) {
            vi->hasBufferCapacity = true;
            vi->bufferCapacity = (*count) * (*size);
        }
        return;
    }

    if (calleeName == "realloc" && args.size() >= 2) {
        if (auto n = tryEvalUSizeExpr(args[1])) {
            vi->hasBufferCapacity = true;
            vi->bufferCapacity = *n;
            vi->hasKnownCStringLen = false;
        }
        return;
    }
}

void Checker::trackVarNumericRangeFromExpr(const std::string& varName,
                                           LucisParser::ExpressionContext* expr,
                                           const TypeInfo* declaredType) {
    auto it = locals_.find(varName);
    if (it == locals_.end()) return;

    auto* vi = &it->second;
    resetTrackedNumericInfo(*vi);

    if (!declaredType || declaredType->kind != TypeKind::Integer) return;

    auto range = tryEvalUSizeRangeExpr(expr);
    if (!range) return;

    vi->hasKnownUSizeRange = true;
    vi->minUSize = range->first;
    vi->maxUSize = range->second;
}

bool Checker::isDropTrackedType(const TypeInfo* type, unsigned arrayDims) const {
    if (!type || arrayDims > 0) return false;
    if (type->kind == TypeKind::String) return true;
    if (type->kind == TypeKind::Extended) return true;
    if (type->dropTracked) return true;
    return false;
}

static bool isBacktickExpr(LucisParser::ExpressionContext* expr) {
    return dynamic_cast<LucisParser::BtickExprContext*>(expr) ||
           dynamic_cast<LucisParser::RawBtickExprContext*>(expr) ||
           dynamic_cast<LucisParser::IntBtickExprContext*>(expr) ||
           dynamic_cast<LucisParser::ShellBtickExprContext*>(expr) ||
           dynamic_cast<LucisParser::CmptBtickExprContext*>(expr);
}

bool Checker::isBorrowedStringExpr(LucisParser::ExpressionContext* expr) const {
    if (dynamic_cast<LucisParser::StrLitExprContext*>(expr)) return true;
    if (isBacktickExpr(expr)) return true;
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto name = ident->IDENTIFIER()->getText();
            return name == "fromCStr" || name == "fromCStrLen";
        }
    }
    return false;
}

bool Checker::exprConsumesOwnership(LucisParser::ExpressionContext* expr) const {
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(ident->IDENTIFIER()->getText());
        if (it != locals_.end())
            return isDropTrackedType(it->second.type, it->second.arrayDims);
    }
    return false;
}

void Checker::updateOwnershipOnInitialization(VarInfo& vi, LucisParser::ExpressionContext* expr) {
    if (!isDropTrackedType(vi.type, vi.arrayDims)) {
        vi.ownership = VarInfo::OwnershipState::BorrowedImm;
        return;
    }

    if (expr && exprConsumesOwnership(expr)) {
        vi.ownership = VarInfo::OwnershipState::Owned;
        return;
    }

    if (vi.type && vi.type->kind == TypeKind::String && isBorrowedStringExpr(expr)) {
        vi.ownership = VarInfo::OwnershipState::BorrowedImm;
        return;
    }

    vi.ownership = VarInfo::OwnershipState::Owned;
}

void Checker::markExprAsMoved(LucisParser::ExpressionContext* expr, antlr4::ParserRuleContext* whereCtx) {
    auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr);
    if (!ident) return;
    auto name = ident->IDENTIFIER()->getText();
    auto it = locals_.find(name);
    if (it == locals_.end()) return;
    auto& vi = it->second;
    if (!isDropTrackedType(vi.type, vi.arrayDims)) return;

    if (vi.ownership == VarInfo::OwnershipState::Moved ||
        vi.ownership == VarInfo::OwnershipState::Dropped) {
        error(whereCtx, "double-move detected: value '" + name + "' was already moved");
        return;
    }

    vi.ownership = VarInfo::OwnershipState::Moved;
}

void Checker::applyCallOwnershipEffects(
    const std::string& calleeName,
    const std::vector<LucisParser::ExpressionContext*>& args,
    antlr4::ParserRuleContext* whereCtx) {
    auto* sig = calleeName.empty() ? nullptr : builtinRegistry_.lookup(calleeName);
    if (!sig) return;
    for (size_t i = 0; i < args.size(); ++i) {
        if (builtinRegistry_.argBorrows(calleeName, i)) continue;
        if (builtinRegistry_.argConsumes(calleeName, i))
            markExprAsMoved(args[i], whereCtx);
    }
}

void Checker::validateExprNotMoved(LucisParser::ExpressionContext* expr, antlr4::ParserRuleContext* whereCtx) {
    auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr);
    if (!ident) return;
    auto name = ident->IDENTIFIER()->getText();
    auto it = locals_.find(name);
    if (it == locals_.end()) return;
    auto& vi = it->second;
    if (!isDropTrackedType(vi.type, vi.arrayDims)) return;

    if (vi.ownership == VarInfo::OwnershipState::Moved ||
        vi.ownership == VarInfo::OwnershipState::Dropped) {
        error(whereCtx, "use-after-move: value '" + name + "' was moved and is no longer valid");
    }
}

void Checker::analyzeUnsafeCBufferCall(const std::string& funcName,
                                       antlr4::ParserRuleContext* ctx,
                                       const std::vector<LucisParser::ExpressionContext*>& args) {
    if (!cBindings_) return;
    const CFunction* cfunc = cBindings_->findFunction(funcName);
    if (!cfunc) return;
    if (args.empty()) return;

    auto warn = [&](const std::string& details) {
        warning(ctx, "possible buffer overflow in C call '" + funcName + "': " + details);
    };

    auto lower = [](std::string s) {
        for (char& ch : s)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };
    auto containsAny = [&](const std::string& s,
                           const std::vector<std::string>& needles) {
        for (const auto& n : needles)
            if (!n.empty() && s.find(n) != std::string::npos)
                return true;
        return false;
    };

    const std::vector<std::string> destHints = {
        "dst", "dest", "out", "buf", "buffer"
    };
    const std::vector<std::string> sizeHints = {
        "n", "len", "size", "count", "cap", "bytes"
    };
    const std::vector<std::string> srcHints = {
        "src", "source", "from", "input", "in"
    };

    std::vector<size_t> pointerIdx;
    std::vector<size_t> integerIdx;
    for (size_t i = 0; i < cfunc->paramTypes.size(); i++) {
        if (i >= args.size()) break;
        auto* ti = cfunc->paramTypes[i];
        if (!ti) continue;
        if (ti->kind == TypeKind::Pointer)
            pointerIdx.push_back(i);
        else if (ti->kind == TypeKind::Integer)
            integerIdx.push_back(i);
    }

    if (pointerIdx.empty()) return;
    // Robustness: reject overly complex signatures
    if (cfunc->paramTypes.size() > 6) return;
    if (integerIdx.size() > 1) return;  // Ambiguity: multiple size candidates

    for (size_t idx : pointerIdx) {
        if (idx >= args.size()) continue;
        if (auto* p = resolveTrackedVarFromExpr(args[idx])) {
            p->pointerEscaped = true;
            p->hasKnownCStringLen = false;
        }
    }

    auto pickByHint = [&](const std::vector<size_t>& candidates,
                          const std::vector<std::string>& hints,
                          size_t preferAfter,
                          bool requireAfter) -> std::optional<size_t> {
        for (size_t idx : candidates) {
            if (requireAfter && idx <= preferAfter) continue;
            if (idx >= cfunc->paramNames.size()) continue;
            auto pname = lower(cfunc->paramNames[idx]);
            if (containsAny(pname, hints))
                return idx;
        }
        for (size_t idx : candidates) {
            if (requireAfter && idx <= preferAfter) continue;
            return idx;
        }
        return std::nullopt;
    };

    auto destIdx = pickByHint(pointerIdx, destHints, 0, false);
    if (!destIdx || *destIdx > 2) return;  // Dest must be in first 3 params

    auto* dest = resolveTrackedVarFromExpr(args[*destIdx]);
    if (!dest || !dest->hasBufferCapacity) return;

    auto sizeIdx = pickByHint(integerIdx, sizeHints, *destIdx, true);
    if (sizeIdx && integerIdx.size() == 1) {
        // Robustness: size must come AFTER dest and be reasonably close
        if (*sizeIdx <= *destIdx || (*sizeIdx - *destIdx) > 2) return;
        auto nRange = tryEvalUSizeRangeExpr(args[*sizeIdx]);
        if (nRange && nRange->first > dest->bufferCapacity) {
            std::string pname = (*sizeIdx < cfunc->paramNames.size() &&
                                 !cfunc->paramNames[*sizeIdx].empty())
                                ? cfunc->paramNames[*sizeIdx]
                                : ("arg" + std::to_string(*sizeIdx + 1));
            warn("inferred write bound '" + pname + "' = " +
                 std::to_string(nRange->first) +
                 " exceeds destination capacity " +
                 std::to_string(dest->bufferCapacity));
        } else if (nRange && nRange->second > dest->bufferCapacity) {
            std::string pname = (*sizeIdx < cfunc->paramNames.size() &&
                                 !cfunc->paramNames[*sizeIdx].empty())
                                ? cfunc->paramNames[*sizeIdx]
                                : ("arg" + std::to_string(*sizeIdx + 1));
            warn("inferred write bound range for '" + pname + "' is [" +
                 std::to_string(nRange->first) + ", " +
                 std::to_string(nRange->second) +
                 "], which may exceed destination capacity " +
                 std::to_string(dest->bufferCapacity));
        }
        return;
    }

    auto srcIdx = pickByHint(pointerIdx, srcHints, *destIdx, true);
    if (!srcIdx) return;

    auto srcLen = tryGetCStringLiteralLen(args[*srcIdx]);
    if (!srcLen) return;

    uint64_t required = *srcLen + 1;
    if (required > dest->bufferCapacity) {
        std::string pname = (*srcIdx < cfunc->paramNames.size() &&
                             !cfunc->paramNames[*srcIdx].empty())
                            ? cfunc->paramNames[*srcIdx]
                            : ("arg" + std::to_string(*srcIdx + 1));
        warn("inferred minimum required size from source '" + pname +
             "' is " + std::to_string(required) +
             " bytes, destination capacity is " +
             std::to_string(dest->bufferCapacity));
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Main entry point
// ═══════════════════════════════════════════════════════════════════════

bool Checker::check(LucisParser::ProgramContext* tree) {
    if (!tree) {
        errors_.push_back("empty program");
        return false;
    }

    // Pass 0: register global builtins (always available, no import needed)
    registerGlobalBuiltins();

    // Phase 1: mirror builtins into SemanticDB
    initSemanticDB();

    // Pass 0.5: register C header bindings (from #include directives)
    // Determine which C functions are overridden by Lucis imports
    // based on preamble declaration order (last import wins).
    std::unordered_set<std::string> lucisOverridesC;
    if (cBindings_) {
        int lastIncludePos = -1;
        std::unordered_map<std::string, int> lucisImportPos;
        int pos = 0;
        for (auto* pre : tree->preambleDecl()) {
            if (pre->includeDecl()) {
                lastIncludePos = pos;
            } else if (auto* ud = pre->useDecl()) {
                if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(ud)) {
                    if (item->IDENTIFIER()) {
                        auto n = item->IDENTIFIER()->getText();
                        if (cBindings_->findFunction(n))
                            lucisImportPos[n] = pos;
                    }
                } else if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(ud)) {
                    for (auto* id : group->IDENTIFIER()) {
                        auto n = id->getText();
                        if (cBindings_->findFunction(n))
                            lucisImportPos[n] = pos;
                    }
                }
            }
            pos++;
        }
        for (auto& [n, lucisPos] : lucisImportPos) {
            if (lucisPos > lastIncludePos)
                lucisOverridesC.insert(n);
        }
    }

    // Only register C bindings from #include directives when this file
    // actually includes C headers — prevents symbols from one file's
    // #include leaking into another file's scope.
    bool hasCInclude = false;
    for (auto* pre : tree->preambleDecl()) {
        if (pre->includeDecl()) { hasCInclude = true; break; }
    }

    if (cBindings_) {
        // Register C structs as types
        for (auto& [name, cstruct] : cBindings_->structs()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti;
                ti.name = name;
                ti.kind = TypeKind::Struct;
                ti.bitWidth = 0;
                ti.isSigned = false;
                ti.fields = cstruct.fields;
                typeRegistry_.registerType(std::move(ti));
            }
        }

        // Register C enums as types + enum constant globals
        for (auto& [name, cenum] : cBindings_->enums()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti;
                ti.name = name;
                ti.kind = TypeKind::Enum;
                ti.bitWidth = 32;
                ti.isSigned = false;
                ti.builtinSuffix = "i32";
                for (auto& [vname, val] : cenum.values)
                    ti.enumVariants.push_back(vname);
                typeRegistry_.registerType(std::move(ti));
            }
            // Register each enum constant as a known variable
            for (auto& [vname, val] : cenum.values) {
                auto* enumTI = typeRegistry_.lookup(name);
                if (enumTI)
                    cEnumConstants_[vname] = { enumTI, val };
            }
        }

        // Register C typedefs as type aliases
        for (auto& [name, ctdef] : cBindings_->typedefs()) {
            if (!typeRegistry_.lookup(name)) {
                TypeInfo ti = *ctdef.underlying;
                ti.name = name;
                typeRegistry_.registerType(std::move(ti));
            }
        }

        // Register C functions (skip those overridden by Lucis imports)
        for (auto& [name, cfunc] : cBindings_->functions()) {
            if (lucisOverridesC.count(name)) continue;
            auto* funcType = makeFunctionType(
                cfunc.returnType, cfunc.paramTypes, cfunc.isVariadic);
            functions_[name] = funcType;
            globalBuiltins_.insert(name);
        }

        // Register C #define constants (integer, float, string)
        for (auto& [name, cmacro] : cBindings_->macros()) {
            if (cmacro.isFloat) {
                auto* f64TI = typeRegistry_.lookup("float64");
                if (f64TI)
                    cEnumConstants_[name] = { f64TI, 0, cmacro.floatValue, true, false };
            } else if (cmacro.isString) {
                auto* charTI = typeRegistry_.lookup("char");
                if (charTI) {
                    // String macros are *char (pointer to char)
                    auto ptrName = "*" + charTI->name;
                    auto* ptrTI = typeRegistry_.lookup(ptrName);
                    if (!ptrTI) {
                        TypeInfo ti;
                        ti.name = ptrName;
                        ti.kind = TypeKind::Pointer;
                        ti.pointeeType = charTI;
                        ti.bitWidth = 0;
                        ti.isSigned = false;
                        ti.builtinSuffix = "cstr";  // *char uses cstr suffix for std::log
                        typeRegistry_.registerType(std::move(ti));
                        ptrTI = typeRegistry_.lookup(ptrName);
                    }
                    if (ptrTI)
                        cEnumConstants_[name] = { ptrTI, 0, 0.0, false, true };
                }
            } else {
                auto* int32TI = typeRegistry_.lookup("int32");
                if (int32TI)
                    cEnumConstants_[name] = { int32TI, cmacro.value };
            }
        }

        // Register C #define struct literal constants (e.g. RAYWHITE)
        for (auto& [name, sm] : cBindings_->structMacros()) {
            auto* structTI = typeRegistry_.lookup(sm.structType);
            if (structTI)
                cEnumConstants_[name] = { structTI, 0 };
        }

        // Register C global variables
        for (auto& [name, gvar] : cBindings_->globals()) {
            cGlobals_[name] = gvar.type;
        }

        // Register C function-like macros
        for (auto& [name, flm] : cBindings_->functionLikeMacros()) {
            cFunctionLikeMacros_[name] = &flm;
        }
    }

    // Pass 1: register all `use` declarations
    checkUseDecls(tree);

    // Pass 1.5: register cross-file structs, enums, and functions
    // from same module and user imports, so type resolution works.
    // Process in dependency order: struct/union skeletons first, then enums/typeAliases.
    if (moduleRegistry_) {
        // First pass: register struct/union skeletons so enum payloads can reference them
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                auto name = sd->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericStructTemplates_.count(name)) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Struct;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                }
            } else if (auto* ud = decl->unionDecl()) {
                auto name = ud->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericUnionTemplates_.count(name)) {
                    TypeInfo skeleton;
                    skeleton.name = name;
                    skeleton.kind = TypeKind::Union;
                    skeleton.bitWidth = 0;
                    skeleton.isSigned = false;
                    typeRegistry_.registerType(std::move(skeleton));
                }
            }
        }
        // Pre-register imported enums so type aliases referencing them resolve
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || sym->kind != ExportedSymbol::Enum) continue;
            if (!typeRegistry_.lookup(sym->name) && !genericEnumTemplates_.count(sym->name)) {
                auto* ed = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(ed);
            }
        }

        // Second pass: enums and type aliases (struct/union skeletons now available)
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* ed = decl->enumDecl()) {
                auto name = ed->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name) && !genericEnumTemplates_.count(name))
                    checkEnumDecl(ed);
            } else if (auto* ta = decl->typeAliasDecl()) {
                auto name = ta->IDENTIFIER()->getText();
                if (!typeRegistry_.lookup(name))
                    checkTypeAliasDecl(ta);
            }
        }

        // Same-module external symbols
        auto extSyms = moduleRegistry_->getExternalSymbols(
            currentModulePath_, currentFile_);

        // Pre-register skeletons for external structs and unions so recursive
        // dependencies do not trigger duplicate full type registration.
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Struct &&
                !typeRegistry_.lookup(sym->name) &&
                !genericStructTemplates_.count(sym->name)) {
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Struct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
                // Phase 1: forward-declare in SemanticDB
                if (semanticDB_)
                    semanticDB_->forwardDeclare(sym->name, semantic::DeclKind::Struct,
                                                sym->modulePath,
                                                toSemanticLoc(sym->decl));
            } else if (sym->kind == ExportedSymbol::Union &&
                       !typeRegistry_.lookup(sym->name) &&
                       !genericUnionTemplates_.count(sym->name)) {
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Union;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
                // Phase 1: forward-declare in SemanticDB
                if (semanticDB_)
                    semanticDB_->forwardDeclare(sym->name, semantic::DeclKind::Union,
                                                sym->modulePath,
                                                toSemanticLoc(sym->decl));
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Struct &&
                !typeRegistry_.lookup(sym->name) &&
                !genericStructTemplates_.count(sym->name)) {
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Struct;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            } else if (sym->kind == ExportedSymbol::Union &&
                       !typeRegistry_.lookup(sym->name) &&
                       !genericUnionTemplates_.count(sym->name)) {
                TypeInfo skeleton;
                skeleton.name = sym->name;
                skeleton.kind = TypeKind::Union;
                skeleton.bitWidth = 0;
                skeleton.isSigned = false;
                typeRegistry_.registerType(std::move(skeleton));
            }
        }

        // The topological sort below handles full registration.
        // Skeletons are registered by checkStructDecl internally.

        // Imported function signatures may reference generic types (e.g. Result<T>)
        // that were not explicitly imported via `use`. Ensure those type templates
        // are registered before function signature/type resolution.
        std::unordered_set<std::string> seenDeps;  // prevent cycles
        auto ensureTypeDependencyFromSpec =
            [&](auto&& self, LucisParser::TypeSpecContext* ts, const std::string& ns) -> void {
                if (!ts) return;
                if (ts->fnTypeSpec()) {
                    for (auto* inner : ts->fnTypeSpec()->typeSpec())
                        self(self, inner, ns);
                    return;
                }
                for (auto* inner : ts->typeSpec())
                    self(self, inner, ns);

                if (!ts->IDENTIFIER()) return;
                auto baseName = ts->IDENTIFIER()->getText();

                auto* depSym = moduleRegistry_->findSymbol(ns, baseName);
                if (!depSym) {
                    for (auto& candidateNs : moduleRegistry_->allModules()) {
                        depSym = moduleRegistry_->findSymbol(candidateNs, baseName);
                        if (depSym) break;
                    }
                }
                if (!depSym) return;
                if (depSym->sourceFile == currentFile_) return;

                // Check if the type needs processing (not registered, or only a skeleton)
                auto* existingTI = typeRegistry_.lookup(baseName);
                bool needsProc = !existingTI;
                if (existingTI && existingTI->kind == TypeKind::Struct &&
                    existingTI->fields.empty() && existingTI->bitWidth == 0)
                    needsProc = true;
                if (existingTI && existingTI->kind == TypeKind::Union &&
                    existingTI->fields.empty() && existingTI->bitWidth == 0)
                    needsProc = true;
                if (existingTI && existingTI->kind == TypeKind::Enum &&
                    existingTI->enumVariantInfos.empty())
                    needsProc = true;

                if (depSym->kind == ExportedSymbol::Enum &&
                    needsProc &&
                    !genericEnumTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ed = static_cast<LucisParser::EnumDeclContext*>(depSym->decl);
                    // Pre-resolve payload types before checking the enum
                    for (auto* variant : ed->enumVariant())
                        for (auto* ts : variant->typeSpec())
                            self(self, ts, ns);
                    checkEnumDecl(ed);
                    return;
                }
                if (depSym->kind == ExportedSymbol::Struct &&
                    needsProc &&
                    !genericStructTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* sd = static_cast<LucisParser::StructDeclContext*>(depSym->decl);
                    // Pre-resolve field types before checking the struct
                    for (auto* field : sd->structField())
                        self(self, field->typeSpec(), ns);
            // Pre-resolve parent struct if inheriting
            if (sd->COLON() && sd->typeSpec()) {
                unsigned parentDims = 0;
                auto* parentTI = resolveTypeSpec(sd->typeSpec(), parentDims);
                if (parentTI && parentTI->kind == TypeKind::Struct &&
                    !genericStructTemplates_.count(parentTI->name) &&
                    !seenDeps.count(parentTI->name)) {
                    auto* parentSym = moduleRegistry_->findSymbol(ns, parentTI->name);
                    if (!parentSym) {
                        for (auto& candidateNs : moduleRegistry_->allModules()) {
                            parentSym = moduleRegistry_->findSymbol(candidateNs, parentTI->name);
                            if (parentSym) break;
                        }
                    }
                    if (parentSym && parentSym->kind == ExportedSymbol::Struct) {
                        auto* parentDecl = static_cast<LucisParser::StructDeclContext*>(parentSym->decl);
                        for (auto* pf : parentDecl->structField())
                            self(self, pf->typeSpec(), parentSym->modulePath);
                        checkStructDecl(parentDecl);
                    }
                }
            }
                    checkStructDecl(sd);
                    return;
                }
                if (depSym->kind == ExportedSymbol::Union &&
                    needsProc &&
                    !genericUnionTemplates_.count(baseName)) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ud = static_cast<LucisParser::UnionDeclContext*>(depSym->decl);
                    // Pre-resolve field types before checking the union
                    for (auto* field : ud->unionField())
                        self(self, field->typeSpec(), ns);
                    checkUnionDecl(ud);
                    return;
                }
                if (depSym->kind == ExportedSymbol::TypeAlias && needsProc) {
                    if (!seenDeps.insert(baseName).second) return;
                    auto* ta = static_cast<LucisParser::TypeAliasDeclContext*>(depSym->decl);
                    // Pre-resolve the underlying type before checking the alias
                    self(self, ta->typeSpec(), ns);
                    checkTypeAliasDecl(ta);
                    return;
                }
            };

        auto ensureFunctionTypeDependencies =
            [&](LucisParser::FunctionDeclContext* decl, const std::string& ns) {
                if (!decl) return;
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec, decl->typeSpec(), ns);
                if (auto* params = decl->paramList()) {
                    for (auto* p : params->param())
                        ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                }
            };

        // Phase A: enums and type aliases (these have no dependencies)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Enum) {
                if (typeRegistry_.lookup(sym->name) || genericEnumTemplates_.count(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(decl);
            } else if (sym->kind == ExportedSymbol::TypeAlias) {
                if (typeRegistry_.lookup(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
                // Ensure transitive generic dependencies (e.g. Result<T>) are registered
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec,
                                             decl->typeSpec(), currentModulePath_);
                checkTypeAliasDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Enum) {
                if (typeRegistry_.lookup(sym->name) || genericEnumTemplates_.count(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::EnumDeclContext*>(sym->decl);
                checkEnumDecl(decl);
            } else if (sym->kind == ExportedSymbol::TypeAlias) {
                if (typeRegistry_.lookup(sym->name))
                    continue;
                auto* decl = static_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
                // Ensure transitive generic dependencies
                ensureTypeDependencyFromSpec(ensureTypeDependencyFromSpec,
                                             decl->typeSpec(), ns);
                checkTypeAliasDecl(decl);
            }
        }

        // Phase B: unions (no inheritance, single pass)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Union) {
                if (genericUnionTemplates_.count(sym->name))
                    continue;
                auto* existing = typeRegistry_.lookup(sym->name);
                if (existing && !(existing->kind == TypeKind::Union && existing->fields.empty() && existing->bitWidth == 0))
                    continue;
                auto* decl = static_cast<LucisParser::UnionDeclContext*>(sym->decl);
                for (auto* field : decl->unionField())
                    ensureTypeDependencyFromSpec(
                        ensureTypeDependencyFromSpec, field->typeSpec(), currentModulePath_);
                checkUnionDecl(decl);
            }
        }
        
        // Phase B: structs (retry loop for parent ordering, covers both
        // same-module external symbols and cross-module imported symbols)
        {
            // Collect all structs: same-module + imported + transitive parents
            struct PendingStruct {
                LucisParser::StructDeclContext* decl;
                std::string ns;
            };
            std::unordered_map<std::string, PendingStruct> pendingMap;
            for (auto* sym : extSyms) {
                if (sym->kind == ExportedSymbol::Struct && !genericStructTemplates_.count(sym->name)) {
                    pendingMap[sym->name] = {
                        static_cast<LucisParser::StructDeclContext*>(sym->decl),
                        currentModulePath_
                    };
                }
            }
            for (auto& [symName, ns] : userImports_) {
                auto* sym = moduleRegistry_->findSymbol(ns, symName);
                if (sym && sym->kind == ExportedSymbol::Struct && !genericStructTemplates_.count(symName)) {
                    pendingMap[symName] = {
                        static_cast<LucisParser::StructDeclContext*>(sym->decl), ns
                    };
                }
            }
            // Add transitive parent dependencies
            bool added = true;
            while (added) {
                added = false;
                for (auto& [name, ps] : pendingMap) {
                    if (ps.decl->COLON() && ps.decl->typeSpec()) {
                        unsigned parentDims = 0;
                        auto* parentTI = resolveTypeSpec(ps.decl->typeSpec(), parentDims);
                        std::string parentName = parentTI ? parentTI->name : "";
                        if (!parentName.empty() && !pendingMap.count(parentName)) {
                            auto* parentSym = moduleRegistry_->findSymbol(ps.ns, parentName);
                            if (!parentSym) {
                                for (auto& mod : moduleRegistry_->allModules()) {
                                    parentSym = moduleRegistry_->findSymbol(mod, parentName);
                                    if (parentSym) break;
                                }
                            }
                            if (parentSym && parentSym->kind == ExportedSymbol::Struct
                                && !genericStructTemplates_.count(parentName)) {
                                pendingMap[parentName] = {
                                    static_cast<LucisParser::StructDeclContext*>(parentSym->decl),
                                    parentSym->modulePath
                                };
                                added = true;
                            }
                        }
                    }
                }
            }

            std::unordered_set<std::string> processed;
            bool madeProgress = true;
            while (madeProgress) {
                madeProgress = false;
                for (auto& [name, ps] : pendingMap) {
                    if (processed.count(name)) continue;
                    auto* existing = typeRegistry_.lookup(name);
                    if (existing && !(existing->kind == TypeKind::Struct && existing->fields.empty() && existing->bitWidth == 0)) {
                        processed.insert(name);
                        continue;
                    }

            if (ps.decl->COLON() && ps.decl->typeSpec()) {
                unsigned parentDims = 0;
                auto* parentTI = resolveTypeSpec(ps.decl->typeSpec(), parentDims);
                if (!parentTI || parentTI->kind != TypeKind::Struct || parentTI->fields.empty())
                    continue;
            }

                    for (auto* field : ps.decl->structField())
                        ensureTypeDependencyFromSpec(
                            ensureTypeDependencyFromSpec, field->typeSpec(), ps.ns);
                    checkStructDecl(ps.decl);
                    processed.insert(name);
                    madeProgress = true;
                }
            }
        }

        // Phase B unions: user imports (no inheritance, single pass)
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || sym->kind != ExportedSymbol::Union) continue;
            if (genericUnionTemplates_.count(sym->name)) continue;
            auto* existing = typeRegistry_.lookup(sym->name);
            if (existing && !(existing->kind == TypeKind::Union && existing->fields.empty() && existing->bitWidth == 0))
                continue;
            auto* decl = static_cast<LucisParser::UnionDeclContext*>(sym->decl);
            for (auto* field : decl->unionField())
                ensureTypeDependencyFromSpec(
                    ensureTypeDependencyFromSpec, field->typeSpec(), ns);
            auto* recheck = typeRegistry_.lookup(sym->name);
            if (!recheck || (recheck->kind == TypeKind::Union && recheck->fields.empty() && recheck->bitWidth == 0))
                checkUnionDecl(decl);
        }

        // Phase B.5: extend blocks for imported structs (auto-resolved)
        // When a struct is imported, its extend blocks from the same module
        // must also be registered so methods like Type::new() work.
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::ExtendBlock) {
                auto* decl = static_cast<LucisParser::ExtendDeclContext*>(sym->decl);
                checkExtendDecl(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            // For each imported struct, look for its extend block in the same ns
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym || (sym->kind != ExportedSymbol::Struct &&
                         sym->kind != ExportedSymbol::Union))
                continue;
            // Find extend block with the same name in the same module
            auto nsSyms = moduleRegistry_->getModuleSymbols(ns);
            for (auto* nsSym : nsSyms) {
                if (nsSym->kind == ExportedSymbol::ExtendBlock &&
                    nsSym->name == symName) {
                    auto* extDecl = static_cast<LucisParser::ExtendDeclContext*>(nsSym->decl);
                    // Pre-register types referenced in extend method signatures
                    // (e.g. return types like ReadFileResult) so resolveTypeSpec
                    // can find them when checkExtendDecl processes the block.
                    for (auto* method : extDecl->extendMethod()) {
                        ensureTypeDependencyFromSpec(
                            ensureTypeDependencyFromSpec, method->typeSpec(), ns);
                        for (auto* p : method->param()) {
                            ensureTypeDependencyFromSpec(
                                ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                        }
                        if (auto* pl = method->paramList()) {
                            for (auto* p : pl->param()) {
                                ensureTypeDependencyFromSpec(
                                    ensureTypeDependencyFromSpec, p->typeSpec(), ns);
                            }
                        }
                    }
                    checkExtendDecl(extDecl);
                }
            }
        }

        // Phase C: functions (may reference types from phases A and B)
        for (auto* sym : extSyms) {
            if (sym->kind == ExportedSymbol::Function) {
                auto* decl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                ensureFunctionTypeDependencies(decl, currentModulePath_);
                registerFunctionSignature(decl);
            }
        }
        for (auto& [symName, ns] : userImports_) {
            auto* sym = moduleRegistry_->findSymbol(ns, symName);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Function) {
                auto* decl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                ensureFunctionTypeDependencies(decl, ns);
                registerFunctionSignature(decl);
            }
        }
    }

    // Pass 2/3: local type registration.
    if (!moduleRegistry_) {
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* ta = decl->typeAliasDecl()) {
                checkTypeAliasDecl(ta);
            }
        }

        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                checkStructDecl(sd);
            } else if (auto* ud = decl->unionDecl()) {
                checkUnionDecl(ud);
            } else if (auto* ed = decl->enumDecl()) {
                checkEnumDecl(ed);
            }
        }
    } else {
        // Under module context, aliases/enums were already registered in pass 1.5.
        // Validate/upgrade local struct and union declarations now.
        // Skip generic structs/templates already registered via the
        // cross-module phase above — avoids duplicate registration
        // when the file path is non-canonical (e.g. LSP mode).
        for (auto* decl : tree->topLevelDecl()) {
            if (auto* sd = decl->structDecl()) {
                auto name = sd->IDENTIFIER()->getText();
                if (sd->typeParamList() && genericStructTemplates_.count(name))
                    continue;
                checkStructDecl(sd);
            } else if (auto* ud = decl->unionDecl()) {
                auto name = ud->IDENTIFIER()->getText();
                if (ud->typeParamList() && genericUnionTemplates_.count(name))
                    continue;
                checkUnionDecl(ud);
            }
        }
    }

    auto hasErrors = [&]() -> bool {
        for (const auto& d : diagnostics_) {
            if (d.severity == Diagnostic::Error)
                return true;
        }
        return false;
    };

    // Stop after type registration failures to avoid cascading diagnostics
    // (unknown types, undefined vars, and return-shape errors that are
    // direct consequences of earlier declaration errors).
    if (hasErrors())
        return false;

    // Pass 3.5: register struct methods via `extend` blocks
    // Skip generic extend blocks already registered via the
    // cross-module phase above (avoids duplicate registration
    // when the file path is non-canonical, e.g. LSP mode).
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->extendDecl()) {
            auto extName = ext->IDENTIFIER()->getText();
            if (ext->typeParamList() && genericExtendTemplates_.count(extName))
                continue;
            checkExtendDecl(ext);
        }
    }

    if (hasErrors())
        return false;

    // Validate attributes on all top-level declarations
    for (auto* decl : tree->topLevelDecl()) {
        auto* attrs = decl->functionDecl()    ? static_cast<AttributeListContext*>(decl->functionDecl()->attributeList())
                    : decl->structDecl()       ? decl->structDecl()->attributeList()
                    : decl->unionDecl()        ? decl->unionDecl()->attributeList()
                    : decl->enumDecl()         ? decl->enumDecl()->attributeList()
                    : decl->typeAliasDecl()    ? decl->typeAliasDecl()->attributeList()
                    : decl->externDecl()       ? decl->externDecl()->attributeList()
                    : decl->extendDecl()       ? decl->extendDecl()->attributeList()
                    : decl->constDeclStmt()    ? decl->constDeclStmt()->attributeList()
                    : nullptr;
        validateAttributeList(attrs, "top-level declaration");
        if (auto* ed = decl->enumDecl())
            for (auto* v : ed->enumVariant())
                validateAttributeList(v->attributeList(), "enum variant");
        if (auto* sd = decl->structDecl())
            for (auto* f : sd->structField())
                validateAttributeList(f->attributeList(), "struct field");
        if (auto* ud = decl->unionDecl())
            for (auto* f : ud->unionField())
                validateAttributeList(f->attributeList(), "union field");
    }

    // Pass 4: register function signatures (before checking bodies)
    // When moduleRegistry_ is present, external symbols are registered
    // in pass 1.5, but we must still ensure local file functions are registered
    // (avoid double-registration by checking `functions_`).
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* func = decl->functionDecl()) {
            if (func->IDENTIFIER().empty()) {
                error(func, "function must have a name");
                continue;
            }
            auto funcName = func->IDENTIFIER(0)->getText();
            if (!moduleRegistry_ || !functions_.count(funcName) || globalBuiltins_.count(funcName)) {
                registerFunctionSignature(func);
            }
        }
        if (auto* ext = decl->externDecl()) {
            checkExternDecl(ext);
        }
    }

    // Register extern functions even in module context
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->externDecl()) {
            checkExternDecl(ext);
        }
    }

    // Pass 4.5: process c_macro { ... } blocks (must precede const/var
    //            resolution so function-like macros are registered)
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cm = decl->cMacroBlock()) {
            checkCMacroBlock(cm);
        }
        if (decl->asmBBlock()) {
            // asm_b blocks are opaque — no semantic checking needed
        }
    }
    // Also process c_macro blocks at statement level (inside functions)
    // This is handled in checkStmt via the statement visitor.

    // Pass 4.6: pre-register all top-level const names (for forward references)
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cd = decl->constDeclStmt()) {
            auto decls = cd->constDeclarator();
            for (auto* d : decls) {
                auto name = d->IDENTIFIER()->getText();
VarInfo vi{typeRegistry_.lookup("int32"), 0, {}, true, false, nullptr};
                vi.isConst = true;
                vi.scopeDepth = 0;
                locals_[name] = vi;
                globalVars_[name] = vi;
            }
        }
    }

    // Pass 4.7: resolve expressions and real types for top-level consts
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* cd = decl->constDeclStmt()) {
            checkConstDeclStmt(cd);
        }
    }

    // Pass 5: check function bodies
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* func = decl->functionDecl()) {
            locals_.clear();
            // Restore top-level consts into function scope
            for (auto& [name, vi] : globalVars_)
                locals_[name] = vi;
            checkFunction(func);
        }
    }

    // Pass 5.5: check extend method bodies
    for (auto* decl : tree->topLevelDecl()) {
        if (auto* ext = decl->extendDecl()) {
            checkExtendMethodBodies(ext);
        }
    }

    // Only actual errors (not warnings) should cause a check failure
    verifySemanticDBConsistency();
    return !hasErrors();
}

// ── Phase 3: Consistency verification ────────────────────────────────────────

void Checker::verifySemanticDBConsistency() {
    if (!semanticDB_) return;

    // Count types in each registry for diagnostic purposes
    size_t trTypes = typeRegistry_.allTypes().size();
    size_t sdTypes = 0;
    for (const auto* d : semanticDB_->allTypes()) {
        if (!d->modulePath.empty() || d->kind == semantic::DeclKind::Struct) ++sdTypes;
    }

    // Verify key structural invariants
    for (const auto& name : typeRegistry_.allTypes()) {
        auto* trType = typeRegistry_.lookup(name);
        if (!trType) continue;

        // Skip primitives
        if (trType->kind == TypeKind::Integer || trType->kind == TypeKind::Float ||
            trType->kind == TypeKind::Bool    || trType->kind == TypeKind::Char ||
            trType->kind == TypeKind::Void    || trType->kind == TypeKind::String ||
            trType->kind == TypeKind::Pointer || trType->kind == TypeKind::Function ||
            trType->kind == TypeKind::VAList  || trType->kind == TypeKind::Extended)
            continue;

        auto* sdDecl = semanticDB_->lookupAny(name);
        if (!sdDecl && !semanticDB_->hasForwardDecl(name)) {
            // User-defined type registered in TypeRegistry but missing from SemanticDB
            // This indicates a gap in Phase 1 sync coverage
            continue; // non-fatal during transition
        }
    }

    (void)trTypes; (void)sdTypes;
}

// ═══════════════════════════════════════════════════════════════════════
//  Type resolution helpers
// ═══════════════════════════════════════════════════════════════════════

const TypeInfo* Checker::resolveTypeSpec(LucisParser::TypeSpecContext* ctx,
                                         unsigned& arrayDims) {
    arrayDims = 0;
    auto* cur = ctx;

    // Unwrap array dimensions: [][]T → arrayDims=2, [N]T → arrayDims=1 (fixed)
    while (cur && cur->LBRACKET()) {
        arrayDims++;
        if (cur->INT_LIT()) {
            // Fixed-size array [N]T — validate size is positive
            int64_t size = std::stoll(cur->INT_LIT()->getText());
            if (size <= 0) {
                error(cur, "array size must be a positive integer, got " +
                      cur->INT_LIT()->getText());
                return nullptr;
            }
        } else if (cur->IDENTIFIER()) {
            // Fixed-size array [IDENTIFIER]T — must be a const with compile-time value
            auto identName = cur->IDENTIFIER()->getText();
            
            // Check if it's a const variable with known value
            auto lit = locals_.find(identName);
            if (lit != locals_.end() && lit->second.isConst) {
                auto cit = compileTimeValues_.find(identName);
                if (cit != compileTimeValues_.end()) {
                    if (cit->second.kind() != ComptimeValue::Kind::Int) {
                        error(cur, "const '" + identName + "' must be an integer to be used as array size");
                        return nullptr;
                    }
                    int64_t size = cit->second.asInt();
                    if (size <= 0) {
                        error(cur, "array size must be a positive integer, got '" +
                              identName + "' = " + std::to_string(size));
                        return nullptr;
                    }
                } else {
                    error(cur, "const '" + identName + "' must have a literal initializer to be used as array size");
                    return nullptr;
                }
            } else {
                error(cur, "array dimension must be a compile-time constant (literal or const); '" +
                      identName + "' is not a const");
                return nullptr;
            }
        }
        cur = cur->typeSpec().empty() ? nullptr : cur->typeSpec(0);
    }

    // Pointer type: *T
    if (cur && cur->STAR()) {
        auto* childTS = cur->typeSpec(0);
        if (childTS) {
            auto starIdx = cur->STAR()->getSymbol()->getTokenIndex();
            auto childIdx = childTS->getStart()->getTokenIndex();
            if (starIdx > childIdx) {
                // Postfix star: type* — wrong C-style syntax
                error(cur, "invalid pointer syntax: use '*" +
                      childTS->getText() + "' instead of '" +
                      childTS->getText() + "*'");
                return nullptr;
            }
        }
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpec(cur->typeSpec(0), innerDims);
        if (!inner) return nullptr;
        // Keep inner array depth for pointer-to-array types like *[N]T.
        arrayDims = innerDims;
        return getPointerType(inner);
    }

    // Function type: fn(params) -> ret
    if (cur && cur->fnTypeSpec()) {
        auto* fnSpec = cur->fnTypeSpec();
        auto specs = fnSpec->typeSpec();
        if (specs.empty()) return nullptr;

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(specs.back(), retDims);
        if (!retType) return nullptr;

        std::vector<const TypeInfo*> paramTypes;
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(specs[i], pDims);
            if (!pType) return nullptr;
            paramTypes.push_back(pType);
        }

        return makeFunctionType(retType, paramTypes);
    }

    // Generic extended type: Vec<int32>, Map<string, int32>, etc.
    // Built-in collection types (vec, map, set) — no import required
    if (!cur) {
        error(ctx, "invalid type specification");
        return nullptr;
    }
    std::string baseName;
    if (cur->VEC())      baseName = "Vec";
    else if (cur->MAP()) baseName = "Map";
    else if (cur->SET()) baseName = "Set";

    // tuple<T1, T2, ...> type
    if (cur->TUPLE()) {
        auto typeParams = cur->typeSpec();
        if (typeParams.size() < 2) {
            error(cur, "tuple requires at least 2 type parameters");
            return nullptr;
        }
        std::vector<const TypeInfo*> elemTypes;
        std::string fullName = "tuple<";
        for (size_t i = 0; i < typeParams.size(); i++) {
            unsigned elemDims = 0;
            auto* elemType = resolveTypeSpec(typeParams[i], elemDims);
            if (!elemType) return nullptr;
            elemTypes.push_back(elemType);
            if (i > 0) fullName += ", ";
            fullName += elemType->name;
        }
        fullName += ">";

        // Check for cached tuple type
        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Tuple;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->tupleElements = std::move(elemTypes);
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    if (!baseName.empty()) {
        auto* extDesc = extTypeRegistry_.lookup(baseName);
        if (!extDesc) {
            error(cur, "'" + baseName + "' is not a known generic type");
            return nullptr;
        }
        auto typeParams = cur->typeSpec();
        if (typeParams.empty()) {
            error(cur, "generic type '" + baseName + "' requires type parameters");
            return nullptr;
        }

        if (extDesc->genericArity == 1) {
            if (typeParams.size() != 1) {
                error(cur, "'" + baseName + "' expects 1 type parameter, got " +
                           std::to_string(typeParams.size()));
                return nullptr;
            }
            unsigned elemDims = 0;
            auto* elemType = resolveTypeSpec(typeParams[0], elemDims);
            if (!elemType) return nullptr;

            if (elemType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "<" + elemType->name + ">' — " +
                           "'" + baseName + "' cannot contain '" +
                           elemType->extendedKind + "' as element type");
                return nullptr;
            }

            auto fullName = baseName + "<" + elemType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = elemType->builtinSuffix;
            ti->elementType = elemType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else if (extDesc->genericArity == 2) {
            if (typeParams.size() != 2) {
                error(cur, "'" + baseName + "' expects 2 type parameters, got " +
                           std::to_string(typeParams.size()));
                return nullptr;
            }
            unsigned keyDims = 0, valDims = 0;
            auto* keyType = resolveTypeSpec(typeParams[0], keyDims);
            auto* valType = resolveTypeSpec(typeParams[1], valDims);
            if (!keyType || !valType) return nullptr;

            if (keyType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + keyType->extendedKind +
                           "' as key type");
                return nullptr;
            }
            if (valType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + valType->extendedKind +
                           "' as value type");
                return nullptr;
            }

            auto fullName = baseName + "<" + keyType->name + ", " + valType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
            ti->keyType = keyType;
            ti->valueType = valType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else {
            error(cur, "unsupported generic arity for '" + baseName + "'");
            return nullptr;
        }
    }

    // User-defined and extended generic types (e.g., Task<int32>, Node<int32>)
    if (cur->LT()) {
        auto baseName = cur->IDENTIFIER()->getText();
        auto typeParams = cur->typeSpec();

        if (typeParams.empty()) {
            error(cur, "generic type '" + baseName + "' requires type parameters");
            return nullptr;
        }

        // Resolve all type arguments first
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* tp : typeParams) {
            unsigned argDims = 0;
            auto* argTI = resolveTypeSpec(tp, argDims);
            if (!argTI) return nullptr;
            resolvedArgs.push_back(argTI);
        }

        // Check user-defined generic struct templates first
        auto structIt = genericStructTemplates_.find(baseName);
        if (structIt != genericStructTemplates_.end()) {
            return instantiateGenericStruct(baseName, structIt->second, resolvedArgs, cur);
        }
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (unionIt != genericUnionTemplates_.end()) {
            return instantiateGenericUnion(baseName, unionIt->second, resolvedArgs, cur);
        }
        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt != genericEnumTemplates_.end()) {
            return instantiateGenericEnum(baseName, enumIt->second, resolvedArgs, cur);
        }

        // Fall through to known extended types (Task, etc.)
        auto* extDesc = extTypeRegistry_.lookup(baseName);
        if (!extDesc) {
            error(cur, "'" + baseName + "' is not a known generic type");
            return nullptr;
        }
        if (!imports_.isImported(baseName)) {
            error(cur, "type '" + baseName + "' is not imported");
            return nullptr;
        }

        if (extDesc->genericArity == 1) {
            if (resolvedArgs.size() != 1) {
                error(cur, "'" + baseName + "' expects 1 type parameter, got " +
                           std::to_string(resolvedArgs.size()));
                return nullptr;
            }
            auto* elemType = resolvedArgs[0];

            if (elemType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "<" + elemType->name + ">' — " +
                           "'" + baseName + "' cannot contain '" +
                           elemType->extendedKind + "' as element type");
                return nullptr;
            }

            auto fullName = baseName + "<" + elemType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = elemType->builtinSuffix;
            ti->elementType = elemType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else if (extDesc->genericArity == 2) {
            if (resolvedArgs.size() != 2) {
                error(cur, "'" + baseName + "' expects 2 type parameters, got " +
                           std::to_string(resolvedArgs.size()));
                return nullptr;
            }
            auto* keyType = resolvedArgs[0];
            auto* valType = resolvedArgs[1];

            if (keyType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + keyType->extendedKind +
                           "' as key type");
                return nullptr;
            }
            if (valType->kind == TypeKind::Extended) {
                error(cur, "nested collection types are not supported: '" +
                           baseName + "' cannot use '" + valType->extendedKind +
                           "' as value type");
                return nullptr;
            }

            auto fullName = baseName + "<" + keyType->name + ", " + valType->name + ">";
            for (auto& dt : dynamicTypes_) {
                if (dt->name == fullName)
                    return dt.get();
            }
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
            ti->keyType = keyType;
            ti->valueType = valType;
            ti->extendedKind = baseName;
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        } else {
            error(cur, "unsupported generic arity for '" + baseName + "'");
            return nullptr;
        }
    }

    // Auto type inference: auto x = 42;
    if (cur->AUTO()) return nullptr;  // nullptr signals "infer from initializer"

    // Primitive or named type
    auto name = cur->getText();

    // cstring is a built-in alias for *char
    if (name == "cstring")
        return getPointerType(typeRegistry_.lookup("char"));

    auto* ti = typeRegistry_.lookup(name);
    if (!ti) {
        error(cur, "unknown type '" + name + "'");
    }
    return ti;
}

const TypeInfo* Checker::getPointerType(const TypeInfo* pointee) {
    for (auto& dt : dynamicTypes_) {
        if (dt->kind == TypeKind::Pointer && dt->pointeeType == pointee)
            return dt.get();
    }
    auto ti = std::make_unique<TypeInfo>();
    ti->name = "*" + pointee->name;
    ti->kind = TypeKind::Pointer;
    ti->bitWidth = 0;
    ti->isSigned = false;
    // For *char, use "cstr" suffix to support C string functions in std::log
    if (pointee->name == "char") {
        ti->builtinSuffix = "cstr";
    } else {
        ti->builtinSuffix = "ptr";
    }
    ti->pointeeType = pointee;
    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}

const TypeInfo* Checker::makeFunctionType(const TypeInfo* returnType,
                                           const std::vector<const TypeInfo*>& paramTypes,
                                           bool isVariadic,
                                           const TypeInfo* variadicElementType) {
    auto ti = std::make_unique<TypeInfo>();
    ti->kind = TypeKind::Function;
    ti->bitWidth = 0;
    ti->isSigned = false;
    ti->builtinSuffix = "ptr";
    ti->returnType = returnType;
    ti->paramTypes = paramTypes;
    ti->isVariadic = isVariadic;
    ti->variadicElementType = variadicElementType;

    ti->name = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ti->name += ",";
        if (isVariadic && i == paramTypes.size() - 1)
            ti->name += "...";
        ti->name += paramTypes[i]->name;
    }
    ti->name += ")->" + returnType->name;

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}

std::string Checker::resolveBaseTypeName(LucisParser::TypeSpecContext* ctx) {
    if (!ctx) return "";
    while (!ctx->typeSpec().empty())
        ctx = ctx->typeSpec(0);
    if (!ctx) return "";
    auto text = ctx->getText();
    while (!text.empty() && text[0] == '*')
        text = text.substr(1);
    return text;
}

const TypeInfo* Checker::resolveBuiltinReturnType(const std::string& retName) {
    // Handle "Vec<T>" return types from builtins
    if (retName.size() > 4 && retName.substr(0, 4) == "Vec<" && retName.back() == '>') {
        std::string elemName = retName.substr(4, retName.size() - 5);
        auto* elemType = typeRegistry_.lookup(elemName);
        if (!elemType) return nullptr;

        auto fullName = retName;
        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Extended;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = elemType->builtinSuffix;
        ti->elementType = elemType;
        ti->extendedKind = "Vec";
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }
    return typeRegistry_.lookup(retName);
}

// ═══════════════════════════════════════════════════════════════════════
//  Type query helpers
// ═══════════════════════════════════════════════════════════════════════

static std::string formatParamTypes(const std::vector<const TypeInfo*>& types) {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); i++) {
        if (i > 0) result += ", ";
        result += types[i] ? types[i]->name : "?";
    }
    result += ")";
    return result;
}

static std::string formatParamTypes(const std::vector<std::string>& types) {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); i++) {
        if (i > 0) result += ", ";
        result += types[i];
    }
    result += ")";
    return result;
}

bool Checker::isNumeric(const TypeInfo* ti) {
    return ti && (ti->kind == TypeKind::Integer || ti->kind == TypeKind::Float);
}

bool Checker::isInteger(const TypeInfo* ti) {
    return ti && ti->kind == TypeKind::Integer;
}

bool Checker::isIntegerOrPointer(const TypeInfo* ti) {
    return ti && (ti->kind == TypeKind::Integer || ti->kind == TypeKind::Pointer);
}

bool Checker::isConditionType(const TypeInfo* ti) {
    if (!ti) return true;  // null → unknown, don't flag
    return ti->kind == TypeKind::Bool ||
           ti->kind == TypeKind::Integer ||
           ti->kind == TypeKind::Float;
}

bool Checker::isAssignable(const TypeInfo* lhs, const TypeInfo* rhs) {
    if (!lhs || !rhs) return true;
    if (lhs == rhs) return true;
    if (lhs->name == rhs->name) return true;

    // Integer ↔ integer (same signedness required)
    if (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer) {
        if (lhs->isSigned == rhs->isSigned) return true;
        unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
        unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
        // Unsigned → wider signed: safe (no sign loss)
        if (!lhs->isSigned && rhs->isSigned && lhsBW <= rhsBW) return true;
        if (!rhs->isSigned && lhs->isSigned && rhsBW <= lhsBW) return true;
    }

    // Enum ↔ integer (C enums are integers)
    if (lhs->kind == TypeKind::Enum && rhs->kind == TypeKind::Integer)
        return true;
    if (lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Enum)
        return true;
    if (lhs->kind == TypeKind::Enum && rhs->kind == TypeKind::Enum)
        return true;

    // Float ↔ float
    if (lhs->kind == TypeKind::Float && rhs->kind == TypeKind::Float)
        return true;

    // Function ↔ function
    if (lhs->kind == TypeKind::Function && rhs->kind == TypeKind::Function)
        return true;

    // Function pointer ↔ function (C callback decay)
    if (lhs->kind == TypeKind::Pointer && lhs->pointeeType &&
        lhs->pointeeType->kind == TypeKind::Function &&
        rhs->kind == TypeKind::Function)
        return true;
    if (rhs->kind == TypeKind::Pointer && rhs->pointeeType &&
        rhs->pointeeType->kind == TypeKind::Function &&
        lhs->kind == TypeKind::Function)
        return true;

    // Same kind
    if (lhs->kind == rhs->kind) return true;

    // Struct inheritance: Child is-assignable-to Parent
    if (lhs->kind == TypeKind::Struct && rhs->kind == TypeKind::Struct) {
        for (auto* ti = rhs; ti; ti = ti->parentType) {
            if (ti == lhs) return true;
        }
    }

    return false;
}

// Returns true if `from` can be safely widened (promoted) to `to`.
// Only widening conversions: int8→int16→int32→int64, float32→float64, etc.
// Same signedness only — no silent signed↔unsigned conversions.
static bool canWidenTo(const TypeInfo* from, const TypeInfo* to) {
    if (!from || !to) return false;
    if (from == to) return true;
    if (from->name == to->name) return true;

    // Integer → wider integer (same signedness)
    if (from->kind == TypeKind::Integer && to->kind == TypeKind::Integer) {
        if (from->isSigned != to->isSigned) return false;
        unsigned fromBW = from->bitWidth;
        unsigned toBW = to->bitWidth;
        if (fromBW == 0) fromBW = 64; // isize/usize → pointer width
        if (toBW == 0) toBW = 64;
        return fromBW <= toBW;
    }

    // Float → wider float
    if (from->kind == TypeKind::Float && to->kind == TypeKind::Float)
        return from->bitWidth <= to->bitWidth;

    return false;
}

void Checker::checkNegativeToUnsigned(const TypeInfo* target,
                                       LucisParser::ExpressionContext* expr,
                                       antlr4::ParserRuleContext* ctx) {
    if (!target || target->kind != TypeKind::Integer || target->isSigned)
        return;

    // Unwrap try expression
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr))
        expr = te->expression(0);

    // Case 1: Positive integer literal → always safe for unsigned
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr))
        return;

    // Case 2: Negative literal (-N) → always invalid for unsigned
    if (dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        error(ctx, "cannot assign negative value to unsigned type '" +
                    target->name + "'");
        return;
    }

    // Case 3: C macro constant — check its known value
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto id = ident->IDENTIFIER()->getText();
        auto cit = cEnumConstants_.find(id);
        if (cit != cEnumConstants_.end()) {
            if (cit->second.isFloat || cit->second.isString)
                return;  // float/string macros are not relevant here
            if (cit->second.value < 0) {
                error(ctx, "cannot assign negative value " +
                            std::to_string(cit->second.value) +
                            " ('" + id + "') to unsigned type '" +
                            target->name + "'");
            }
            return;  // known non-negative macro → OK
        }
    }

    // Case 4: Explicit cast (expr as uint32) → user took responsibility
    if (dynamic_cast<LucisParser::CastExprContext*>(expr))
        return;
}

// ═══════════════════════════════════════════════════════════════════════
//  Lambda / Closure helpers
// ═══════════════════════════════════════════════════════════════════════

static std::string buildLambdaTypeName(const TypeInfo* retType,
                                        const std::vector<const TypeInfo*>& paramTypes) {
    std::string name = "fn(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) name += ", ";
        name += paramTypes[i]->name;
    }
    name += ") -> " + (retType ? retType->name : "void");
    return name;
}

static std::string buildClosureTypeName(const TypeInfo* retType,
                                          const std::vector<const TypeInfo*>& paramTypes,
                                          const std::vector<TypeInfo::CaptureInfo>& captures) {
    std::string name = "closure(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) name += ", ";
        name += paramTypes[i]->name;
    }
    name += ") -> " + (retType ? retType->name : "void");
    name += " captures [";
    for (size_t i = 0; i < captures.size(); i++) {
        if (i > 0) name += ", ";
        name += captures[i].name + ": " + (captures[i].type ? captures[i].type->name : "?");
    }
    name += "]";
    return name;
}

static void collectFreeVars(LucisParser::ExpressionContext* expr,
                             const std::unordered_set<std::string>& paramNames,
                             std::unordered_set<std::string>& freeVars) {
    if (!expr) return;
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        if (paramNames.find(name) == paramNames.end())
            freeVars.insert(name);
        return;
    }
    // Recurse into all child expression nodes
    for (size_t i = 0; i < expr->children.size(); i++) {
        if (auto* child = dynamic_cast<LucisParser::ExpressionContext*>(expr->children[i])) {
            collectFreeVars(child, paramNames, freeVars);
        }
    }
}

static void collectFreeVarsFromBlock(LucisParser::BlockContext* block,
                                      const std::unordered_set<std::string>& paramNames,
                                      std::unordered_set<std::string>& freeVars) {
    if (!block) return;
    for (auto* stmt : block->statement()) {
        // Check all expression contexts within the statement
        for (size_t i = 0; i < stmt->children.size(); i++) {
            if (auto* child = dynamic_cast<LucisParser::ExpressionContext*>(stmt->children[i])) {
                collectFreeVars(child, paramNames, freeVars);
            }
        }
    }
}

const TypeInfo* Checker::resolveLambdaExpr(LucisParser::LambdaExprContext* lexpr) {
    auto id = lambdaCounter_++;
    auto funcName = "__lambda_" + std::to_string(id);

    // Parse params
    std::vector<const TypeInfo*> paramTypes;
    std::unordered_set<std::string> paramNames;
    auto* params = lexpr->paramList();
    if (params) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue; // Skip variadic for now
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(p->typeSpec(), dims);
            if (!ti) { error(p, "unknown type in lambda parameter"); continue; }
            auto pname = p->IDENTIFIER()->getText();
            paramTypes.push_back(ti);
            paramNames.insert(pname);
            // Temporarily register as local for body resolution
            locals_[pname] = {ti, dims, {}, true, true, nullptr};
        }
    }

    // Resolve body expression type → return type
    auto* bodyExpr = lexpr->expression();
    auto* retType = resolveExprType(bodyExpr);
    if (!retType) retType = typeRegistry_.lookup("int32");

    // Analyze captures: free vars that are local variables in enclosing scope
    std::unordered_set<std::string> freeVars;
    collectFreeVars(bodyExpr, paramNames, freeVars);

    std::vector<TypeInfo::CaptureInfo> captures;
    for (auto& fv : freeVars) {
        auto it = locals_.find(fv);
        if (it != locals_.end()) {
            // Only capture locals, not globals/functions
            captures.push_back({fv, it->second.type, it->second.arrayDims});
        }
    }

    // Remove temporary params from scope
    for (auto& pn : paramNames)
        locals_.erase(pn);

    // Build the function/closure TypeInfo
    auto ti = std::make_unique<TypeInfo>();
    ti->name = funcName;
    ti->returnType = retType;
    ti->paramTypes = paramTypes;
    ti->syntheticFuncName = funcName;

    // Store lambda info for IRGen
    LambdaInfo linfo;
    linfo.funcName = funcName;
    linfo.returnType = retType;
    linfo.paramTypes = paramTypes;
    linfo.captures = captures;
    linfo.isBlock = false;
    linfo.exprCtx = bodyExpr;
    lambdaInfo_[lexpr] = std::move(linfo);

    if (captures.empty()) {
        // Non-capturing → regular function type
        ti->kind = TypeKind::Function;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "ptr";
        ti->name = buildLambdaTypeName(retType, paramTypes);
    } else {
        // Capturing → closure type
        ti->kind = TypeKind::Closure;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "closure";
        ti->closureCaptures = std::move(captures);
        ti->name = buildClosureTypeName(retType, paramTypes, ti->closureCaptures);
    }

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}

const TypeInfo* Checker::resolveLambdaBlockExpr(LucisParser::LambdaBlockExprContext* lblk) {
    auto id = lambdaCounter_++;
    auto funcName = "__lambda_" + std::to_string(id);

    // Parse params
    std::vector<const TypeInfo*> paramTypes;
    std::unordered_set<std::string> paramNames;
    auto* params = lblk->paramList();
    if (params) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue;
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(p->typeSpec(), dims);
            if (!ti) { error(p, "unknown type in lambda parameter"); continue; }
            auto pname = p->IDENTIFIER()->getText();
            paramTypes.push_back(ti);
            paramNames.insert(pname);
            locals_[pname] = {ti, dims, {}, true, true, nullptr};
        }
    }

    // Walk block to find return type and register locals
    auto* block = lblk->block();
    const TypeInfo* retType = typeRegistry_.lookup("void");
    for (auto* stmt : block->statement()) {
        if (auto* retStmt = stmt->returnStmt()) {
            if (auto* retExpr = retStmt->expression()) {
                auto* exprType = resolveExprType(retExpr);
                if (exprType) retType = exprType;
            }
            break;  // first return stmt determines the type
        }
        if (auto* varDecl = stmt->varDeclStmt()) {
            // Process var decls to keep locals_ populated
            auto* typeSpec = varDecl->typeSpec();
            for (auto* d : varDecl->varDeclarator()) {
                unsigned dims = 0;
                auto* ti = typeSpec ? resolveTypeSpec(typeSpec, dims) : nullptr;
                if (!ti) continue;
                auto vname = d->IDENTIFIER()->getText();
                locals_[vname] = {ti, dims, {}, true, true, nullptr};
                if (auto* initExpr = d->expression()) {
                    resolveExprType(initExpr);
                }
            }
        }
        if (auto* exprStmt = stmt->exprStmt()) {
            resolveExprType(exprStmt->expression());
        }
    }
    if (retType->kind == TypeKind::Void) {
        retType = typeRegistry_.lookup("int32");
    }

    // Analyze captures
    std::unordered_set<std::string> freeVars;
    collectFreeVarsFromBlock(block, paramNames, freeVars);

    std::vector<TypeInfo::CaptureInfo> captures;
    for (auto& fv : freeVars) {
        auto it = locals_.find(fv);
        if (it != locals_.end()) {
            captures.push_back({fv, it->second.type, it->second.arrayDims});
        }
    }

    // Remove temporary params
    for (auto& pn : paramNames)
        locals_.erase(pn);

    auto ti = std::make_unique<TypeInfo>();
    ti->returnType = retType;
    ti->paramTypes = paramTypes;
    ti->syntheticFuncName = funcName;

    LambdaInfo linfo;
    linfo.funcName = funcName;
    linfo.returnType = retType;
    linfo.paramTypes = paramTypes;
    linfo.captures = captures;
    linfo.isBlock = true;
    linfo.blockCtx = block;
    lambdaInfo_[lblk] = std::move(linfo);

    if (captures.empty()) {
        ti->kind = TypeKind::Function;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "ptr";
        ti->name = buildLambdaTypeName(retType, paramTypes);
    } else {
        ti->kind = TypeKind::Closure;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->builtinSuffix = "closure";
        ti->closureCaptures = std::move(captures);
        ti->name = buildClosureTypeName(retType, paramTypes, ti->closureCaptures);
    }

    const TypeInfo* raw = ti.get();
    dynamicTypes_.push_back(std::move(ti));
    return raw;
}

// ═══════════════════════════════════════════════════════════════════════
//  Expression type resolution
// ═══════════════════════════════════════════════════════════════════════

const TypeInfo* Checker::resolveExprType(LucisParser::ExpressionContext* expr) {
    if (!expr) return nullptr;

    // Guard against infinite recursion in pathological type expressions
    recursionDepth_++;
    if (recursionDepth_ > MAX_RECURSION_DEPTH) {
        recursionDepth_--;
        error(expr, "type expression nesting exceeds maximum recursion depth of " +
                    std::to_string(MAX_RECURSION_DEPTH));
        return typeRegistry_.lookup("int64");  // fallback type
    }

    auto resolveTypeSpecInContext = [&](LucisParser::TypeSpecContext* ts,
                                        unsigned& dims) -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(ts, activeTypeSubst_, dims);
        return resolveTypeSpec(ts, dims);
    };

    // ── Helper: true if expr is a plain integer literal (not a variable, call, etc.)
    auto isIntLitExpr = [](LucisParser::ExpressionContext* e) -> bool {
        return dynamic_cast<LucisParser::IntLitExprContext*>(e) ||
               dynamic_cast<LucisParser::HexLitExprContext*>(e) ||
               dynamic_cast<LucisParser::OctLitExprContext*>(e) ||
               dynamic_cast<LucisParser::BinLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(e) ||
               dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(e);
    };

    // ── Suffixed literals (Rust-style: 42u64, 0xFFi8, 3.14f32) ────────
    static const std::unordered_map<std::string, std::string> kSuffixTypeMap = {
        {"i8", "int8"}, {"i16", "int16"}, {"i32", "int32"}, {"i64", "int64"},
        {"i128", "int128"}, {"iinf", "intinf"}, {"isize", "isize"},
        {"u8", "uint8"}, {"u16", "uint16"}, {"u32", "uint32"}, {"u64", "uint64"},
        {"u128", "uint128"}, {"usize", "usize"},
        {"f32", "float32"}, {"f64", "float64"}, {"f80", "float80"}, {"f128", "float128"}
    };
    auto resolveSuffixed = [&](const std::string& text) -> const TypeInfo* {
        for (auto& [suf, typeName] : kSuffixTypeMap) {
            if (text.size() > suf.size() &&
                text.compare(text.size() - suf.size(), suf.size(), suf) == 0)
                return typeRegistry_.lookup(typeName);
        }
        return nullptr;
    };

    if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(si->SUFFIXED_INT()->getText())) return t;
    }
    if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sh->SUFFIXED_HEX()->getText())) return t;
    }
    if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(so->SUFFIXED_OCT()->getText())) return t;
    }
    if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sb->SUFFIXED_BIN()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sf->SUFFIXED_FLOAT()->getText())) return t;
    }
    if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sd->SUFFIXED_DOT_FLOAT()->getText())) return t;
    }
    if (auto* si = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(si->SUFFIXED_INT_FLOAT()->getText())) return t;
    }
    if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr)) {
        if (auto* t = resolveSuffixed(sf->SUFFIXED_FLOAT_INT()->getText())) return t;
    }

    // ── Unsuffixed literals ───────────────────────────────────────────
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr))
        return typeRegistry_.lookup("int32");

    if (dynamic_cast<LucisParser::FloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr))
        return typeRegistry_.lookup("float64");

    if (dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return typeRegistry_.lookup("bool");

    if (dynamic_cast<LucisParser::CharLitExprContext*>(expr))
        return typeRegistry_.lookup("char");

    if (dynamic_cast<LucisParser::StrLitExprContext*>(expr))
        return typeRegistry_.lookup("string");

    // C string literal: c"hello" → *char (null-terminated)
    if (dynamic_cast<LucisParser::CStrLitExprContext*>(expr))
        return getPointerType(typeRegistry_.lookup("char"));

    // Backtick strings — all return `string`
    if (dynamic_cast<LucisParser::BtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::RawBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::IntBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::ShellBtickExprContext*>(expr) ||
        dynamic_cast<LucisParser::CmptBtickExprContext*>(expr))
        return typeRegistry_.lookup("string");

    // ── Inline assembly expression ────────────────────────────
    if (auto* asmE = dynamic_cast<LucisParser::AsmExprContext*>(expr)) {
        auto* outList = asmE->asmOutputList();
        size_t numOutputs = outList ? outList->asmOutput().size() : 0;

        // Validate outputs
        if (outList) {
            for (auto* out : outList->asmOutput()) {
                auto raw = out->constraint->getText();
                auto constraint = raw.substr(1, raw.size() - 2);
                if (constraint.empty()) {
                    error(out, "asm output constraint cannot be empty");
                } else if (constraint[0] != '=' && constraint[0] != '+'
                           && !std::isdigit(constraint[0])) {
                    error(out, "asm output constraint must start with '=', '+', or a digit");
                }

                if (auto* ident = out->IDENTIFIER()) {
                    auto varName = ident->getText();
                    auto it = locals_.find(varName);
                    if (it == locals_.end()) {
                        error(out, "undefined variable '" + varName + "' in asm output");
                    } else {
                        it->second.used = true;
                    }
                }
            }
        }

        // Validate inputs
        if (auto* inList = asmE->asmInputList()) {
            for (auto* operand : inList->asmOperand()) {
                auto raw = operand->constraint->getText();
                auto constraint = raw.substr(1, raw.size() - 2);
                if (constraint.empty()) {
                    error(operand, "asm input constraint cannot be empty");
                } else if (constraint[0] == '=') {
                    error(operand, "asm input constraint cannot use '=' (reserved for outputs)");
                } else if (constraint[0] == '+') {
                    error(operand, "asm input constraint cannot use '+' (use in output with '+r' instead)");
                } else if (std::isdigit(constraint[0])) {
                    int matchIdx = std::stoi(constraint);
                    if (matchIdx < 0 || static_cast<size_t>(matchIdx) >= numOutputs) {
                        error(operand, "asm matching constraint '" + constraint
                              + "' refers to output " + constraint + " but there "
                              + (numOutputs == 1 ? "is only 1 output"
                                                 : "are only " + std::to_string(numOutputs) + " outputs"));
                    }
                }
                resolveExprType(operand->expression());
            }
        }

        if (numOutputs == 0)
            return typeRegistry_.lookup("void");
        auto* output = outList->asmOutput()[0];
        if (auto* ident = output->IDENTIFIER()) {
            auto varName = ident->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end())
                return it->second.type;
        }
        // Unnamed output: return int64 (register-wide default for auto)
        return typeRegistry_.lookup("int64");
    }

    if (dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return nullptr; // null is compatible with any pointer

    // ── Identifier ───────────────────────────────────────────────────
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        auto it = locals_.find(name);
        if (it != locals_.end()) {
            validateExprNotMoved(expr, id);
            // Warn on use of uninitialized variable
            if (!it->second.initialized) {
                warning(id, "variable '" + name + "' is used before being initialized");
            }
            it->second.used = true;
            return it->second.type;
        }

        auto fit = functions_.find(name);
        if (fit != functions_.end())
            return fit->second;

        // Comptime functions referenced as values
        if (comptimeRegistry_.isComptime(name))
            return typeRegistry_.lookup("int32");

        // C enum constants from #include headers
        auto ceit = cEnumConstants_.find(name);
        if (ceit != cEnumConstants_.end())
            return ceit->second.type;

        // C global variables from #include headers
        auto cgit = cGlobals_.find(name);
        if (cgit != cGlobals_.end())
            return cgit->second;

        // Imported symbols — check constant registry first, then treat as
        // polymorphic function reference (nullptr).
        if (imports_.isImported(name)) {
            auto& constType = builtinRegistry_.lookupConstant(name);
            if (!constType.empty())
                return typeRegistry_.lookup(constType);
            return nullptr;
        }

        // User module import or same-module symbol
        if (userImports_.count(name)) {
            auto modPath = userImports_[name];
            if (moduleRegistry_) {
                auto* sym = moduleRegistry_->findSymbol(modPath, name);
                if (sym && sym->kind == ExportedSymbol::Constant) {
                    // If const has explicit type spec, resolve it; otherwise infer from expression
                    if (auto* cd = dynamic_cast<LucisParser::ConstDeclaratorContext*>(sym->decl)) {
                        if (cd->typeSpec()) {
                            unsigned dims = 0;
                            auto* ti = resolveTypeSpec(cd->typeSpec(), dims);
                            if (ti) return ti;
                        }
                        // Infer type from initializer expression
                        if (cd->expression()) {
                            auto* ti = resolveExprType(cd->expression());
                            if (ti) return ti;
                        }
                    }
                    return typeRegistry_.lookup("int32");
                }
            }
            return nullptr;
        }

        // Enum variant imported via `use EnumType::*;`
        auto evIt = enumVariantImports_.find(name);
        if (evIt != enumVariantImports_.end())
            return evIt->second.enumType;

        if (moduleRegistry_ && !currentModulePath_.empty()) {
            auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
            if (sym && sym->sourceFile != currentFile_)
                return nullptr;
        }

        if (name == "it" && unwrapCatchItDepth_ == 0) {
            error(expr, "'it' is only available inside 'expr catch { ... }' unwrap blocks");
            return nullptr;
        }

        error(expr, "undefined variable '" + name + "'");
        return nullptr;
    }

    // ── Lambda expression: |params| body ─────────────────────────────
    if (auto* lexpr = dynamic_cast<LucisParser::LambdaExprContext*>(expr)) {
        return resolveLambdaExpr(lexpr);
    }
    if (auto* lblk = dynamic_cast<LucisParser::LambdaBlockExprContext*>(expr)) {
        return resolveLambdaBlockExpr(lblk);
    }

    // ── Parenthesized ────────────────────────────────────────────────
    if (auto* p = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveExprType(p->expression());

    // ── Tuple literal: (expr, expr, ...) ─────────────────────────────
    if (auto* tl = dynamic_cast<LucisParser::TupleLitExprContext*>(expr)) {
        auto elements = tl->expression();
        std::vector<const TypeInfo*> elemTypes;
        std::string fullName = "tuple<";
        for (size_t i = 0; i < elements.size(); i++) {
            auto* et = resolveExprType(elements[i]);
            if (!et) return nullptr;
            elemTypes.push_back(et);
            if (i > 0) fullName += ", ";
            fullName += et->name;
        }
        fullName += ">";

        for (auto& dt : dynamicTypes_) {
            if (dt->name == fullName)
                return dt.get();
        }
        auto ti = std::make_unique<TypeInfo>();
        ti->name = fullName;
        ti->kind = TypeKind::Tuple;
        ti->bitWidth = 0;
        ti->isSigned = false;
        ti->tupleElements = std::move(elemTypes);
        const TypeInfo* raw = ti.get();
        dynamicTypes_.push_back(std::move(ti));
        return raw;
    }

    // ── Tuple index: expr.0, expr.1, ... ─────────────────────────────
    if (auto* ti = dynamic_cast<LucisParser::TupleIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(ti->expression());
        if (!baseType || baseType->kind != TypeKind::Tuple) {
            if (baseType)
                error(expr, "'.N' index requires a tuple type, got '" + baseType->name + "'");
            return nullptr;
        }
        int index = std::stoi(ti->INT_LIT()->getText());
        if (index < 0 || index >= static_cast<int>(baseType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(index) + " is out of range (tuple has " +
                        std::to_string(baseType->tupleElements.size()) + " elements)");
            return nullptr;
        }
        return baseType->tupleElements[index];
    }

    // ── Chained tuple index: expr.N.M (FLOAT_LIT) ──────────────────
    if (auto* cti = dynamic_cast<LucisParser::ChainedTupleIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(cti->expression());
        if (!baseType || baseType->kind != TypeKind::Tuple) {
            if (baseType)
                error(expr, "'.N.M' index requires a tuple type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto text = cti->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        int idx1 = std::stoi(text.substr(0, dotPos));
        int idx2 = std::stoi(text.substr(dotPos + 1));
        if (idx1 < 0 || idx1 >= static_cast<int>(baseType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx1) + " is out of range");
            return nullptr;
        }
        auto* innerType = baseType->tupleElements[idx1];
        if (!innerType || innerType->kind != TypeKind::Tuple) {
            error(expr, "'.N.M' requires element " + std::to_string(idx1) + " to be a tuple");
            return nullptr;
        }
        if (idx2 < 0 || idx2 >= static_cast<int>(innerType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx2) + " is out of range");
            return nullptr;
        }
        return innerType->tupleElements[idx2];
    }

    // ── Tuple arrow index: ptr->0, ptr->1, ... ──────────────────────
    if (auto* tai = dynamic_cast<LucisParser::TupleArrowIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(tai->expression());
        if (!baseType || baseType->kind != TypeKind::Pointer) {
            if (baseType)
                error(expr, "'->N' requires a pointer type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Tuple) {
            if (pointee)
                error(expr, "'->N' requires pointer to tuple, got pointer to '" + pointee->name + "'");
            return nullptr;
        }
        int index = std::stoi(tai->INT_LIT()->getText());
        if (index < 0 || index >= static_cast<int>(pointee->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(index) + " is out of range (tuple has " +
                        std::to_string(pointee->tupleElements.size()) + " elements)");
            return nullptr;
        }
        return pointee->tupleElements[index];
    }

    // ── Chained tuple arrow index: ptr->N.M (FLOAT_LIT) ────────────
    if (auto* ctai = dynamic_cast<LucisParser::ChainedTupleArrowIndexExprContext*>(expr)) {
        auto* baseType = resolveExprType(ctai->expression());
        if (!baseType || baseType->kind != TypeKind::Pointer) {
            if (baseType)
                error(expr, "'->N.M' requires a pointer type, got '" + baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Tuple) {
            if (pointee)
                error(expr, "'->N.M' requires pointer to tuple, got pointer to '" + pointee->name + "'");
            return nullptr;
        }
        auto text = ctai->FLOAT_LIT()->getText();
        auto dotPos = text.find('.');
        int idx1 = std::stoi(text.substr(0, dotPos));
        int idx2 = std::stoi(text.substr(dotPos + 1));
        if (idx1 < 0 || idx1 >= static_cast<int>(pointee->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx1) + " is out of range");
            return nullptr;
        }
        auto* innerType = pointee->tupleElements[idx1];
        if (!innerType || innerType->kind != TypeKind::Tuple) {
            error(expr, "'->N.M' requires element " + std::to_string(idx1) + " to be a tuple");
            return nullptr;
        }
        if (idx2 < 0 || idx2 >= static_cast<int>(innerType->tupleElements.size())) {
            error(expr, "tuple index " + std::to_string(idx2) + " is out of range");
            return nullptr;
        }
        return innerType->tupleElements[idx2];
    }

    // ── Try expression (unwrap — same type as inner) ─────────────────
    if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr)) {
        auto* innerType = resolveExprType(te->expression(0));
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (classifyUnwrapCatchEnum(innerType, pattern, reason))
            innerType = singlePayloadType(*pattern.okVariant);
        if (te->OR() && te->expression().size() >= 2) {
            auto* fallbackType = resolveExprType(te->expression(1));
            if (fallbackType && innerType && !isAssignable(innerType, fallbackType)) {
                error(te, "try-or fallback type '" +
                           (fallbackType ? fallbackType->name : "?") +
                           "' is not compatible with '" +
                           (innerType ? innerType->name : "?") + "'");
                return nullptr;
            }
        }
        return innerType;
    }

    // ── Match expression: match expr { pattern -> body, ... } ─────────
    if (auto* me = dynamic_cast<LucisParser::MatchExprContext*>(expr)) {
        auto* matchedType = resolveExprType(me->expression());
        if (!matchedType) return nullptr;

        // Check if this is a literal match (patterns are literals, not enum variants)
        bool hasLiteralPattern = false;
        bool hasEnumPattern = false;
        for (auto* arm : me->matchArm()) {
            for (auto* pattern : arm->pattern()) {
                if (pattern->literalPattern()) hasLiteralPattern = true;
                if (!pattern->IDENTIFIER().empty()) hasEnumPattern = true;
            }
        }

        // Validate matched type based on pattern types
        if (hasLiteralPattern && !hasEnumPattern) {
            // Literal match: must be int, float, string, bool, or char
            bool validType = matchedType->kind == TypeKind::Integer ||
                            matchedType->kind == TypeKind::Float ||
                            matchedType->kind == TypeKind::String ||
                            matchedType->kind == TypeKind::Bool ||
                            matchedType->kind == TypeKind::Char;
            if (!validType) {
                error(me, "match with literal patterns requires int, float, string, bool, or char, got '" +
                           matchedType->name + "'");
                return nullptr;
            }
        } else if (matchedType->kind != TypeKind::Enum) {
            error(me, "match requires an enum expression, got '" +
                       matchedType->name + "'");
            return nullptr;
        }

        const TypeInfo* resultType = nullptr;
        for (size_t i = 0; i < me->matchArm().size(); i++) {
            auto* arm = me->matchArm(i);

            // Or-patterns: process each sub-pattern, share same body
            bool armHasWildcard = false;
            std::string armBindName;
            for (size_t pi = 0; pi < arm->pattern().size(); pi++) {
            auto* pattern = arm->pattern(pi);

            // Resolve pattern: find matching variant or literal
            const EnumVariantInfo* matchedVariant = nullptr;
            std::string bindName;
            std::string variantName;
            bool isWildcard = pattern->WILDCARD() != nullptr;
            bool isLiteral = pattern->literalPattern() != nullptr;
            if (isWildcard) armHasWildcard = true;

            if (isWildcard) {
                // _ wildcard — matches any remaining variant/value
                // still need to resolve body type below
            } else if (isLiteral) {
                // Literal pattern: validate type compatibility
                auto* lit = pattern->literalPattern();
                const TypeInfo* litType = nullptr;
                if (lit->INT_LIT() || lit->HEX_LIT() || lit->OCT_LIT() || lit->BIN_LIT()) {
                    litType = typeRegistry_.lookup("int32");
                } else if (lit->FLOAT_LIT()) {
                    litType = typeRegistry_.lookup("float64");
                } else if (lit->STR_LIT() || lit->C_STR_LIT()) {
                    litType = typeRegistry_.lookup("string");
                } else if (lit->BOOL_LIT()) {
                    litType = typeRegistry_.lookup("bool");
                } else if (lit->CHAR_LIT()) {
                    litType = typeRegistry_.lookup("char");
                } else if (lit->NULL_LIT()) {
                    litType = typeRegistry_.lookup("null");
                }
                if (litType && !isAssignable(matchedType, litType)) {
                    error(pattern, "literal type '" + litType->name +
                                   "' is not compatible with matched type '" +
                                   matchedType->name + "'");
                    return nullptr;
                }
            } else if (!pattern->IDENTIFIER().empty()) {
                if (pattern->SCOPE() && pattern->IDENTIFIER().size() >= 2) {
                    variantName = pattern->IDENTIFIER(1)->getText();
                } else {
                    variantName = pattern->IDENTIFIER(0)->getText();
                }
            }

            if (!variantName.empty()) {
                // Find variant in enum
                for (auto& v : matchedType->enumVariantInfos) {
                    if (v.name == variantName) {
                        matchedVariant = &v;
                        break;
                    }
                }
                if (!matchedVariant) {
                    error(arm, "enum '" + matchedType->name +
                               "' has no variant '" + variantName + "'");
                    return nullptr;
                }

                // If payload binding: Variant(name)
                if (pattern->LPAREN() && !pattern->IDENTIFIER().empty()) {
                    size_t bindIdx = pattern->SCOPE() ? 2 : 1;
                    if (bindIdx < pattern->IDENTIFIER().size()) {
                        bindName = pattern->IDENTIFIER(bindIdx)->getText();
                    } else if (pattern->WILDCARD()) {
                        bindName = "_";
                    }
                    if (!bindName.empty() && bindName != "_") {
                        if (matchedVariant->payloadFields.empty()) {
                            error(arm, "variant '" + variantName +
                                       "' has no payload to bind");
                            return nullptr;
                        }
                        auto* payloadType = matchedVariant->payloadFields[0].typeInfo;
                        armBindName = bindName;
                        locals_[bindName] = {payloadType, 0, {}, true, true, nullptr};
                    }
                }
            }
            } // end or-pattern for loop

            // Resolve guard condition if present
            if (arm->IF()) {
                auto* guardType = resolveExprType(arm->expression(0));
                if (guardType && guardType->kind != TypeKind::Bool) {
                    error(arm, "match guard must be a boolean expression");
                    return nullptr;
                }
            }

            // Resolve body type — must happen before scope cleanup
            const TypeInfo* bodyType = nullptr;
            if (arm->block()) {
                auto savedLocals = locals_;
                ++scopeDepth_;
                bool terminated = false;
                auto* block = arm->block();
                for (auto* stmt : block->statement()) {
                    if (terminated) break;
                    checkStmt(stmt, currentReturnType_, terminated);
                }
                --scopeDepth_;

                // Resolve type while variables are still in scope
                auto stmts = block->statement();
                if (!stmts.empty()) {
                    auto* lastStmt = stmts.back();
                    if (auto* exprS = lastStmt->exprStmt()) {
                        bodyType = resolveExprType(exprS->expression());
                    } else if (auto* ret = lastStmt->returnStmt()) {
                        if (ret->expression()) {
                            bodyType = resolveExprType(ret->expression());
                        } else {
                            bodyType = typeRegistry_.lookup("void");
                        }
                    } else {
                        bodyType = typeRegistry_.lookup("void");
                    }
                } else {
                    bodyType = typeRegistry_.lookup("void");
                }

                locals_ = savedLocals;
            } else {
                size_t bodyExprIdx = arm->IF() ? 1 : 0;
                bodyType = resolveExprType(arm->expression(bodyExprIdx));
            }

            if (bodyType && bodyType->kind != TypeKind::Void) {
                if (!resultType) {
                    resultType = bodyType;
                } else if (resultType != bodyType && !isAssignable(resultType, bodyType)) {
                    error(arm, "match arm type '" + bodyType->name +
                               "' is not compatible with previous arm type '" +
                               resultType->name + "'");
                    return nullptr;
                }
            }

            // Clean up binding from scope
            if (!armBindName.empty() && armBindName != "_") {
                locals_.erase(armBindName);
            }
        }

        // Exhaustiveness: check all variants are covered (unless wildcard present)
        // Skip exhaustiveness check for literal patterns (they are not exhaustive by nature)
        bool hasWildcard = false;
        for (size_t i = 0; i < me->matchArm().size() && !hasWildcard; i++) {
            for (size_t pi = 0; pi < me->matchArm(i)->pattern().size(); pi++) {
                if (me->matchArm(i)->pattern(pi)->WILDCARD()) {
                    hasWildcard = true; break;
                }
            }
        }
        if (!hasWildcard && !hasLiteralPattern && !matchedType->enumVariantInfos.empty()) {
            // Collect covered variants
            std::unordered_set<std::string> covered;
            for (size_t i = 0; i < me->matchArm().size(); i++) {
                for (size_t pi = 0; pi < me->matchArm(i)->pattern().size(); pi++) {
                auto* p = me->matchArm(i)->pattern(pi);
                if (!p->IDENTIFIER().empty()) {
                    std::string vname;
                    if (p->SCOPE() && p->IDENTIFIER().size() >= 2)
                        vname = p->IDENTIFIER(1)->getText();
                    else
                        vname = p->IDENTIFIER(0)->getText();
                    if (!vname.empty()) covered.insert(vname);
                }
            }
            } // inner or-pattern loop
            // Find missing variants
            std::string missing;
            for (auto& v : matchedType->enumVariantInfos) {
                if (!covered.count(v.name)) {
                    if (!missing.empty()) missing += ", ";
                    missing += v.name;
                }
            }
            if (!missing.empty()) {
                error(me, "non-exhaustive match: missing variants " + missing);
                return nullptr;
            }
        }

        return resultType ? resultType : typeRegistry_.lookup("void");
    }

    // ── Propagate operator: expr? — unwrap Result or return error ─────
    if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(expr)) {
        auto* sourceType = resolveExprType(pe->expression());
        if (!sourceType) return nullptr;

        // Phase 5: optional context block adds message on error re-propagation
        // Validate that the block (if present) is well-formed
        if (auto* ctxBlock = pe->block()) {
            // The block should contain at least a string expression as context
            // Full validation is done in IRGen; here we just ensure it's not empty
            if (ctxBlock->statement().empty()) {
                error(pe, "? context block must contain a context message");
                return nullptr;
            }
        }

        // Phase 3: void functions skip classifyUnwrapCatchEnum (allow unit variants)
        bool isVoidReturn = currentReturnType_ &&
                            currentReturnType_->kind == TypeKind::Void;

        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (isVoidReturn) {
            if (sourceType->kind != TypeKind::Enum || sourceType->enumVariantInfos.empty()) {
                error(pe, "cannot use '?': expression must be an enum type");
                return nullptr;
            }
            for (const auto& v : sourceType->enumVariantInfos) {
                bool isErr = v.isError ||
                             v.name == "Err" || v.name == "Error" ||
                             v.name == "Failure" || v.name == "Fail" ||
                             v.name == "None";
                if (!isErr && v.payloadFields.size() == 1 &&
                    v.payloadFields[0].typeInfo &&
                    v.payloadFields[0].typeInfo->name == "Error")
                    isErr = true;
                if (isErr) { pattern.errVariant = &v; break; }
            }
            if (!pattern.errVariant) {
                error(pe, "cannot use '?' in void function: enum '" +
                           sourceType->name + "' has no error variant");
                return nullptr;
            }
            return typeRegistry_.lookup("void");
        }

        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason)) {
            error(pe, reason);
            return nullptr;
        }
        // '?' requires the function return type to match or be compatible
        if (currentReturnType_ && currentReturnType_ != sourceType) {
                // Check if enums are compatible (same error payload, assignable success)
                bool compatible = false;
                if (currentReturnType_->kind == TypeKind::Enum &&
                    sourceType->kind == TypeKind::Enum) {
                    UnwrapCatchPatternInfo retPattern;
                    std::string retReason;
                    if (classifyUnwrapCatchEnum(currentReturnType_, retPattern, retReason)) {
                        auto* srcErr = singlePayloadType(*pattern.errVariant);
                        auto* retErr = singlePayloadType(*retPattern.errVariant);
                        if (srcErr && retErr && srcErr == retErr) {
                            compatible = true;
                        }
                    }
                }
                if (!compatible) {
                    error(pe, "cannot use '?': function return type '" +
                               currentReturnType_->name + "' is not compatible with '" +
                               sourceType->name + "'");
                    return nullptr;
                }
        }
        return singlePayloadType(*pattern.okVariant);
    }

    // ── Catch unwrap expression: expr catch { ... } ───────────────────
    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(expr)) {
        auto* sourceType = resolveExprType(cu->expression());
        if (!sourceType) return nullptr;

        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason)) {
            error(cu, reason);
            return nullptr;
        }

        auto* errPayloadTI = pattern.errVariant
            ? singlePayloadType(*pattern.errVariant) : nullptr;
        if (!errPayloadTI) errPayloadTI = typeRegistry_.lookup("Error");
        auto saved = locals_.find("it");
        bool hadSaved = saved != locals_.end();
        VarInfo savedInfo;
        if (hadSaved) savedInfo = saved->second;

        locals_["it"] = {errPayloadTI, 0, {}, true, true, nullptr};

        ++unwrapCatchItDepth_;
        auto* retCtx = currentReturnType_ ? currentReturnType_ : typeRegistry_.lookup("void");
        checkBlock(cu->block(), retCtx);
        --unwrapCatchItDepth_;

        if (hadSaved) locals_["it"] = savedInfo;
        else locals_.erase("it");

        return singlePayloadType(*pattern.okVariant);
    }

    // ── Spawn expression: spawn f() → Task<T> where f() returns T ───
    if (auto* se = dynamic_cast<LucisParser::SpawnExprContext*>(expr)) {
        auto* innerType = resolveExprType(se->expression());
        if (innerType) {
            auto fullName = "Task<" + innerType->name + ">";
            auto* cached = typeRegistry_.lookup(fullName);
            if (cached) return cached;
            auto ti = std::make_unique<TypeInfo>();
            ti->name = fullName;
            ti->kind = TypeKind::Extended;
            ti->bitWidth = 0;
            ti->isSigned = false;
            ti->elementType = innerType;
            ti->extendedKind = "Task";
            const TypeInfo* raw = ti.get();
            dynamicTypes_.push_back(std::move(ti));
            return raw;
        }
        error(expr, "cannot resolve type of spawned expression");
        return typeRegistry_.lookup("void");
    }

    // ── Await expression: await task → T where task is Task<T> ──────
    if (auto* ae = dynamic_cast<LucisParser::AwaitExprContext*>(expr)) {
        auto* taskType = resolveExprType(ae->expression());
        if (taskType && taskType->kind == TypeKind::Extended &&
            taskType->extendedKind == "Task" && taskType->elementType)
            return taskType->elementType;
        if (!taskType) {
            error(expr, "cannot resolve type of awaited expression");
            return typeRegistry_.lookup("void");
        }
        return taskType;
    }

    // ── Unary negation ───────────────────────────────────────────────
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto* operand = resolveExprType(neg->expression());
        if (operand && !isNumeric(operand))
            error(expr, "unary '-' requires numeric operand, got '" +
                             operand->name + "'");
        if (operand && operand->kind == TypeKind::Integer && !operand->isSigned)
            error(expr, "unary '-' cannot be applied to unsigned type '" +
                             operand->name + "'");
        return operand;
    }

    // ── Logical NOT ──────────────────────────────────────────────────
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        resolveExprType(lnot->expression());
        return typeRegistry_.lookup("bool");
    }

    // ── Bitwise NOT ──────────────────────────────────────────────────
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr)) {
        auto* operand = resolveExprType(bnot->expression());
        if (operand && !isIntegerOrPointer(operand))
            error(expr, "operator '~' requires integer operand, got '" +
                             operand->name + "'");
        return operand;
    }

    // ── Multiplicative: *, /, % ──────────────────────────────────────
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        auto exprs = mul->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = mul->op->getText();

        if (opText == "%") {
            if (lhs && !isIntegerOrPointer(lhs))
                error(expr, "operator '%' requires integer operands, got '" +
                                 lhs->name + "'");
            if (rhs && !isIntegerOrPointer(rhs))
                error(expr, "operator '%' requires integer operands, got '" +
                                 rhs->name + "'");
        } else {
            bool lhsBad = lhs && !isNumeric(lhs);
            bool rhsBad = rhs && !isNumeric(rhs);
            if (lhsBad || rhsBad) {
                auto& t = lhsBad ? lhs : rhs;
                error(expr, "operator '" + opText +
                                 "' requires numeric operands, got '" + t->name + "'");
            }

            if (!lhsBad && !rhsBad && lhs && rhs) {
                bool kindMismatch = lhs->kind != rhs->kind;
                bool signMismatch = lhs->kind == TypeKind::Integer &&
                                    rhs->kind == TypeKind::Integer &&
                                    lhs->isSigned != rhs->isSigned &&
                                    !isIntLitExpr(exprs[0]) &&
                                    !isIntLitExpr(exprs[1]);
                if (kindMismatch || signMismatch) {
                    error(expr, "operator '" + opText +
                                     "' does not allow mixed numeric kinds ('" +
                                     lhs->name + "' and '" + rhs->name +
                                     "'); cast explicitly");
                }
            }
        }

        // Compile-time division by zero check
        if (opText == "/" || opText == "%") {
            if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(exprs[1])) {
                if (intLit->INT_LIT()->getText() == "0")
                    error(expr, "division by zero");
            }
        }

        if (lhs && rhs && lhs->kind == rhs->kind) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    }

    // ── Additive: +, - ──────────────────────────────────────────────
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        auto exprs = add->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = add->op->getText();

        bool lhsPtr = lhs && lhs->kind == TypeKind::Pointer;
        bool rhsPtr = rhs && rhs->kind == TypeKind::Pointer;
        bool lhsNum = lhs && isNumeric(lhs);
        bool rhsNum = rhs && isNumeric(rhs);

        // pointer + int  or  int + pointer  → pointer
        if ((lhsPtr && rhsNum) || (lhsNum && rhsPtr)) {
            return lhsPtr ? lhs : rhs;
        }
        // pointer - pointer → i64 (ptrdiff)
        if (lhsPtr && rhsPtr && opText == "-") {
            return typeRegistry_.lookup("i64");
        }

        // string + string → string
        if (lhs && rhs && lhs->kind == TypeKind::String && rhs->kind == TypeKind::String) {
            if (opText != "+") {
                error(expr, "operator '-' does not support string operands");
                return lhs;
            }
            return typeRegistry_.lookup("string");
        }

        bool lhsBad = lhs && !isNumeric(lhs);
        bool rhsBad = rhs && !isNumeric(rhs);
        if (lhsBad || rhsBad) {
            auto& t = lhsBad ? lhs : rhs;
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + t->name + "'");
        }

        if (!lhsBad && !rhsBad && lhs && rhs) {
            bool kindMismatch = lhs->kind != rhs->kind;
            bool signMismatch = lhs->kind == TypeKind::Integer &&
                                rhs->kind == TypeKind::Integer &&
                                lhs->isSigned != rhs->isSigned &&
                                !isIntLitExpr(exprs[0]) &&
                                !isIntLitExpr(exprs[1]);
            if (kindMismatch || signMismatch) {
                error(expr, "operator '" + opText +
                                 "' does not allow mixed numeric kinds ('" +
                                 lhs->name + "' and '" + rhs->name +
                                 "'); cast explicitly");
            }
        }

        if (lhs && rhs && lhs->kind == rhs->kind) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    }

    // ── Shift: <<, >> ───────────────────────────────────────────────
    auto checkShift = [&](auto* shift, const std::string& opText) -> const TypeInfo* {
        auto exprs = shift->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        if (lhs && !isIntegerOrPointer(lhs))
            error(expr, "operator '" + opText +
                             "' requires integer operands, got '" + lhs->name + "'");
        if (rhs && !isIntegerOrPointer(rhs))
            error(expr, "operator '" + opText +
                             "' requires integer operands, got '" + rhs->name + "'");
        return lhs;
    };
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr))
        return checkShift(lsh, "<<");
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr))
        return checkShift(rsh, ">>");

    // ── Relational: <, >, <=, >= ────────────────────────────────────
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto exprs = rel->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = rel->op->getText();

        if (lhs && !isNumeric(lhs))
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + lhs->name + "'");
        else if (rhs && !isNumeric(rhs))
            error(expr, "operator '" + opText +
                             "' requires numeric operands, got '" + rhs->name + "'");

        if (lhs && rhs && isNumeric(lhs) && isNumeric(rhs)) {
            bool kindMismatch = lhs->kind != rhs->kind;
            bool signMismatch = lhs->kind == TypeKind::Integer &&
                                rhs->kind == TypeKind::Integer &&
                                lhs->isSigned != rhs->isSigned &&
                                !isIntLitExpr(exprs[0]) &&
                                !isIntLitExpr(exprs[1]);
            if (kindMismatch || signMismatch) {
                error(expr, "operator '" + opText +
                                 "' does not allow mixed numeric kinds ('" +
                                 lhs->name + "' and '" + rhs->name +
                                 "'); cast explicitly");
            }
        }
        return typeRegistry_.lookup("bool");
    }

    // ── Equality: ==, != ────────────────────────────────────────────
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto exprs = eq->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        auto opText = eq->op->getText();

        // Keep equality strict for numeric operands to prevent IR-time mismatches
        // such as float-vs-int comparisons reaching LLVM FCmp/ICmp with mixed types.
        if (lhs && rhs && isNumeric(lhs) && isNumeric(rhs) && lhs->kind != rhs->kind) {
            error(expr, "operator '" + opText +
                             "' does not allow mixed numeric kinds ('" +
                             lhs->name + "' and '" + rhs->name +
                             "'); cast explicitly");
        } else if (lhs && rhs && lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer) {
            // Integer ↔ integer comparison always allowed (sign/width mismatches handled in IR)
        } else if (lhs && rhs &&
                   !(isAssignable(lhs, rhs) || isAssignable(rhs, lhs))) {
            error(expr, "operator '" + opText +
                             "' has incompatible operand types '" +
                             lhs->name + "' and '" + rhs->name + "'");
        }

        return typeRegistry_.lookup("bool");
    }

    // ── Bitwise AND/XOR/OR ──────────────────────────────────────────
    auto checkBitwise = [&](auto* ctx, const std::string& opText) -> const TypeInfo* {
        auto exprs = ctx->expression();
        auto* lhs = resolveExprType(exprs[0]);
        auto* rhs = resolveExprType(exprs[1]);
        if (lhs && !isIntegerOrPointer(lhs))
            error(expr, "operator '" + opText + "' requires integer operands, got '" +
                             lhs->name + "'");
        if (rhs && !isIntegerOrPointer(rhs))
            error(expr, "operator '" + opText + "' requires integer operands, got '" +
                             rhs->name + "'");
        if (lhs && rhs && lhs->kind == TypeKind::Integer && rhs->kind == TypeKind::Integer &&
            lhs->isSigned != rhs->isSigned &&
            !isIntLitExpr(exprs[0]) && !isIntLitExpr(exprs[1])) {
            error(expr, "operator '" + opText +
                             "' does not allow mixed signed/unsigned integers ('" +
                             lhs->name + "' and '" + rhs->name + "'); cast explicitly");
        }
        if (lhs && rhs) {
            unsigned lhsBW = lhs->bitWidth == 0 ? 64 : lhs->bitWidth;
            unsigned rhsBW = rhs->bitWidth == 0 ? 64 : rhs->bitWidth;
            return lhsBW >= rhsBW ? lhs : rhs;
        }
        return lhs ? lhs : rhs;
    };
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr))
        return checkBitwise(ba, "&");
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr))
        return checkBitwise(bx, "^");
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr))
        return checkBitwise(bo, "|");

    // ── Logical AND/OR ──────────────────────────────────────────────
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        auto exprs = la->expression();
        resolveExprType(exprs[0]);
        resolveExprType(exprs[1]);
        return typeRegistry_.lookup("bool");
    }

    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        auto exprs = lo->expression();
        resolveExprType(exprs[0]);
        resolveExprType(exprs[1]);
        return typeRegistry_.lookup("bool");
    }

    // ── Helper: check if an expression is an lvalue ─────────────────
    auto isLValue = [](LucisParser::ExpressionContext* e) -> bool {
        return dynamic_cast<LucisParser::IdentExprContext*>(e)
            || dynamic_cast<LucisParser::FieldAccessExprContext*>(e)
            || dynamic_cast<LucisParser::DerefExprContext*>(e)
            || dynamic_cast<LucisParser::IndexExprContext*>(e);
    };

    // ── Pre-increment/decrement ─────────────────────────────────────
    if (auto* pi = dynamic_cast<LucisParser::PreIncrExprContext*>(expr)) {
        auto* inner = pi->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '++' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '++' requires a variable (lvalue)");
        return type;
    }

    if (auto* pd = dynamic_cast<LucisParser::PreDecrExprContext*>(expr)) {
        auto* inner = pd->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '--' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '--' requires a variable (lvalue)");
        return type;
    }

    // ── Post-increment/decrement ────────────────────────────────────
    if (auto* pi = dynamic_cast<LucisParser::PostIncrExprContext*>(expr)) {
        auto* inner = pi->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '++' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '++' requires a variable (lvalue)");
        return type;
    }

    if (auto* pd = dynamic_cast<LucisParser::PostDecrExprContext*>(expr)) {
        auto* inner = pd->expression();
        auto* type = resolveExprType(inner);
        if (type && !isInteger(type) && type->kind != TypeKind::Pointer)
            error(expr, "operator '--' requires integer or pointer operand, got '" +
                             type->name + "'");
        if (!isLValue(inner))
            error(expr, "operator '--' requires a variable (lvalue)");
        return type;
    }

    // ── Ternary: cond ? trueExpr : falseExpr ────────────────────────
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto exprs = tern->expression();
        auto* condType = resolveExprType(exprs[0]); // condition
        if (condType && !isConditionType(condType))
            error(expr, "ternary condition has type '" + condType->name +
                         "', expected 'bool' or numeric type");
        auto* trueType = resolveExprType(exprs[1]);
        auto* falseType = resolveExprType(exprs[2]);
        if (trueType && falseType && !isAssignable(trueType, falseType))
            error(expr, "ternary branches have incompatible types: '" +
                             trueType->name + "' and '" + falseType->name + "'");
        return trueType ? trueType : falseType;
    }

    // ── Is: expr is type [::Variant] ────────────────────────────────
    if (auto* isE = dynamic_cast<LucisParser::IsExprContext*>(expr)) {
        auto* lhsType = resolveExprType(isE->expression());
        unsigned dims = 0;
        auto* rhsType = resolveTypeSpecInContext(isE->typeSpec(), dims);

        // Variant identity check: value is EnumType::Variant
        if (isE->SCOPE()) {
            if (isE->IDENTIFIER().empty()) {
                error(expr, "invalid enum variant check in 'is' expression");
                return typeRegistry_.lookup("bool");
            }
            auto* variantNode = isE->IDENTIFIER(0);
            if (!variantNode) {
                error(expr, "invalid enum variant check in 'is' expression");
                return typeRegistry_.lookup("bool");
            }

            auto variantName = variantNode->getText();
            if (!rhsType || rhsType->kind != TypeKind::Enum) {
                error(expr, "right side of variant 'is' check must be an enum type");
                return typeRegistry_.lookup("bool");
            }
            if (!lhsType || lhsType->kind != TypeKind::Enum) {
                error(expr, "left side of variant 'is' check must be an enum value");
                return typeRegistry_.lookup("bool");
            }
            if (lhsType != rhsType) {
                error(expr, "enum type mismatch in variant 'is' check: left is '" +
                             lhsType->name + "', right is '" + rhsType->name + "'");
                return typeRegistry_.lookup("bool");
            }

            auto* variantInfo = findEnumVariantInfo(rhsType, variantName);
            if (!variantInfo) {
                error(expr, "enum '" + rhsType->name + "' has no variant '" +
                             variantName + "'");
            }

            // Binding: value is EnumType::Variant(name) — validate the binding
            if (isE->LPAREN() && isE->IDENTIFIER().size() > 1) {
                auto* bindNode = isE->IDENTIFIER(1);
                if (!bindNode) {
                    error(expr, "invalid binding in variant 'is' check");
                } else if (!variantInfo || variantInfo->payloadFields.empty()) {
                    error(expr, "variant '" + variantName +
                                "' has no payload to bind");
                }
            } else if (isE->LPAREN()) {
                if (!variantInfo || variantInfo->payloadFields.empty()) {
                    error(expr, "variant '" + variantName +
                                "' has no payload to bind");
                }
                // Binding type resolution is handled in checkIfStmt.
            }

            return typeRegistry_.lookup("bool");
        }

        return typeRegistry_.lookup("bool");
    }

    // ── Null coalescing: expr ?? default ────────────────────────────
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr)) {
        auto exprs = nc->expression();
        auto* lhsType = resolveExprType(exprs[0]);
        auto* rhsType = resolveExprType(exprs[1]);
        if (lhsType && lhsType->kind != TypeKind::Pointer)
            error(expr, "left side of '\?\?' must be a pointer type, got '" +
                             lhsType->name + "'");
        if (lhsType && rhsType && !isAssignable(lhsType, rhsType))
            error(expr, "'\?\?' type mismatch: left is '" +
                             lhsType->name + "', default is '" +
                             rhsType->name + "'");
        return lhsType ? lhsType : rhsType;
    }

    // ── Arrow method call: expr->method(args) ───────────────────────
    if (auto* amc = dynamic_cast<LucisParser::ArrowMethodCallExprContext*>(expr)) {
        auto* baseType = resolveExprType(amc->expression());
        auto methodName = amc->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = amc->argList()) {
            for (auto* argExpr : argList->expression())
                argTypes.push_back(resolveExprType(argExpr));
        }

        if (!baseType) return nullptr;

        if (baseType->kind != TypeKind::Pointer) {
            error(expr, "'->' requires a pointer type, got '" +
                             baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType->pointeeType;
        if (!pointee || pointee->kind != TypeKind::Struct) {
            error(expr, "'->' method call requires pointer to struct");
            return nullptr;
        }

        auto* sm = findMethodInChain(pointee, methodName);
        if (sm && !sm->isStatic) {
            if (argTypes.size() != sm->paramTypes.size()) {
                error(expr, "method '" + methodName + "' expects " +
                    std::to_string(sm->paramTypes.size()) +
                    " arguments, got " + std::to_string(argTypes.size()));
            }
            for (size_t i = 0; i < argTypes.size() && i < sm->paramTypes.size(); i++) {
                if (!argTypes[i] || !sm->paramTypes[i]) continue;
                if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                    error(expr, "method '" + methodName + "' argument " +
                        std::to_string(i + 1) + " type mismatch: expected '" +
                        sm->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            return sm->returnType;
        }
        error(expr, "struct '" + pointee->name +
                    "' has no method '" + methodName + "'");
        return nullptr;
    }

    // ── Arrow access: expr->field ───────────────────────────────────
    if (auto* aa = dynamic_cast<LucisParser::ArrowAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(aa->expression());
        auto fieldName = aa->IDENTIFIER()->getText();

        if (baseType && baseType->kind != TypeKind::Pointer) {
            error(expr, "'->' requires a pointer type, got '" +
                             baseType->name + "'");
            return nullptr;
        }
        auto* pointee = baseType ? baseType->pointeeType : nullptr;
        if (pointee && pointee->kind != TypeKind::Struct) {
            error(expr, "'->' requires pointer to struct, got pointer to '" +
                             pointee->name + "'");
            return nullptr;
        }
        if (pointee) {
            for (auto& field : pointee->fields) {
                if (field.name == fieldName)
                    return field.typeInfo;
            }
            error(expr, "struct '" + pointee->name +
                             "' has no field '" + fieldName + "'");
        }
        return nullptr;
    }

    // ── Range: expr..expr ───────────────────────────────────────────
    if (auto* rng = dynamic_cast<LucisParser::RangeExprContext*>(expr)) {
        auto exprs = rng->expression();
        auto* startType = resolveExprType(exprs[0]);
        auto* endType   = resolveExprType(exprs[1]);
        if (startType && !isInteger(startType))
            error(expr, "range start must be integer, got '" +
                             startType->name + "'");
        if (endType && !isInteger(endType))
            error(expr, "range end must be integer, got '" +
                             endType->name + "'");
        return startType;
    }

    // ── Range inclusive: expr..=expr ─────────────────────────────────
    if (auto* rng = dynamic_cast<LucisParser::RangeInclExprContext*>(expr)) {
        auto exprs = rng->expression();
        auto* startType = resolveExprType(exprs[0]);
        auto* endType   = resolveExprType(exprs[1]);
        if (startType && !isInteger(startType))
            error(expr, "range start must be integer, got '" +
                             startType->name + "'");
        if (endType && !isInteger(endType))
            error(expr, "range end must be integer, got '" +
                             endType->name + "'");
        return startType;
    }

    // ── Spread: ...expr ─────────────────────────────────────────────
    if (auto* sp = dynamic_cast<LucisParser::SpreadExprContext*>(expr)) {
        return resolveExprType(sp->expression());
    }

    // ── Cast: expr as type ──────────────────────────────────────────
    if (auto* cast = dynamic_cast<LucisParser::CastExprContext*>(expr)) {
        auto* srcType = resolveExprType(cast->expression());
        unsigned dims = 0;
        auto* dstType = resolveTypeSpecInContext(cast->typeSpec(), dims);

        if (srcType && dstType && srcType != dstType) {
            auto sk = srcType->kind;
            auto dk = dstType->kind;

            // string → pointer (e.g. string as cstring / string as *char)
            if (sk == TypeKind::String && dk == TypeKind::Pointer) {
                if (dstType->pointeeType &&
                    dstType->pointeeType->kind == TypeKind::Char) {
                    error(expr,
                        "cannot cast 'string' to 'cstring' directly. "
                        "Use 'cstr(expr)' to convert a string to a "
                        "null-terminated C string");
                } else {
                    error(expr,
                        "cannot cast 'string' to '" + dstType->name +
                        "': incompatible types");
                }
            }
            // pointer → string
            else if (sk == TypeKind::Pointer && dk == TypeKind::String) {
                if (srcType->pointeeType &&
                    srcType->pointeeType->kind == TypeKind::Char) {
                    error(expr,
                        "cannot cast 'cstring' to 'string' directly. "
                        "Use 'fromCStr(expr)' to convert a C string to "
                        "a TM string");
                } else {
                    error(expr,
                        "cannot cast '" + srcType->name +
                        "' to 'string': incompatible types");
                }
            }
            // char → string (not supported, use .toString())
            else if (sk == TypeKind::Char && dk == TypeKind::String) {
                error(expr,
                    "cannot cast 'char' to 'string'. "
                    "Use char.toString() to convert a character to a string");
            }
            // struct/extended/string → numeric or vice-versa
            else if ((sk == TypeKind::Struct || sk == TypeKind::Extended ||
                      sk == TypeKind::String) &&
                     (dk == TypeKind::Integer || dk == TypeKind::Float ||
                      dk == TypeKind::Bool)) {
                error(expr,
                    "cannot cast '" + srcType->name + "' to '" +
                    dstType->name + "': incompatible types");
            }
            else if ((sk == TypeKind::Integer || sk == TypeKind::Float ||
                      sk == TypeKind::Bool) &&
                     (dk == TypeKind::Struct || dk == TypeKind::Extended ||
                      dk == TypeKind::String)) {
                error(expr,
                    "cannot cast '" + srcType->name + "' to '" +
                    dstType->name + "': incompatible types");
            }
        }

        return dstType;
    }

    // ── Sizeof: sizeof(type) ────────────────────────────────────────
    if (auto* sz = dynamic_cast<LucisParser::SizeofExprContext*>(expr)) {
        // sizeof requires concrete type sizes; unsized [] dimensions are not valid.
        auto* spec = sz->typeSpec();
        while (spec && spec->LBRACKET()) {
            if (!spec->INT_LIT()) {
                error(expr, "sizeof: unsized array type is not allowed; use fixed-size '[N]T'");
                return typeRegistry_.lookup("usize");
            }
            if (spec->typeSpec().empty()) break;
            spec = spec->typeSpec(0);
        }

        unsigned dims = 0;
        auto* ti = resolveTypeSpecInContext(sz->typeSpec(), dims);
        if (!ti) error(expr, "sizeof: unknown type");
        return typeRegistry_.lookup("usize");
    }

    // ── Typeof: typeof(expr) ────────────────────────────────────────
    if (auto* to = dynamic_cast<LucisParser::TypeofExprContext*>(expr)) {
        resolveExprType(to->expression());
        return typeRegistry_.lookup("string");
    }

    // ── Alignof: alignof(type) ────────────────────────────────────────
    if (auto* al = dynamic_cast<LucisParser::AlignofExprContext*>(expr)) {
        unsigned dims = 0;
        auto* ti = resolveTypeSpec(al->typeSpec(), dims);
        if (!ti) {
            error(al, "alignof: unknown type");
        }
        return typeRegistry_.lookup("int64");
    }

    // ── Offsetof: offsetof(type, field) ───────────────────────────────
    if (auto* of = dynamic_cast<LucisParser::OffsetofExprContext*>(expr)) {
        unsigned dims = 0;
        auto* ti = resolveTypeSpec(of->typeSpec(), dims);
        if (!ti) {
            error(of, "offsetof: unknown type");
            return typeRegistry_.lookup("int64");
        }
        if (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union) {
            error(of, "offsetof: expected a struct or union type, got  + ti->name + ");
            return typeRegistry_.lookup("int64");
        }
        std::string fieldName = of->IDENTIFIER()->getText();
        bool found = false;
        for (const auto& field : ti->fields) {
            if (field.name == fieldName) {
                found = true;
                break;
            }
        }
        if (!found) {
            error(of, "offsetof: type  + ti->name +  has no field named  + fieldName + ");
        }
        return typeRegistry_.lookup("int64");
    }

    // ── Method call: expr.method(args) ─────────────────────────────
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        auto* receiverType = resolveExprType(mc->expression());
        auto methodName = mc->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = mc->argList()) {
            for (auto* argExpr : argList->expression()) {
                argTypes.push_back(resolveExprType(argExpr));
            }
        }

        if (!receiverType) return nullptr;

        // Auto-dereference: ptr.method() → pointee.method()
        if (receiverType->kind == TypeKind::Pointer && receiverType->pointeeType)
            receiverType = receiverType->pointeeType;

        unsigned recvArrayDims = resolveExprArrayDims(mc->expression());

        const MethodDescriptor* desc = nullptr;

        if (recvArrayDims > 0) {
            // Array method
            desc = methodRegistry_.lookupArrayMethod(methodName);
            if (!desc) {
                error(expr, "array type '[]" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            // Validate numeric-only constraint
            if (desc->requireNumeric && !isNumeric(receiverType)) {
                error(expr, "method '" + methodName +
                            "' requires numeric element type, got '" +
                            receiverType->name + "'");
                return nullptr;
            }
        } else if (receiverType->kind == TypeKind::Struct) {
            // Struct field function call: obj.callback(args)
            for (auto& field : receiverType->fields) {
                if (field.name == methodName && field.typeInfo) {
                    auto* fnTI = field.typeInfo;
                    if (fnTI->kind == TypeKind::Pointer && fnTI->pointeeType &&
                        fnTI->pointeeType->kind == TypeKind::Function)
                        fnTI = fnTI->pointeeType;
                    if (fnTI->kind == TypeKind::Function) {
                        if (argTypes.size() != fnTI->paramTypes.size()) {
                            error(expr, "function '" + methodName + "' expects " +
                                std::to_string(fnTI->paramTypes.size()) +
                                " arguments " + formatParamTypes(fnTI->paramTypes) +
                                ", got " + std::to_string(argTypes.size()));
                        }
                        return fnTI->returnType;
                    }
                }
            }
            // Struct method via `extend` block (walks parent chain)
            auto* sm = findMethodInChain(receiverType, methodName);
            if (sm && !sm->isStatic) {
                if (argTypes.size() != sm->paramTypes.size()) {
                    error(expr, "method '" + methodName + "' expects " +
                        std::to_string(sm->paramTypes.size()) +
                        " arguments " + formatParamTypes(sm->paramTypes) +
                        ", got " + std::to_string(argTypes.size()));
                }
                for (size_t i = 0; i < argTypes.size() && i < sm->paramTypes.size(); i++) {
                    if (!argTypes[i] || !sm->paramTypes[i]) continue;
                    if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                        error(expr, "method '" + methodName + "' argument " +
                            std::to_string(i + 1) + " type mismatch: expected '" +
                            sm->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
                return sm->returnType;
            }
            error(expr, "struct '" + receiverType->name +
                        "' has no method '" + methodName + "'");
            return nullptr;
        } else if (receiverType->kind == TypeKind::Extended) {
            // Extended type method (e.g. vec.push, vec.pop)
            auto* extDesc = extTypeRegistry_.lookup(receiverType->extendedKind);
            if (!extDesc) {
                error(expr, "unknown extended type '" + receiverType->extendedKind + "'");
                return nullptr;
            }
            for (auto& md : extDesc->methods) {
                if (md.name == methodName) {
                    desc = &md;
                    break;
                }
            }
            if (!desc) {
                error(expr, "type '" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            if (desc->requireNumeric) {
                auto* elemTI = receiverType->elementType;
                if (elemTI && !isNumeric(elemTI)) {
                    error(expr, "method '" + methodName +
                                "' requires numeric element type, got '" +
                                elemTI->name + "'");
                    return nullptr;
                }
            }
        } else {
            // Built-in type method
            desc = methodRegistry_.lookup(receiverType->kind, methodName);
            if (!desc) {
                error(expr, "type '" + receiverType->name +
                            "' has no method '" + methodName + "'");
                return nullptr;
            }
            // Validate signed/unsigned constraints
            if (desc->requireSigned && receiverType->kind == TypeKind::Integer &&
                !receiverType->isSigned) {
                error(expr, "method '" + methodName +
                            "' requires a signed integer type");
                return nullptr;
            }
            if (desc->requireUnsigned && receiverType->kind == TypeKind::Integer &&
                receiverType->isSigned) {
                error(expr, "method '" + methodName +
                            "' requires an unsigned integer type");
                return nullptr;
            }
        }

        // Validate argument count
        if (argTypes.size() != desc->paramTypes.size()) {
            error(expr, "method '" + methodName + "' expects " +
                std::to_string(desc->paramTypes.size()) +
                " arguments " + formatParamTypes(desc->paramTypes) +
                ", got " + std::to_string(argTypes.size()));
            return nullptr;
        }

        // Validate argument types
        for (size_t i = 0; i < argTypes.size(); i++) {
            if (!argTypes[i]) continue;
            const TypeInfo* expectedParam = nullptr;
            if (desc->paramTypes[i] == "_self")
                expectedParam = receiverType;
            else if (desc->paramTypes[i] == "_elem") {
                if (receiverType->kind == TypeKind::Extended && receiverType->elementType)
                    expectedParam = receiverType->elementType;
                else
                    expectedParam = receiverType;
            } else if (desc->paramTypes[i] == "_key") {
                if (receiverType->kind == TypeKind::Extended && receiverType->keyType)
                    expectedParam = receiverType->keyType;
                else {
                    error(expr, "cannot resolve map key type for method '" + methodName + "'");
                    continue;
                }
            } else if (desc->paramTypes[i] == "_val") {
                if (receiverType->kind == TypeKind::Extended && receiverType->valueType)
                    expectedParam = receiverType->valueType;
                else {
                    error(expr, "cannot resolve map value type for method '" + methodName + "'");
                    continue;
                }
            } else
                expectedParam = typeRegistry_.lookup(desc->paramTypes[i]);

            if (expectedParam && !isAssignable(expectedParam, argTypes[i])) {
                error(expr, "method '" + methodName + "' argument " +
                    std::to_string(i + 1) + " type mismatch: expected '" +
                    expectedParam->name + "', got '" + argTypes[i]->name + "'");
            }
        }

        // Resolve return type
        if (desc->returnType == "_self")
            return receiverType;
        if (desc->returnType == "_elem") {
            if (receiverType->kind == TypeKind::Extended && receiverType->elementType)
                return receiverType->elementType;
            return receiverType;
        }
        if (desc->returnType == "_key") {
            if (receiverType->kind == TypeKind::Extended && receiverType->keyType)
                return receiverType->keyType;
            return receiverType;
        }
        if (desc->returnType == "_val") {
            if (receiverType->kind == TypeKind::Extended && receiverType->valueType)
                return receiverType->valueType;
            return receiverType;
        }
        // _vec_key → Vec<keyType>, _vec_val → Vec<valType>
        if (desc->returnType == "_vec_key" || desc->returnType == "_vec_val") {
            const TypeInfo* innerType = nullptr;
            if (desc->returnType == "_vec_key" && receiverType->keyType)
                innerType = receiverType->keyType;
            else if (desc->returnType == "_vec_val" && receiverType->valueType)
                innerType = receiverType->valueType;
            if (innerType) {
                auto fullName = "Vec<" + innerType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) return dt.get();
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = innerType->builtinSuffix;
                ti->elementType = innerType;
                ti->extendedKind = "Vec";
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                return raw;
            }
        }
        return typeRegistry_.lookup(desc->returnType);
    }

    // ── Function call: expr(args) ───────────────────────────────────
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        auto* callee = call->expression();

        // Detect callee name for polymorphic builtin detection
        std::string calleeName;
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(callee))
            calleeName = ident->IDENTIFIER()->getText();

        std::vector<const TypeInfo*> argTypes;
        std::vector<LucisParser::ExpressionContext*> argExprs;
        if (auto* argList = call->argList()) {
            for (auto* argExpr : argList->expression()) {
                argExprs.push_back(argExpr);
                argTypes.push_back(resolveExprType(argExpr));
            }
        }

        // A value argument cannot have type void.
        for (size_t i = 0; i < argTypes.size(); i++) {
            auto* ti = argTypes[i];
            if (ti && ti->kind == TypeKind::Void) {
                error(argExprs[i],
                      "argument " + std::to_string(i + 1) +
                      " has type 'void'; functions returning void cannot be used as values");
                return nullptr;
            }
        }

        // Lucis std::log::sprintf expects Lucis `string` values, not `*char`.
        // Guard expression calls during semantic checking.
        if (calleeName == "sprintf" && imports_.isImported("sprintf")) {
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* ti = argTypes[i];
                if (!ti) continue;
                if (ti->kind == TypeKind::Pointer &&
                    ti->pointeeType &&
                    ti->pointeeType->kind == TypeKind::Char) {
                    error(argExprs[i],
                          "'sprintf' from Lucis does not accept '*char'; "
                          "pass 'string' values (use 'fromCStr(...)' if needed)");
                    return nullptr;
                }
            }
        }

        // Generic function call with inferred type arguments: foo(10)
        if (!calleeName.empty()) {
            auto funcIt = genericFuncTemplates_.find(calleeName);
            if (funcIt != genericFuncTemplates_.end()) {
                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = funcIt->second.decl->paramList())
                    formalParams = paramList->param();

                if (argTypes.size() != formalParams.size()) {
                    error(expr, "generic function '" + calleeName +
                                 "' expects " + std::to_string(formalParams.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return nullptr;
                }

                auto inferred = inferGenericTypeArgs(
                    calleeName,
                    funcIt->second.typeParams,
                    funcIt->second.decl->typeParamList(),
                    formalParams,
                    argTypes,
                    expr);
                if (!inferred) return nullptr;

                applyCallOwnershipEffects(calleeName, argExprs, expr);
                return instantiateGenericFunc(calleeName, funcIt->second, *inferred, expr);
            }
        }

        // Bare enum variant call via `use EnumType::*;`
        if (!calleeName.empty()) {
            auto evIt = enumVariantImports_.find(calleeName);
            if (evIt != enumVariantImports_.end()) {
                auto* enumType = evIt->second.enumType;
                auto* variantInfo = evIt->second.variantInfo;

                if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                    error(expr, "variant '" + calleeName +
                                 "' uses named payload; use braces instead of parentheses");
                    return enumType;
                }
                if (argTypes.size() != variantInfo->payloadFields.size()) {
                    error(expr, "variant '" + calleeName +
                                 "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return enumType;
                }
                for (size_t i = 0; i < argTypes.size(); i++) {
                    auto* expected = variantInfo->payloadFields[i].typeInfo;
                    if (argTypes[i] && expected && !isAssignable(expected, argTypes[i])) {
                        error(expr, "variant '" + calleeName +
                                     "' argument " + std::to_string(i + 1) +
                                     " type mismatch: expected '" + expected->name +
                                     "', got '" + argTypes[i]->name + "'");
                    }
                }
                return enumType;
            }
        }

        // Function-like macro call: KEY_F(1) etc.
        if (!calleeName.empty()) {
            auto flmIt = cFunctionLikeMacros_.find(calleeName);
            if (flmIt != cFunctionLikeMacros_.end()) {
                auto* flm = flmIt->second;

                if (argTypes.size() != flm->paramNames.size()) {
                    error(expr, "function-like macro '" + calleeName + "' expects " +
                          std::to_string(flm->paramNames.size()) + " argument(s), got " +
                          std::to_string(argTypes.size()));
                }
                // Function-like macros expand to integer constant expressions
                auto* int32TI = typeRegistry_.lookup("int32");
                if (int32TI) return int32TI;
                return nullptr;
            }
        }

        auto* calleeType = resolveExprType(callee);

        // User-defined function with full type info
        if (calleeType && calleeType->kind == TypeKind::Function) {
            size_t paramCount = calleeType->paramTypes.size();

            if (calleeType->isVariadic) {
                // Variadic param itself is optional; only fixed params before it are required
                size_t requiredCount = paramCount > 0 ? paramCount - 1 : 0;
                if (argTypes.size() < requiredCount) {
                    error(expr, "function call expects at least " +
                                     std::to_string(requiredCount) +
                                     " arguments " + formatParamTypes(calleeType->paramTypes) +
                                     ", got " +
                                     std::to_string(argTypes.size()));
                }
                // Validate fixed parameter types
                for (size_t i = 0; i < std::min(argTypes.size(), paramCount); i++) {
                    if (argTypes[i] && calleeType->paramTypes[i] &&
                        !isAssignable(calleeType->paramTypes[i], argTypes[i])) {
                        error(expr,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch: expected '" +
                            calleeType->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            } else {
                if (argTypes.size() != paramCount) {
                    error(expr, "function call expects " +
                                     std::to_string(paramCount) +
                                     " arguments " + formatParamTypes(calleeType->paramTypes) +
                                     ", got " +
                                     std::to_string(argTypes.size()));
                } else if (calleeName != "toString") {
                    for (size_t i = 0; i < argTypes.size(); i++) {
                        if (argTypes[i] && calleeType->paramTypes[i] &&
                            !isAssignable(calleeType->paramTypes[i], argTypes[i])) {
                            error(expr,
                                "argument " + std::to_string(i + 1) +
                                " type mismatch: expected '" +
                                calleeType->paramTypes[i]->name + "', got '" +
                                argTypes[i]->name + "'");
                        }
                    }
                }
            }
            if (!calleeName.empty())
                analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
            applyCallOwnershipEffects(calleeName, argExprs, expr);
            return calleeType->returnType;
        }

        // Comptime function call — resolve return type from declaration
        if (!calleeName.empty() && comptimeRegistry_.isComptime(calleeName)) {
            applyCallOwnershipEffects(calleeName, argExprs, expr);
            auto* decl = static_cast<LucisParser::FunctionDeclContext*>(
                comptimeRegistry_.lookup(calleeName));
            if (decl && decl->typeSpec()) {
                unsigned dims = 0;
                return resolveTypeSpec(decl->typeSpec(), dims);
            }
            return typeRegistry_.lookup("int32");
        }

        // Builtin function via registry
        if (!calleeName.empty()) {
            auto* sig = builtinRegistry_.lookup(calleeName);
            if (sig) {
                // Validate argument count
                if (sig->isVariadic) {
                    if (argTypes.size() < sig->paramTypes.size()) {
                        error(expr, "'" + calleeName + "' expects at least " +
                            std::to_string(sig->paramTypes.size()) +
                            " argument(s) " + formatParamTypes(sig->paramTypes) +
                            ", got " +
                            std::to_string(argTypes.size()));
                    }
                } else if (argTypes.size() != sig->paramTypes.size()) {
                    error(expr, "'" + calleeName + "' expects " +
                        std::to_string(sig->paramTypes.size()) +
                        " argument(s) " + formatParamTypes(sig->paramTypes) +
                        ", got " +
                        std::to_string(argTypes.size()));
                } else if (!sig->isPolymorphic) {
                    // Validate argument types
                    for (size_t i = 0; i < argTypes.size(); i++) {
                        if (!argTypes[i]) continue;
                        auto& expected = sig->paramTypes[i];
                        if (expected == "_any" || expected == "_numeric" ||
                            expected == "_integer" || expected == "_float")
                            continue;
                        auto* expectedTI = resolveBuiltinReturnType(expected);
                        if (expectedTI && !isAssignable(expectedTI, argTypes[i])) {
                            error(expr, "'" + calleeName + "' argument " +
                                std::to_string(i + 1) + ": expected '" +
                                expected + "', got '" + argTypes[i]->name + "'");
                        }
                    }
                }

                // Cross-parameter validation for polymorphic builtins
                if (sig->isPolymorphic && argTypes.size() >= 2) {
                    auto isNumeric = [](const TypeInfo* t) {
                        return t && (t->kind == TypeKind::Integer ||
                                     t->kind == TypeKind::Float);
                    };
                    auto isScalar = [](const TypeInfo* t) {
                        return t && (t->kind == TypeKind::Integer ||
                                     t->kind == TypeKind::Float ||
                                     t->kind == TypeKind::Bool ||
                                     t->kind == TypeKind::Char);
                    };
                    bool allAny = true, allNumeric = true;
                    for (auto& p : sig->paramTypes) {
                        if (p != "_any") allAny = false;
                        if (p != "_numeric") allNumeric = false;
                    }
                    if (allAny) {
                        auto* firstType = argTypes[0];
                        for (size_t i = 1; i < argTypes.size(); i++) {
                            if (!argTypes[i] || !firstType) continue;
                            bool stringVsScalar =
                                (firstType->kind == TypeKind::String && isScalar(argTypes[i])) ||
                                (argTypes[i]->kind == TypeKind::String && isScalar(firstType));
                            if (stringVsScalar) {
                                error(expr, "'" + calleeName + "' argument " +
                                    std::to_string(i + 1) + ": type mismatch, expected '" +
                                    firstType->name + "', got '" + argTypes[i]->name + "'");
                            }
                        }
                    }
                    if (allNumeric) {
                        for (size_t i = 0; i < argTypes.size(); i++) {
                            if (!argTypes[i]) continue;
                            if (!isNumeric(argTypes[i])) {
                                error(expr, "'" + calleeName + "' argument " +
                                    std::to_string(i + 1) + ": expected numeric type, got '" +
                                    argTypes[i]->name + "'");
                            }
                        }
                    }
                }

                // Resolve return type
                auto& retName = sig->returnType;
                if (retName == "_any" || retName == "_numeric") {
                    // Polymorphic: return type matches first argument
                    if (!argTypes.empty() && argTypes[0])
                    {
                        analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                        applyCallOwnershipEffects(calleeName, argExprs, expr);
                        return argTypes[0];
                    }
                    analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                    applyCallOwnershipEffects(calleeName, argExprs, expr);
                    return typeRegistry_.lookup("int32");
                }
                analyzeUnsafeCBufferCall(calleeName, expr, argExprs);
                applyCallOwnershipEffects(calleeName, argExprs, expr);
                return resolveBuiltinReturnType(retName);
            }
        }

        return nullptr;
    }

    // ── Field access: expr.field ─────────────────────────────────────
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(fa->expression());
        auto fieldName = fa->IDENTIFIER()->getText();

        // .len/.length on array/variadic params → int64
        if (fieldName == "len" || fieldName == "length") {
            if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(fa->expression())) {
                auto it = locals_.find(ident->IDENTIFIER()->getText());
                if (it != locals_.end() && it->second.arrayDims > 0)
                    return typeRegistry_.lookup("usize");
            }
            // .len/.length on string → usize
            if (baseType && baseType->kind == TypeKind::String)
                return typeRegistry_.lookup("usize");
            // .len/.length on Vec<T> → usize
            if (baseType && baseType->kind == TypeKind::Extended &&
                (baseType->extendedKind == "Vec" || baseType->extendedKind == "Set" ||
                 baseType->extendedKind == "Map"))
                return typeRegistry_.lookup("usize");
        }

        if (baseType && baseType->kind == TypeKind::Pointer) {
            if (baseType->pointeeType && (baseType->pointeeType->kind == TypeKind::Struct ||
                baseType->pointeeType->kind == TypeKind::Union)) {
                baseType = baseType->pointeeType;
            } else if (auto* pointeeTI = typeRegistry_.lookup(baseType->name.substr(1))) {
                baseType = pointeeTI;
            }
        }

        if (baseType && (baseType->kind == TypeKind::Struct || baseType->kind == TypeKind::Union)) {
            for (auto& field : baseType->fields) {
                if (field.name == fieldName)
                    return field.typeInfo;
            }
            error(expr, "[" + currentFile_ + "] '" + baseType->name +
                             "' has no field '" + fieldName + "'");
        } else if (baseType) {
            error(expr, "[" + currentFile_ + "] '" + baseType->name +
                             "' has no field '" + fieldName + "'");
        }
        return nullptr;
    }

    // ── Index: expr[index] ──────────────────────────────────────────
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        auto exprs = idx->expression();
        auto* baseType = resolveExprType(exprs[0]);
        auto* indexType = resolveExprType(exprs[1]);

        // Map<K,V>[key] returns V — key type must match K
        if (baseType && baseType->kind == TypeKind::Extended && baseType->keyType) {
            if (indexType && !isAssignable(baseType->keyType, indexType))
                error(expr, "map key type mismatch: expected '" +
                             baseType->keyType->name + "', got '" +
                             indexType->name + "'");
            if (!baseType->valueType) {
                error(expr, "map value type could not be resolved");
                return typeRegistry_.lookup("void");
            }
            return baseType->valueType;
        }

        // Ranges in [] are for loops/comprehensions, not for string (see docs/language/ranges.md, docs/stdlib/string.md)
        {
            auto* indexExpr = exprs[1];
            while (auto* p = dynamic_cast<LucisParser::ParenExprContext*>(indexExpr))
                indexExpr = p->expression();
            if (baseType && baseType->kind == TypeKind::String &&
                (dynamic_cast<LucisParser::RangeExprContext*>(indexExpr) ||
                 dynamic_cast<LucisParser::RangeInclExprContext*>(indexExpr))) {
                error(expr,
                    "cannot subscript 'string' with a range: use .slice(start, end) on the "
                    "string or std::str::slice (see docs/stdlib/string.md)");
                return nullptr;
            }
        }

        // Vec<T>[i] and array[i] — index must be integer
        if (indexType && !isInteger(indexType))
            error(expr, "index must be integer, got '" +
                             indexType->name + "'");

        // Array/Slice index: []T[i] returns T - check BEFORE pointer dereference.
        // Without this, []cstring (which resolves to *char) is confused with
        // a pointer dereference, incorrectly stripping the pointer type.
        unsigned baseDims = resolveExprArrayDims(exprs[0]);
        if (baseDims > 0)
            return baseType;
        // Dereference pointer if necessary
        auto* derefType = baseType;
        if (derefType && derefType->kind == TypeKind::Pointer && derefType->pointeeType)
            derefType = derefType->pointeeType;

        // Vec<T>[i] returns T
        if (derefType && derefType->kind == TypeKind::Extended && derefType->elementType)
            return derefType->elementType;

        // string[i] returns char (uint8)
        if (derefType && derefType->kind == TypeKind::String)
            return typeRegistry_.lookup("char");

        // If original type is pointer and dereferenced type is not handled above, return deref type
        if (baseType && baseType->kind == TypeKind::Pointer && derefType)
            return derefType;

        return baseType;
    }

    // ── Struct / Union literal: Name { field: expr, ... } ────────────
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(expr)) {
        auto ids = sl->IDENTIFIER();
        if (ids.empty()) return nullptr;

        auto typeName = ids[0]->getText();
        auto* typeInfo = typeRegistry_.lookup(typeName);

        if (!typeInfo) {
            error(expr, "unknown type '" + typeName + "'");
            return nullptr;
        }
        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + typeName + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = sl->expression();
        size_t fieldCount = ids.size() - 1;

        if (typeInfo->kind == TypeKind::Union) {
            if (fieldCount != 1) {
                error(expr, "union '" + typeName +
                                 "' literal must initialize exactly 1 field, got " +
                                 std::to_string(fieldCount));
            }
        } else {
            size_t requiredFields = 0;
            for (auto& sf : typeInfo->fields) {
                if (!sf.autoFill) requiredFields++;
            }
            if (fieldCount < requiredFields) {
                error(expr, "struct '" + typeName + "' requires at least " +
                                 std::to_string(requiredFields) +
                                 " fields, got " + std::to_string(fieldCount));
            } else if (fieldCount > typeInfo->fields.size()) {
                error(expr, "struct '" + typeName + "' has " +
                                 std::to_string(typeInfo->fields.size()) +
                                 " fields, got " + std::to_string(fieldCount));
            }
        }

        for (size_t i = 0; i < fieldCount; i++) {
            auto fieldName = ids[i + 1]->getText();
            bool found = false;
            for (auto& sf : typeInfo->fields) {
                if (sf.name == fieldName) {
                    found = true;
                    if (i < fieldExprs.size()) {
                        auto* valType = resolveExprType(fieldExprs[i]);
                        if (valType && sf.typeInfo &&
                            !isAssignable(sf.typeInfo, valType)) {
                            error(expr, "field '" + fieldName +
                                             "' expects type '" +
                                             sf.typeInfo->name + "', got '" +
                                             valType->name + "'");
                        }
                        // Track ownership: if field expects an owned type,
                        // mark the source expression as moved
                        if (sf.typeInfo && isDropTrackedType(sf.typeInfo, 0))
                            markExprAsMoved(fieldExprs[i], expr);
                    }
                    break;
                }
            }
            if (!found)
                error(expr, "'" + typeName +
                                 "' has no field '" + fieldName + "'");
        }
        return typeInfo;
    }

    // ── Struct / Union positional init: Name { expr, expr, ... } ────
    if (auto* spi = dynamic_cast<LucisParser::StructPosInitExprContext*>(expr)) {
        auto typeName = spi->IDENTIFIER()->getText();
        auto* typeInfo = typeRegistry_.lookup(typeName);

        if (!typeInfo) {
            error(expr, "unknown type '" + typeName + "'");
            return nullptr;
        }
        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + typeName + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = spi->expression();
        if (typeInfo->kind == TypeKind::Union) {
            if (fieldExprs.size() != 1) {
                error(expr, "union '" + typeName +
                             "' literal must initialize exactly 1 field, got " +
                             std::to_string(fieldExprs.size()));
            }
        } else {
            size_t requiredFields = 0;
            for (auto& sf : typeInfo->fields) {
                if (!sf.autoFill) requiredFields++;
            }
            if (fieldExprs.size() < requiredFields) {
                error(expr, "struct '" + typeName + "' requires at least " +
                             std::to_string(requiredFields) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            } else if (fieldExprs.size() > typeInfo->fields.size()) {
                error(expr, "struct '" + typeName + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            }
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= typeInfo->fields.size()) break;
            auto& sf = typeInfo->fields[i];
            auto* valType = resolveExprType(fieldExprs[i]);
            if (valType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, valType)) {
                error(expr, "field '" + sf.name + "' expects type '" +
                             sf.typeInfo->name + "', got '" +
                             valType->name + "'");
            }
            // Track ownership: if field expects an owned type,
            // mark the source expression as moved
            if (sf.typeInfo && isDropTrackedType(sf.typeInfo, 0))
                markExprAsMoved(fieldExprs[i], expr);
        }
        return typeInfo;
    }

    // ── Qualified positional struct/union init or enum variant init: LIB::Point { x, y } or Shape::Circle { 1, 2 } ───
    if (auto* qspi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(expr)) {
        if (qspi->IDENTIFIER().empty()) {
            error(expr, "invalid qualified type initialization");
            return nullptr;
        }
        auto first = qspi->IDENTIFIER(0)->getText();
        auto second = qspi->IDENTIFIER().size() > 1 ? qspi->IDENTIFIER(1)->getText() : "";
        auto* typeInfo = tryResolveQualifiedType(expr, first, second);
        if (!typeInfo) return nullptr;

        if (typeInfo->kind == TypeKind::Enum) {
            return typeInfo;
        }

        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + first + "::" + second + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = qspi->expression();
        if (typeInfo->kind == TypeKind::Union) {
            if (fieldExprs.size() != 1) {
                error(expr, "union '" + first + "::" + second +
                             "' literal must initialize exactly 1 field, got " +
                             std::to_string(fieldExprs.size()));
            }
        } else {
            size_t requiredFields = 0;
            for (auto& sf : typeInfo->fields) {
                if (!sf.autoFill) requiredFields++;
            }
            if (fieldExprs.size() < requiredFields) {
                error(expr, "struct '" + first + "::" + second + "' requires at least " +
                             std::to_string(requiredFields) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            } else if (fieldExprs.size() > typeInfo->fields.size()) {
                error(expr, "struct '" + first + "::" + second + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldExprs.size()));
            }
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= typeInfo->fields.size()) break;
            auto& sf = typeInfo->fields[i];
            auto* valType = resolveExprType(fieldExprs[i]);
            if (valType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, valType)) {
                error(expr, "field '" + sf.name + "' expects type '" +
                             sf.typeInfo->name + "', got '" +
                             valType->name + "'");
            }
        }
        return typeInfo;
    }

    // ── Qualified named struct/union init or enum variant init: LIB::Point { x: 10, y: 20 } or Shape::Circle { r: 4.0 } ───
    if (auto* qsni = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(expr)) {
        auto first = qsni->IDENTIFIER().size() > 0 ? qsni->IDENTIFIER(0)->getText() : "";
        auto second = qsni->IDENTIFIER().size() > 1 ? qsni->IDENTIFIER(1)->getText() : "";
        auto* typeInfo = tryResolveQualifiedType(expr, first, second);
        if (!typeInfo) return nullptr;

        if (typeInfo->kind == TypeKind::Enum) {
            return typeInfo;
        }

        if (typeInfo->kind != TypeKind::Struct && typeInfo->kind != TypeKind::Union) {
            error(expr, "'" + first + "::" + second + "' is not a struct or union type");
            return nullptr;
        }

        auto fieldExprs = qsni->expression();
        auto ids = qsni->IDENTIFIER();
        size_t fieldCount = fieldExprs.size();

        if (typeInfo->kind == TypeKind::Union) {
            if (fieldCount != 1) {
                error(expr, "union '" + first + "::" + second +
                             "' literal must initialize exactly 1 field");
            }
        } else {
            size_t requiredFields = 0;
            for (auto& sf : typeInfo->fields) {
                if (!sf.autoFill) requiredFields++;
            }
            if (fieldCount < requiredFields) {
                error(expr, "struct '" + first + "::" + second + "' requires at least " +
                             std::to_string(requiredFields) +
                             " fields, got " + std::to_string(fieldCount));
            } else if (fieldCount > typeInfo->fields.size()) {
                error(expr, "struct '" + first + "::" + second + "' has " +
                             std::to_string(typeInfo->fields.size()) +
                             " fields, got " + std::to_string(fieldCount));
            }
        }

        for (size_t i = 0; i < fieldCount; i++) {
            auto fieldName = ids[i + 2]->getText();
            bool found = false;
            for (auto& sf : typeInfo->fields) {
                if (sf.name == fieldName) {
                    found = true;
                    if (i < fieldExprs.size()) {
                        auto* valType = resolveExprType(fieldExprs[i]);
                        if (valType && sf.typeInfo &&
                            !isAssignable(sf.typeInfo, valType)) {
                            error(expr, "field '" + fieldName +
                                             "' expects type '" +
                                             sf.typeInfo->name + "', got '" +
                                             valType->name + "'");
                        }
                    }
                    break;
                }
            }
            if (!found)
                error(expr, "'" + first + "::" + second +
                                 "' has no field '" + fieldName + "'");
        }
        return typeInfo;
    }

    // ── Static method call: Struct::method(args) ─────────────────────
    if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(expr)) {
        auto ids = smc->IDENTIFIER();
        if (ids.size() < 2) {
            error(expr, "invalid static call expression");
            return nullptr;
        }

        // ── Intrinsic call: lucis::core::trap() ────────────────────────
        {
            std::vector<std::string> idTexts;
            for (auto* id : ids)
                idTexts.push_back(id->getText());

            if (IntrinsicRegistry::isIntrinsicPrefix(idTexts[0])) {
                std::string ns, funcName;
                if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    error(expr, "invalid intrinsic path: expected 'lucis::module::function'");
                    return nullptr;
                }

                auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
                if (!intrinsic) {
                    error(expr, "unknown intrinsic '" + ns + "::" + funcName + "'");
                    return nullptr;
                }

                // Validate argument count
                auto argExprs = smc->argList()
                    ? smc->argList()->expression()
                    : std::vector<LucisParser::ExpressionContext*>{};

                size_t fixedCount = intrinsic->params.size();
                if (intrinsic->isVariadic) {
                    if (argExprs.size() < fixedCount) {
                        error(expr, "intrinsic '" + ns + "::" + funcName +
                              "' expects at least " + std::to_string(fixedCount) +
                              " argument(s), got " + std::to_string(argExprs.size()));
                        return nullptr;
                    }
                } else if (argExprs.size() != fixedCount) {
                    error(expr, "intrinsic '" + ns + "::" + funcName + "' expects " +
                          std::to_string(fixedCount) + " argument(s), got " +
                          std::to_string(argExprs.size()));
                    return nullptr;
                }

                // Validate argument types (fixed params only for variadic)
                for (size_t i = 0; i < std::min(argExprs.size(), fixedCount); i++) {
                    auto* argType = resolveExprType(argExprs[i]);
                    auto& expected = intrinsic->params[i].type;
                    if (expected != "_any" && argType) {
                        auto* expectedType = typeRegistry_.lookup(expected);
                        if (!canWidenTo(argType, expectedType)) {
                            error(argExprs[i], "intrinsic '" + ns + "::" + funcName +
                                  "' argument " + std::to_string(i + 1) + ": expected '" +
                                  expected + "', got '" + argType->name + "'");
                        }
                    }
                }

                return resolveBuiltinReturnType(intrinsic->returnType);
            }
        }
        // ── End intrinsic call check ─────────────────────────────────

        auto structName = ids.front()->getText();
        auto methodName = ids.back()->getText();

        auto checkStdModuleCall = [&](const std::string& modulePath) -> const TypeInfo* {
            ImportResolver probe;
            probe.addImport(modulePath, methodName);

            std::vector<const TypeInfo*> moduleArgTypes;
            if (auto* argList = smc->argList()) {
                for (auto* argExpr : argList->expression())
                    moduleArgTypes.push_back(resolveExprType(argExpr));
            }

            auto* sig = builtinRegistry_.lookup(methodName);
            if (!probe.isImported(methodName) || !sig) {
                error(expr, "module '" + modulePath +
                             "' does not export callable symbol '" + methodName + "'");
                return nullptr;
            }

            if (sig->isVariadic) {
                if (moduleArgTypes.size() < sig->paramTypes.size()) {
                    error(expr, "'" + methodName + "' expects at least " +
                                 std::to_string(sig->paramTypes.size()) +
                                 " argument(s) " + formatParamTypes(sig->paramTypes) +
                                 ", got " + std::to_string(moduleArgTypes.size()));
                }
            } else if (moduleArgTypes.size() != sig->paramTypes.size()) {
                error(expr, "'" + methodName + "' expects " +
                             std::to_string(sig->paramTypes.size()) +
                             " argument(s) " + formatParamTypes(sig->paramTypes) +
                             ", got " + std::to_string(moduleArgTypes.size()));
            }

            // Validate argument types for non-polymorphic builtins
            if (!sig->isPolymorphic) {
                for (size_t i = 0; i < moduleArgTypes.size(); i++) {
                    if (!moduleArgTypes[i]) continue;
                    auto& expected = sig->paramTypes[i];
                    if (expected == "_any" || expected == "_numeric" ||
                        expected == "_integer" || expected == "_float")
                        continue;
                    auto* expectedTI = resolveBuiltinReturnType(expected);
                    if (expectedTI && !isAssignable(expectedTI, moduleArgTypes[i])) {
                        error(expr, "'" + methodName + "' argument " +
                            std::to_string(i + 1) + ": expected '" +
                            expected + "', got '" + moduleArgTypes[i]->name + "'");
                    }
                }
            }

            // Cross-parameter validation for polymorphic builtins
            if (sig->isPolymorphic && moduleArgTypes.size() >= 2) {
                auto isNumeric = [](const TypeInfo* t) {
                    return t && (t->kind == TypeKind::Integer ||
                                 t->kind == TypeKind::Float);
                };
                auto isScalar = [](const TypeInfo* t) {
                    return t && (t->kind == TypeKind::Integer ||
                                 t->kind == TypeKind::Float ||
                                 t->kind == TypeKind::Bool ||
                                 t->kind == TypeKind::Char);
                };
                bool allAny = true, allNumeric = true;
                for (auto& p : sig->paramTypes) {
                    if (p != "_any") allAny = false;
                    if (p != "_numeric") allNumeric = false;
                }
                if (allAny) {
                    auto* firstType = moduleArgTypes[0];
                    for (size_t i = 1; i < moduleArgTypes.size(); i++) {
                        if (!moduleArgTypes[i] || !firstType) continue;
                        bool stringVsScalar =
                            (firstType->kind == TypeKind::String && isScalar(moduleArgTypes[i])) ||
                            (moduleArgTypes[i]->kind == TypeKind::String && isScalar(firstType));
                        if (stringVsScalar) {
                            error(expr, "'" + methodName + "' argument " +
                                std::to_string(i + 1) + ": type mismatch, expected '" +
                                firstType->name + "', got '" + moduleArgTypes[i]->name + "'");
                        }
                    }
                }
                if (allNumeric) {
                    for (size_t i = 0; i < moduleArgTypes.size(); i++) {
                        if (!moduleArgTypes[i]) continue;
                        if (!isNumeric(moduleArgTypes[i])) {
                            error(expr, "'" + methodName + "' argument " +
                                std::to_string(i + 1) + ": expected numeric type, got '" +
                                moduleArgTypes[i]->name + "'");
                        }
                    }
                }
            }

            auto& retName = sig->returnType;
            if (retName == "_any" || retName == "_numeric") {
                if (!moduleArgTypes.empty() && moduleArgTypes[0])
                    return moduleArgTypes[0];
                return typeRegistry_.lookup("int32");
            }
            return resolveBuiltinReturnType(retName);
        };

        if (ids.size() > 2) {
            auto rootName = ids.front()->getText();
            auto rootImport = userImports_.find(rootName);
            if (rootImport == userImports_.end() || rootImport->second != rootName) {
                error(expr, "module root '" + rootName +
                             "' is not imported; add 'use " + rootName + ";'");
                return nullptr;
            }

            std::string modulePath;
            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                if (!modulePath.empty()) modulePath += "::";
                modulePath += ids[i]->getText();
            }

            if (ImportResolver::isStdModule(modulePath)) {
                return checkStdModuleCall(modulePath);
            }

            // Qualified static method call on user type: LIB::User::new(args)
            if (ids.size() == 3) {
                auto nsName = ids[0]->getText();
                auto typeName = ids[1]->getText();
                auto methodName2 = ids[2]->getText();

                // Validate type exists in module
                if (moduleRegistry_) {
                    if (!moduleRegistry_->hasModule(nsName)) {
                        error(expr, "unknown module '" + nsName + "'");
                        return nullptr;
                    }
                    auto* sym = moduleRegistry_->findSymbol(nsName, typeName);
                    if (!sym) {
                        error(expr, "'" + nsName + "::" + typeName + "' is not a known type");
                        return nullptr;
                    }
                }
                auto* ti = typeRegistry_.lookup(typeName);
                if (!ti || (ti->kind != TypeKind::Struct && ti->kind != TypeKind::Union)) {
                    error(expr, "'" + nsName + "::" + typeName + "' does not support static methods");
                    return nullptr;
                }

                // Look up static method from struct methods (walks parent chain)
                auto* sm = findMethodInChain(ti, methodName2);
                if (sm && sm->isStatic) {
                    if (auto* argList = smc->argList())
                        for (auto* a : argList->expression()) resolveExprType(a);
                    return sm->returnType;
                }
                error(expr, "type '" + nsName + "::" + typeName +
                             "' has no static method '" + methodName2 + "'");
                return nullptr;
            }

            error(expr, "unsupported qualified static call '" + modulePath +
                         "::" + methodName + "'");
            return nullptr;
        }

        auto importedModule = userImports_.find(structName);
        if (importedModule != userImports_.end()) {
            if (ImportResolver::isStdModule(importedModule->second)) {
                return checkStdModuleCall(importedModule->second);
            }
            if (moduleRegistry_ && moduleRegistry_->hasModule(importedModule->second)) {
                auto* sym = moduleRegistry_->findSymbol(importedModule->second, methodName);
                if (sym) {
                    if (sym->kind == ExportedSymbol::Function) {
                        auto* funcDecl = static_cast<LucisParser::FunctionDeclContext*>(sym->decl);
                        unsigned retDims = 0;
                        auto* retType = resolveTypeSpec(funcDecl->typeSpec(), retDims);
                        if (!retType) return nullptr;

                        std::vector<const TypeInfo*> paramTypes;
                        if (auto* paramList = funcDecl->paramList()) {
                            for (auto* param : paramList->param()) {
                                unsigned pDims = 0;
                                auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
                                if (!pType) return nullptr;
                                paramTypes.push_back(pType);
                            }
                        }
                        return makeFunctionType(retType, paramTypes, false);
                    }
                    error(expr, "module '" + importedModule->second + "' does not export callable symbol '" + methodName + "'");
                    return nullptr;
                }
            }
        }

        std::vector<const TypeInfo*> argTypes;
        if (auto* argList = smc->argList()) {
            for (auto* argExpr : argList->expression())
                argTypes.push_back(resolveExprType(argExpr));
        }

        auto* enumType = typeRegistry_.lookup(structName);
        if (enumType && enumType->kind == TypeKind::Enum) {
            auto* variantInfo = findEnumVariantInfo(enumType, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + structName + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return enumType;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + structName + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return enumType;
            }
            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + structName + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return enumType;
            }
            auto args = smc->argList() ? smc->argList()->expression()
                                       : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + structName + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return enumType;
        }

        // Generic enum constructor inside an instantiated generic function,
        // e.g. in sum<T>: Result::Ok(value) where activeTypeSubst_ has T.
        auto genEnumIt = genericEnumTemplates_.find(structName);
        if (genEnumIt != genericEnumTemplates_.end() && !activeTypeSubst_.empty()) {
            std::vector<const TypeInfo*> inferredArgs;
            for (const auto& tp : genEnumIt->second.typeParams) {
                auto it = activeTypeSubst_.find(tp);
                if (it == activeTypeSubst_.end() || !it->second) {
                    error(expr, "cannot infer type arguments for generic enum '" +
                                    structName + "'; use explicit type arguments");
                    return nullptr;
                }
                inferredArgs.push_back(it->second);
            }

            auto* inferredEnum = instantiateGenericEnum(structName, genEnumIt->second,
                                                        inferredArgs, expr);
            if (!inferredEnum) return nullptr;

            auto* variantInfo = findEnumVariantInfo(inferredEnum, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + inferredEnum->name + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return inferredEnum;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return inferredEnum;
            }
            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return inferredEnum;
            }
            auto args = smc->argList() ? smc->argList()->expression()
                                       : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + inferredEnum->name + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return inferredEnum;
        }

        // Generic static method inference: Node::create(42)
        auto extIt = genericExtendTemplates_.find(structName);
        auto structIt = genericStructTemplates_.find(structName);
        if (extIt != genericExtendTemplates_.end() && structIt != genericStructTemplates_.end()) {
            for (auto* method : extIt->second.decl->extendMethod()) {
                if (method->AMPERSAND()) continue;
                if (method->IDENTIFIER(0)->getText() != methodName) continue;

                std::vector<LucisParser::ParamContext*> formalParams;
                if (auto* paramList = method->paramList())
                    formalParams = paramList->param();

                if (argTypes.size() != formalParams.size()) {
                    error(expr, "static method '" + structName + "::" + methodName +
                                 "' expects " + std::to_string(formalParams.size()) +
                                 " argument(s), got " + std::to_string(argTypes.size()));
                    return nullptr;
                }

                auto inferred = inferGenericTypeArgs(
                    structName + "::" + methodName,
                    extIt->second.typeParams,
                    extIt->second.decl->typeParamList(),
                    formalParams,
                    argTypes,
                    expr);
                if (!inferred) return nullptr;

                auto* instanceTI = instantiateGenericStruct(structName, structIt->second,
                                                            *inferred, expr);
                if (!instanceTI) return nullptr;

                auto* sm = findMethodInChain(instanceTI, methodName);
                if (!sm || !sm->isStatic) {
                    error(expr, "struct '" + instanceTI->name + "' has no static method '" + methodName + "'");
                    return nullptr;
                }

                if (argTypes.size() != sm->paramTypes.size()) {
                    error(expr, "static method '" + structName + "::" + methodName + "' argument count mismatch: expected " +
                        std::to_string(sm->paramTypes.size()) + ", got " + std::to_string(argTypes.size()));
                    return nullptr;
                }
                for (size_t i = 0; i < argTypes.size(); i++) {
                    if (!argTypes[i] || !sm->paramTypes[i]) continue;
                    if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                        error(expr, "static method '" + structName + "::" +
                            methodName + "' argument " +
                            std::to_string(i + 1) + " type mismatch: expected '" +
                            sm->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
                return sm->returnType;
            }
        }

        auto* structType = typeRegistry_.lookup(structName);
        if (!structType) {
            error(expr, "unknown type '" + structName + "'");
            return nullptr;
        }
        if (structType->kind != TypeKind::Struct) {
            error(expr, "'" + structName + "' is not a struct type");
            return nullptr;
        }

        auto* sm = findMethodInChain(structType, methodName);
        if (!sm || !sm->isStatic) {
            if (auto* argList = smc->argList())
                for (auto* argExpr : argList->expression())
                    resolveExprType(argExpr);
            error(expr, "struct '" + structName + "' has no static method '" + methodName + "'");
            return nullptr;
        }

        if (argTypes.size() != sm->paramTypes.size()) {
            error(expr, "static method '" + structName + "::" + methodName +
                "' expects " + std::to_string(sm->paramTypes.size()) +
                " arguments " + formatParamTypes(sm->paramTypes) +
                ", got " + std::to_string(argTypes.size()));
        } else {
            for (size_t i = 0; i < argTypes.size(); i++) {
                if (!argTypes[i] || !sm->paramTypes[i]) continue;
                if (!isAssignable(sm->paramTypes[i], argTypes[i])) {
                    error(expr, "static method '" + structName + "::" +
                        methodName + "' argument " +
                        std::to_string(i + 1) + " type mismatch: expected '" +
                        sm->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
        }
        return sm->returnType;
    }

    // ── Generic function call: max<int32>(a, b) ─────────────────────
    if (auto* gfc = dynamic_cast<LucisParser::GenericFnCallExprContext*>(expr)) {
        auto funcName = gfc->IDENTIFIER()->getText();
        auto typeParamSpecs = gfc->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // Find the generic function template
        auto funcIt = genericFuncTemplates_.find(funcName);
        if (funcIt == genericFuncTemplates_.end()) {
            error(expr, "'" + funcName + "' is not a generic function");
            if (auto* argList = gfc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return nullptr;
        }

        // Instantiate (type-check the body with concrete types)
        return instantiateGenericFunc(funcName, funcIt->second, typeArgs, expr);
    }

    // ── Generic static method call: Node<int32>::create(42) ─────────
    if (auto* gsmc = dynamic_cast<LucisParser::GenericStaticMethodCallExprContext*>(expr)) {
        auto ids = gsmc->IDENTIFIER();
        auto structBaseName = ids[0]->getText();
        auto methodName = ids[1]->getText();
        auto typeParamSpecs = gsmc->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(structBaseName);
        if (enumIt != genericEnumTemplates_.end()) {
            auto* enumType = instantiateGenericEnum(structBaseName, enumIt->second, typeArgs, expr);
            if (!enumType) return nullptr;

            auto* variantInfo = findEnumVariantInfo(enumType, methodName);
            if (!variantInfo) {
                error(expr, "enum '" + enumType->name + "' has no variant '" + methodName +
                             "'; enums do not support static methods via 'extend'");
                return enumType;
            }
            if (variantInfo->payloadKind == EnumPayloadKind::Named) {
                error(expr, "variant '" + enumType->name + "::" + methodName +
                             "' uses named payload; use braces instead of parentheses");
                return enumType;
            }

            std::vector<const TypeInfo*> argTypes;
            if (auto* argList = gsmc->argList()) {
                for (auto* argExpr : argList->expression())
                    argTypes.push_back(resolveExprType(argExpr));
            }

            if (argTypes.size() != variantInfo->payloadFields.size()) {
                error(expr, "variant '" + enumType->name + "::" + methodName +
                             "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                             " argument(s), got " + std::to_string(argTypes.size()));
                return enumType;
            }
            auto args = gsmc->argList() ? gsmc->argList()->expression()
                                        : std::vector<LucisParser::ExpressionContext*>{};
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* expected = variantInfo->payloadFields[i].typeInfo;
                bool literalToVecOk = false;
                if (expected && expected->kind == TypeKind::Extended &&
                    expected->extendedKind == "Vec" &&
                    i < args.size() &&
                    dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i])) {
                    literalToVecOk = true;
                    auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(args[i]);
                    for (auto* e : arr->expression()) {
                        auto* et = resolveExprType(e);
                        if (et && expected->elementType &&
                            !isAssignable(expected->elementType, et)) {
                            error(e, "element type mismatch: expected '" +
                                         expected->elementType->name + "', got '" +
                                         et->name + "'");
                            literalToVecOk = false;
                        }
                    }
                }
                if (argTypes[i] && expected &&
                    !literalToVecOk &&
                    !isAssignable(expected, argTypes[i])) {
                    error(expr, "variant '" + enumType->name + "::" + methodName +
                                 "' argument " + std::to_string(i + 1) +
                                 " type mismatch: expected '" + expected->name +
                                 "', got '" + argTypes[i]->name + "'");
                }
            }
            return enumType;
        }

        // Find the generic struct template
        auto structIt = genericStructTemplates_.find(structBaseName);
        if (structIt == genericStructTemplates_.end()) {
            error(expr, "'" + structBaseName + "' is not a generic struct");
            if (auto* argList = gsmc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return nullptr;
        }

        // Instantiate the struct (registers methods)
        auto* instanceTI = instantiateGenericStruct(structBaseName, structIt->second,
                                                     typeArgs, expr);
        if (!instanceTI) return nullptr;

        auto mangledName = mangleGenericName(structBaseName, typeArgs);

        // Find the method in the instantiated struct (walks parent chain)
        auto* sm = findMethodInChain(instanceTI, methodName);
        if (sm && sm->isStatic) {
            if (auto* argList = gsmc->argList())
                for (auto* a : argList->expression()) resolveExprType(a);
            return sm->returnType;
        }

        error(expr, "struct '" + mangledName + "' has no static method '" + methodName + "'");
        return nullptr;
    }

    // ── Qualified generic call: lucis::unsafe::va_arg<int32>(ptr) ──────
    if (auto* gqfc = dynamic_cast<LucisParser::GenericQualifiedFnCallExprContext*>(expr)) {
        auto ids = gqfc->IDENTIFIER();
        if (ids.size() < 2) {
            error(expr, "invalid qualified generic call");
            return nullptr;
        }

        // Resolve type arguments
        auto typeParamSpecs = gqfc->typeSpec();
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // ── Intrinsic check: lucis::unsafe::va_arg<T>(ptr) ─────────
        if (IntrinsicRegistry::isIntrinsicPrefix(ids[0]->getText())) {
            // Build path: all identifiers except the last = namespace path
            std::vector<std::string> idTexts;
            for (auto* id : ids) idTexts.push_back(id->getText());

            std::string ns, funcName;
            if (!IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                error(expr, "invalid intrinsic qualified call");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            auto* intrinsic = intrinsicRegistry_.lookup(ns, funcName);
            if (!intrinsic || !intrinsic->isGeneric) {
                error(expr, "unknown generic intrinsic '" + ns + "::" + funcName + "'");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            // Validate type args: must have at least 1
            if (typeArgs.empty()) {
                error(expr, "generic intrinsic '" + ns + "::" + funcName +
                      "' expects at least 1 type argument, got 0");
                if (auto* argList = gqfc->argList())
                    for (auto* a : argList->expression()) resolveExprType(a);
                return nullptr;
            }

            // Validate argument count
            auto argExprs = gqfc->argList()
                ? gqfc->argList()->expression()
                : std::vector<LucisParser::ExpressionContext*>{};

            size_t fixedCount = intrinsic->params.size();
            if (intrinsic->isVariadic) {
                if (argExprs.size() < fixedCount) {
                    error(expr, "generic intrinsic '" + ns + "::" + funcName +
                          "' expects at least " + std::to_string(fixedCount) +
                          " argument(s), got " + std::to_string(argExprs.size()));
                    return nullptr;
                }
            } else if (argExprs.size() != fixedCount) {
                error(expr, "generic intrinsic '" + ns + "::" + funcName + "' expects " +
                      std::to_string(fixedCount) + " argument(s), got " +
                      std::to_string(argExprs.size()));
                return nullptr;
            }

            // Validate argument types (skip _any)
            for (size_t i = 0; i < std::min(argExprs.size(), fixedCount); i++) {
                auto* argType = resolveExprType(argExprs[i]);
                auto& expected = intrinsic->params[i].type;
                if (expected != "_any" && argType) {
                    auto* expectedType = typeRegistry_.lookup(expected);
                    if (!canWidenTo(argType, expectedType)) {
                        error(argExprs[i], "generic intrinsic '" + ns + "::" + funcName +
                              "' argument " + std::to_string(i + 1) + ": expected '" +
                              expected + "', got '" + argType->name + "'");
                    }
                }
            }

            // Return type: "_any" resolves to the designated type arg
            if (intrinsic->returnType == "_any" && !typeArgs.empty()) {
                size_t idx = intrinsic->returnTypeArgIndex;
                if (idx < typeArgs.size())
                    return typeArgs[idx];
            }
            return resolveBuiltinReturnType(intrinsic->returnType);
        }

        // Not an intrinsic — fall through to generic struct lookup
        error(expr, "qualified generic call is only supported for intrinsics");
        if (auto* argList = gqfc->argList())
            for (auto* a : argList->expression()) resolveExprType(a);
        return nullptr;
    }

    // ── Generic struct literal: Node<int32> { value: 42, next: null } ─
    if (auto* gsl = dynamic_cast<LucisParser::GenericStructLitExprContext*>(expr)) {
        auto baseName = gsl->IDENTIFIER(0)->getText();
        auto typeParamSpecs = gsl->typeSpec();

        // Resolve type arguments
        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        // Find template and instantiate
        auto structIt = genericStructTemplates_.find(baseName);
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (structIt == genericStructTemplates_.end() &&
            unionIt == genericUnionTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic struct or union");
            return nullptr;
        }

        const TypeInfo* instanceTI = nullptr;
        if (structIt != genericStructTemplates_.end()) {
            instanceTI = instantiateGenericStruct(baseName, structIt->second,
                                                  typeArgs, expr);
        } else {
            instanceTI = instantiateGenericUnion(baseName, unionIt->second,
                                                 typeArgs, expr);
        }
        if (!instanceTI) return nullptr;

        // Validate field initializers
        auto mangledName = mangleGenericName(baseName, typeArgs);
        auto ids = gsl->IDENTIFIER();
        auto fieldExprs = gsl->expression();
        std::unordered_map<std::string, const TypeInfo*> fieldTypeMap;
        for (auto& f : instanceTI->fields)
            fieldTypeMap[f.name] = f.typeInfo;

        if (instanceTI->kind == TypeKind::Union && fieldExprs.size() != 1) {
            error(expr, "union '" + mangledName +
                         "' literal must initialize exactly 1 field, got " +
                         std::to_string(fieldExprs.size()));
        }

        // ids[0] is the type name, ids[1..n] are field names
        for (size_t i = 0; i < fieldExprs.size(); i++) {
            auto fieldName = ids[i + 1]->getText();
            auto* exprType = resolveExprType(fieldExprs[i]);
            auto ftIt = fieldTypeMap.find(fieldName);
            if (ftIt == fieldTypeMap.end()) {
                error(expr, "unknown field '" + fieldName + "' in '" + mangledName + "'");
            } else if (exprType && !isAssignable(ftIt->second, exprType)) {
                error(expr, "field '" + fieldName + "' type mismatch: expected '" +
                           ftIt->second->name + "', got '" + exprType->name + "'");
            }
        }

        return instanceTI;
    }

    // ── Generic struct positional init: Node<int32> { expr, expr, ... } ─
    if (auto* gspi = dynamic_cast<LucisParser::GenericStructPosInitExprContext*>(expr)) {
        auto baseName = gspi->IDENTIFIER()->getText();
        auto typeParamSpecs = gspi->typeSpec();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : typeParamSpecs) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto structIt = genericStructTemplates_.find(baseName);
        auto unionIt = genericUnionTemplates_.find(baseName);
        if (structIt == genericStructTemplates_.end() &&
            unionIt == genericUnionTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic struct or union");
            return nullptr;
        }

        const TypeInfo* instanceTI = nullptr;
        if (structIt != genericStructTemplates_.end()) {
            instanceTI = instantiateGenericStruct(baseName, structIt->second,
                                                  typeArgs, expr);
        } else {
            instanceTI = instantiateGenericUnion(baseName, unionIt->second,
                                                 typeArgs, expr);
        }
        if (!instanceTI) return nullptr;

        auto mangledName = mangleGenericName(baseName, typeArgs);
        auto fieldExprs = gspi->expression();

        if (instanceTI->kind == TypeKind::Union && fieldExprs.size() != 1) {
            error(expr, "union '" + mangledName +
                         "' literal must initialize exactly 1 field, got " +
                         std::to_string(fieldExprs.size()));
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= instanceTI->fields.size()) break;
            auto& sf = instanceTI->fields[i];
            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && sf.typeInfo &&
                !isAssignable(sf.typeInfo, exprType)) {
                error(expr, "field '" + sf.name + "' type mismatch: expected '" +
                           sf.typeInfo->name + "', got '" + exprType->name + "'");
            }
        }

        return instanceTI;
    }



    // ── Generic enum named variant literal: Enum<T>::Variant { ... } ─────
    if (auto* genv = dynamic_cast<LucisParser::GenericEnumNamedVariantExprContext*>(expr)) {
        auto ids = genv->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : genv->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
            return enumType;
        }
        if (variantInfo->payloadKind != EnumPayloadKind::Named) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' does not use named payload");
            return enumType;
        }

        auto fieldExprs = genv->expression();
        std::unordered_set<std::string> provided;
        for (size_t i = 0; i < fieldExprs.size(); i++) {
            auto fieldName = ids[i + 2]->getText();
            if (!provided.insert(fieldName).second) {
                error(expr, "duplicate payload field '" + fieldName +
                             "' in variant '" + enumType->name + "::" + variantName + "'");
                continue;
            }

            const FieldInfo* expectedField = nullptr;
            for (const auto& field : variantInfo->payloadFields) {
                if (field.name == fieldName) {
                    expectedField = &field;
                    break;
                }
            }
            if (!expectedField) {
                error(expr, "unknown payload field '" + fieldName +
                             "' in variant '" + enumType->name + "::" + variantName + "'");
                continue;
            }

            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && expectedField->typeInfo &&
                !isAssignable(expectedField->typeInfo, exprType)) {
                error(expr, "payload field '" + fieldName +
                             "' type mismatch: expected '" + expectedField->typeInfo->name +
                             "', got '" + exprType->name + "'");
            }
        }

        if (provided.size() != variantInfo->payloadFields.size()) {
            for (const auto& field : variantInfo->payloadFields) {
                if (!provided.count(field.name)) {
                    error(expr, "missing payload field '" + field.name +
                                 "' in variant '" + enumType->name + "::" + variantName + "'");
                }
            }
        }

        return enumType;
    }

    // ── Generic enum positional variant: Enum<T>::Variant { expr, ... } ─
    if (auto* gepv = dynamic_cast<LucisParser::GenericEnumPosVariantExprContext*>(expr)) {
        auto ids = gepv->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gepv->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
            return enumType;
        }

        auto fieldExprs = gepv->expression();
        if (fieldExprs.size() != variantInfo->payloadFields.size()) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' expects " + std::to_string(variantInfo->payloadFields.size()) +
                         " payload fields, got " + std::to_string(fieldExprs.size()));
        }

        for (size_t i = 0; i < fieldExprs.size(); i++) {
            if (i >= variantInfo->payloadFields.size()) break;
            const auto& expectedField = variantInfo->payloadFields[i];
            auto* exprType = resolveExprType(fieldExprs[i]);
            if (exprType && expectedField.typeInfo &&
                !isAssignable(expectedField.typeInfo, exprType)) {
                error(expr, "payload field '" + expectedField.name +
                             "' type mismatch: expected '" + expectedField.typeInfo->name +
                             "', got '" + exprType->name + "'");
            }
        }

        return enumType;
    }

    // ── Enum access: Enum::Variant ──────────────────────────────────
    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        auto enumName = ids[0]->getText();
        auto variant = ids[1]->getText();

        auto* enumType = typeRegistry_.lookup(enumName);
        if (!enumType) {
            error(expr, "unknown enum type '" + enumName + "'");
            return nullptr;
        }
        if (enumType->kind != TypeKind::Enum) {
            error(expr, "'" + enumName + "' is not an enum type");
            return nullptr;
        }

        auto* variantInfo = findEnumVariantInfo(enumType, variant);
        if (!variantInfo) {
            error(expr, "enum '" + enumName +
                             "' has no variant '" + variant + "'");
        } else if (!variantInfo->payloadFields.empty()) {
            error(expr, "variant '" + enumName + "::" + variant +
                         "' requires payload construction");
        }
        return enumType;
    }

    // ── Generic enum access: Enum<T>::Variant ───────────────────────
    if (auto* gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        auto baseName = ids[0]->getText();
        auto variantName = ids[1]->getText();

        std::vector<const TypeInfo*> typeArgs;
        for (auto* ts : gea->typeSpec()) {
            unsigned dims = 0;
            auto* argTI = resolveTypeSpecInContext(ts, dims);
            if (!argTI) return nullptr;
            typeArgs.push_back(argTI);
        }

        auto enumIt = genericEnumTemplates_.find(baseName);
        if (enumIt == genericEnumTemplates_.end()) {
            error(expr, "'" + baseName + "' is not a generic enum");
            return nullptr;
        }

        auto* enumType = instantiateGenericEnum(baseName, enumIt->second, typeArgs, expr);
        if (!enumType) return nullptr;

        auto* variantInfo = findEnumVariantInfo(enumType, variantName);
        if (!variantInfo) {
            error(expr, "enum '" + enumType->name + "' has no variant '" + variantName + "'");
        } else if (!variantInfo->payloadFields.empty()) {
            error(expr, "variant '" + enumType->name + "::" + variantName +
                         "' requires payload construction");
        }
        return enumType;
    }

    // ── Address-of: &var ────────────────────────────────────────────
    if (auto* addr = dynamic_cast<LucisParser::AddrOfExprContext*>(expr)) {
        auto* innerType = resolveExprType(addr->expression());
        if (!innerType) return nullptr;
        // Mark the variable as used if it's a simple identifier
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(addr->expression())) {
            auto it = locals_.find(ident->IDENTIFIER()->getText());
            if (it != locals_.end()) it->second.used = true;
        }
        return getPointerType(innerType);
    }

    // ── Dereference: *expr ──────────────────────────────────────────
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr)) {
        auto* operand = resolveExprType(deref->expression());
        if (operand && operand->kind != TypeKind::Pointer) {
            error(expr, "cannot dereference non-pointer type '" +
                             operand->name + "'");
            return nullptr;
        }
        if (operand && operand->pointeeType)
            return operand->pointeeType;
        return nullptr;
    }

    // ── Array literal: [expr, expr, ...] ────────────────────────────
    if (auto* arr = dynamic_cast<LucisParser::ArrayLitExprContext*>(expr)) {
        auto elements = arr->expression();
        if (elements.empty()) return nullptr;

        auto* firstType = resolveExprType(elements[0]);
        for (size_t i = 1; i < elements.size(); i++) {
            auto* elemType = resolveExprType(elements[i]);
            if (firstType && elemType && !isAssignable(firstType, elemType))
                error(expr, "array element type mismatch: expected '" +
                                 firstType->name + "', got '" +
                                 elemType->name + "'");
        }
        return firstType;
    }

    // ── List comprehension: [expr | for type x in iterable if cond] ─
    if (auto* lc = dynamic_cast<LucisParser::ListCompExprContext*>(expr)) {
        // Register the loop variable temporarily
        unsigned varDims = 0;
        auto* varType = resolveTypeSpec(lc->typeSpec(), varDims);
        auto  varName = lc->IDENTIFIER()->getText();

        auto prev = locals_.find(varName);
        auto hadPrev = prev != locals_.end();
        VarInfo prevInfo;
        if (hadPrev) prevInfo = prev->second;

        if (varType)
            locals_[varName] = { varType, varDims, {}, true, true, nullptr };

        // Resolve iterable expression
        resolveExprType(lc->expression(1));

        // Resolve optional filter
        auto exprs = lc->expression();
        if (exprs.size() > 2)
            resolveExprType(exprs[2]);

        // Resolve the value expression — its type is the element type
        auto* elemType = resolveExprType(lc->expression(0));

        // Restore scope
        if (hadPrev)
            locals_[varName] = prevInfo;
        else
            locals_.erase(varName);

        recursionDepth_--;
        return elemType;
    }

    recursionDepth_--;
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
//  Top-level declaration checks
// ═══════════════════════════════════════════════════════════════════════

void Checker::setModuleContext(const ModuleRegistry* registry,
                                const std::string& modulePath,
                                const std::string& currentFile) {
    moduleRegistry_     = registry;
    currentModulePath_  = modulePath;
    currentFile_        = currentFile;
}

void Checker::setProjectPaths(const std::string& projectRoot,
                               const std::vector<std::string>& sourcePaths) {
    projectRoot_ = projectRoot;
    sourcePaths_ = sourcePaths;
}

std::string Checker::findModuleFile(const std::string& modPath) const {
    namespace fs = std::filesystem;
    // 1) Search stdlib directories.
    for (auto& dir : ImportResolver::stdlibSearchPaths()) {
        auto candidate = fs::path(dir) / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate.string();
    }
    // 2) Search project directories.
    if (!projectRoot_.empty()) {
        auto root = fs::path(projectRoot_);
        auto candidate = root / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec)) return candidate.string();
        for (auto& sp : sourcePaths_) {
            candidate = root / fs::path(sp) / (modPath + ".lc");
            if (fs::exists(candidate, ec)) return candidate.string();
        }
    }
    return {};
}

void Checker::setCBindings(const CBindings* bindings) {
    cBindings_ = bindings;
}

// ── c_macro { ... } block evaluation ──────────────────────────────────

// Tokenize a macro body string into tokens for evalMacroAdd.
// Splits on whitespace and separates operators: (, ), +, -, *, /, %, etc.
static std::vector<std::string> tokenizeMacroBody(const std::string& body) {
    std::vector<std::string> tokens;
    std::string tok;
    for (size_t i = 0; i < body.size(); i++) {
        char c = body[i];
        if (std::isspace(c)) {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            continue;
        }
        // Multi-char operators
        if (c == '<' && i+1 < body.size() && body[i+1] == '<') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back("<<");
            i++;
            continue;
        }
        if (c == '>' && i+1 < body.size() && body[i+1] == '>') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(">>");
            i++;
            continue;
        }
        // Single-char operators/delimiters
        if (c == '(' || c == ')' || c == '+' || c == '-' || c == '*' ||
            c == '/' || c == '%' || c == '&' || c == '|' || c == '^' ||
            c == '~' || c == '?' || c == ':') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(std::string(1, c));
            continue;
        }
        tok += c;
    }
    if (!tok.empty()) tokens.push_back(tok);
    return tokens;
}

// Extract macro name from "#define FOO ..." → "FOO"
static std::string extractMacroName(const std::string& line) {
    auto s = line.find("#define");
    if (s == std::string::npos) return {};
    s = line.find_first_not_of(" \t", s + 7);
    if (s == std::string::npos) return {};
    size_t e = s;
    while (e < line.size() && !std::isspace(line[e]) && line[e] != '(') e++;
    return line.substr(s, e - s);
}

void Checker::checkCMacroBlock(LucisParser::CMacroBlockContext* ctx) {
    namespace fs = std::filesystem;

    // 1. Extract raw C text from token: "c_macro { #define FOO 42\n... }"
    auto tok = ctx->C_MACRO_BLOCK();
    if (!tok) return;
    std::string fullText = tok->getText();

    // Strip "c_macro {" prefix (9 chars) and "}" suffix (1 char)
    if (fullText.size() < 11) {
        error(ctx, "empty c_macro block");
        return;
    }
    std::string rawC = fullText.substr(9, fullText.size() - 10);  // between braces

    // 2. Evaluate via gcc preprocessor + optional compilation
    std::string tempDir = projectRoot_.empty()
        ? "/tmp"
        : (fs::path(projectRoot_) / ".lucis" / "cmacro").string();

    CBindings localBindings;
    evalCMacroRaw(rawC, currentFile_, tok->getSymbol()->getLine(),
                  tempDir, localBindings, true);

    // 3. Register results into Checker's internal tables
    for (auto& [name, cm] : localBindings.macros()) {
        if (cMacroBindings_.findMacro(name)) continue;
        cMacroBindings_.addMacro(cm);
        auto* int32TI = typeRegistry_.lookup("int32");
        if (int32TI)
            cEnumConstants_[name] = CEnumConstant{int32TI, cm.value, 0.0, false, false};
    }

    for (auto& [name, flm] : localBindings.functionLikeMacros()) {
        if (cMacroBindings_.findFunctionLikeMacro(name)) continue;
        cMacroBindings_.addFunctionLikeMacro(flm);
        cFunctionLikeMacros_[name] = const_cast<CFunctionLikeMacro*>(
            cMacroBindings_.findFunctionLikeMacro(name));
    }
}

bool Checker::isKnownFunction(const std::string& name) const {
    // 1. Local file function
    if (functions_.count(name)) return true;
    // 1b. Local generic function template
    if (genericFuncTemplates_.count(name)) return true;
    // 1c. Comptime function
    if (comptimeRegistry_.isComptime(name)) return true;
    // 2. Builtin
    if (globalBuiltins_.count(name)) return true;
    // 3. Std import
    if (imports_.isImported(name)) return true;
    // 4. User import
    if (userImports_.count(name)) return true;
    // 5. Same-module symbol from another file
    if (moduleRegistry_ && !currentModulePath_.empty()) {
        auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
        if (sym && sym->kind == ExportedSymbol::Function &&
            sym->sourceFile != currentFile_)
            return true;
    }
    return false;
}

bool Checker::isKnownType(const std::string& name) const {
    if (typeRegistry_.lookup(name)) return true;
    // Check user-defined generic struct templates
    if (genericStructTemplates_.count(name)) return true;
    if (genericUnionTemplates_.count(name)) return true;
    if (genericEnumTemplates_.count(name)) return true;
    // Check user imports for struct/enum/alias
    if (userImports_.count(name)) return true;
    // Check same-module types from other files
    if (moduleRegistry_ && !currentModulePath_.empty()) {
        auto* sym = moduleRegistry_->findSymbol(currentModulePath_, name);
        if (sym && (sym->kind == ExportedSymbol::Struct ||
                    sym->kind == ExportedSymbol::Union ||
                    sym->kind == ExportedSymbol::Enum ||
                    sym->kind == ExportedSymbol::TypeAlias) &&
            sym->sourceFile != currentFile_)
            return true;
    }
    return false;
}

static bool typeSpecMentionsGenericParam(
    LucisParser::TypeSpecContext* typeSpec,
    const std::unordered_set<std::string>& genericParams) {
    if (!typeSpec) return false;

    if (!typeSpec->LBRACKET() && !typeSpec->STAR() && !typeSpec->LT() &&
        !typeSpec->VEC() && !typeSpec->MAP() && !typeSpec->SET() &&
        !typeSpec->TUPLE() && !typeSpec->AUTO() && !typeSpec->fnTypeSpec()) {
        return genericParams.count(typeSpec->getText()) > 0;
    }

    for (auto* inner : typeSpec->typeSpec()) {
        if (typeSpecMentionsGenericParam(inner, genericParams))
            return true;
    }
    return false;
}

void Checker::checkUseDecls(LucisParser::ProgramContext* tree) {
    auto processUse = [&](LucisParser::UseDeclContext* useDecl) {
        if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) {
            auto rootName = root->IDENTIFIER()->getText();
            if (rootName == "std") {
                userImports_[rootName] = rootName;
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(rootName)) {
                userImports_[rootName] = rootName;
            } else if (!moduleRegistry_ || !findModuleFile(ModuleRegistry::usePathToModulePath(rootName)).empty()) {
                userImports_[rootName] = rootName;
            } else {
                error(root, "unknown module root '" + rootName + "'");
            }
        } else if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) {
            std::string path;
            for (auto* id : item->modulePath()->IDENTIFIER()) {
                if (!path.empty()) path += "::";
                path += id->getText();
            }
            auto symbolName = item->IDENTIFIER()->getText();
            auto modPath = ModuleRegistry::usePathToModulePath(path);

            if (ImportResolver::isStdModule(path)) {
                if (ImportResolver::moduleExportsSymbol(path, symbolName)) {
                    imports_.addImport(path, symbolName);
                } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                    auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                    if (!sym) {
                        error(item, "module '" + path +
                                    "' does not export '" + symbolName + "'");
                    } else {
                        userImports_[symbolName] = modPath;
                    }
                } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                    userImports_[symbolName] = modPath;
                } else {
                    error(item, "unknown module '" + path + "'");
                }
            } else if (ImportResolver::isStdModule(path + "::" + symbolName)) {
                userImports_[symbolName] = modPath;
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                if (!sym) {
                    error(item, "module '" + path +
                                "' does not export '" + symbolName + "'");
                } else {
                    userImports_[symbolName] = modPath;
                }
            } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                userImports_[symbolName] = modPath;
            } else {
                error(item, "unknown module '" + path + "'");
            }
        } else if (auto* grp = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) {
            std::string path;
            for (auto* id : grp->modulePath()->IDENTIFIER()) {
                if (!path.empty()) path += "::";
                path += id->getText();
            }
            auto modPath = ModuleRegistry::usePathToModulePath(path);

            if (ImportResolver::isStdModule(path)) {
                for (auto* id : grp->IDENTIFIER()) {
                    auto symbolName = id->getText();
                    if (ImportResolver::moduleExportsSymbol(path, symbolName)) {
                        imports_.addImport(path, symbolName);
                    } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                        auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                        if (!sym) {
                            error(grp, "module '" + path +
                                        "' does not export '" + symbolName + "'");
                        } else {
                            userImports_[symbolName] = modPath;
                        }
                    } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                        userImports_[symbolName] = modPath;
                    } else {
                        error(grp, "unknown module '" + path + "'");
                    }
                }
            } else if (moduleRegistry_ && moduleRegistry_->hasModule(modPath)) {
                for (auto* id : grp->IDENTIFIER()) {
                    auto symbolName = id->getText();
                    auto* sym = moduleRegistry_->findSymbol(modPath, symbolName);
                    if (!sym) {
                        error(grp, "module '" + path +
                                    "' does not export '" + symbolName + "'");
                    } else {
                        userImports_[symbolName] = modPath;
                    }
                }
            } else if (!moduleRegistry_ || !findModuleFile(modPath).empty()) {
                for (auto* id : grp->IDENTIFIER()) {
                    userImports_[id->getText()] = modPath;
                }
            } else {
                error(grp, "unknown module '" + path + "'");
            }
        } else if (auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl)) {
            unsigned arrayDims = 0;
            auto* enumType = resolveTypeSpec(ew->typeSpec(), arrayDims);
            if (!enumType) {
                error(ew, "unknown type in 'use' wildcard");
            } else if (enumType->kind != TypeKind::Enum) {
                error(ew, "type '" + enumType->name + "' is not an enum");
            } else if (arrayDims > 0) {
                error(ew, "cannot use array type in 'use' wildcard");
            } else {
                for (const auto& vi : enumType->enumVariantInfos) {
                    auto [it, inserted] = enumVariantImports_.try_emplace(
                        vi.name,
                        InjectedVariant{ enumType, &vi });
                    if (!inserted) {
                        error(ew, "ambiguous variant name '" + vi.name +
                                   "': already imported from '" +
                                   it->second.enumType->name + "'");
                    }
                }
            }
        }
    };

    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        processUse(useDecl);
    }
    for (auto* tld : tree->topLevelDecl()) {
        auto* useDecl = tld->useDecl();
        if (!useDecl) continue;
        processUse(useDecl);
    }
}

void Checker::checkTypeAliasDecl(LucisParser::TypeAliasDeclContext* decl) {
    auto name = decl->IDENTIFIER()->getText();

    auto* existing = typeRegistry_.lookup(name);
    bool canUpgradeSkeleton = false;
    if (existing) {
        canUpgradeSkeleton =
            existing->kind == TypeKind::Struct &&
            existing->fields.empty() &&
            existing->bitWidth == 0;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    // For now, we register a Function TypeInfo for fn(...) -> T aliases
    auto* typeSpecCtx = decl->typeSpec();
    if (auto* fnSpec = typeSpecCtx->fnTypeSpec()) {
        TypeInfo ti;
        ti.name = name;
        ti.kind = TypeKind::Function;
        ti.bitWidth = 0;
        ti.isSigned = false;
        ti.builtinSuffix = "ptr";

        // Resolve return type
        auto retSpecs = fnSpec->typeSpec();
        auto retName = resolveBaseTypeName(retSpecs.back());
        auto* retTI = typeRegistry_.lookup(retName);
        if (!retTI) {
            error(decl, "unknown return type '" + retName +
                             "' in type alias '" + name + "'");
            return;
        }
        ti.returnType = retTI;

        // Resolve parameter types (all typeSpecs except the last one which is return)
        for (size_t i = 0; i + 1 < retSpecs.size(); i++) {
            auto paramName = resolveBaseTypeName(retSpecs[i]);
            auto* paramTI = typeRegistry_.lookup(paramName);
            if (!paramTI) {
                error(decl, "unknown param type '" + paramName +
                                 "' in type alias '" + name + "'");
                return;
            }
            ti.paramTypes.push_back(paramTI);
        }

        syncToSemanticDB_TypeAlias(ti, currentModulePath_, decl); // Phase 1
        typeRegistry_.registerType(std::move(ti));
    } else {
        // General alias: type MyInt = int32; / type Result = tuple<string, int32>;
        unsigned dims = 0;
        auto* baseTI = resolveTypeSpec(typeSpecCtx, dims);
        if (!baseTI) {
            error(decl, "unknown type in type alias '" + name + "'");
            return;
        }
        // Register as a copy with the alias name
        TypeInfo ti = *baseTI;
        ti.name = name;
        syncToSemanticDB_TypeAlias(ti, currentModulePath_, decl);
        typeRegistry_.registerType(std::move(ti));
    }
}

void Checker::checkStructDecl(LucisParser::StructDeclContext* decl) {
    auto name = decl->IDENTIFIER()->getText();

    // Generic struct template — register as template, not as concrete type
    if (auto* tpl = decl->typeParamList()) {
        if (genericStructTemplates_.count(name)) {
            error(decl, "generic struct '" + name + "' already defined");
            return;
        }
        GenericStructTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericStructTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericStruct(name, savedTypeParams, decl);
        return;
    }

    auto* existing = typeRegistry_.lookup(name);
    if (existing) {
        bool canUpgradeSkeleton =
            existing->kind == TypeKind::Struct &&
            existing->fields.empty() &&
            existing->bitWidth == 0;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    // Register skeleton first so self-referencing pointer fields
    // (e.g. *Node inside Node) can resolve the type.
    TypeInfo skeleton;
    skeleton.name = name;
    skeleton.kind = TypeKind::Struct;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    if (!existing)
        typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;

    // ── Parent struct inheritance ─────────────────────────────────────
    if (decl->COLON() && decl->typeSpec()) {
        unsigned parentDims = 0;
        auto* parentTI = resolveTypeSpec(decl->typeSpec(), parentDims);
        if (!parentTI || parentTI->kind != TypeKind::Struct) {
            error(decl, "parent type not found or is not a struct");
            return;
        }

        ti.parentType = parentTI;
        for (auto& pf : parentTI->fields) {
            seen.insert(pf.name);
            ti.fields.push_back(pf);
        }
    }

    for (auto* field : decl->structField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpec(field->typeSpec(), fieldDims);
        if (!fieldTI) {
            error(field, "unknown field type in struct '" + name + "'");
            return;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                             "' in struct '" + name + "'");
            continue;
        }
        std::vector<unsigned> fieldSizes;
        {
            auto* spec = field->typeSpec();
            while (spec && spec->LBRACKET()) {
                if (spec->INT_LIT())
                    fieldSizes.push_back(static_cast<unsigned>(
                        std::stoul(spec->INT_LIT()->getText())));
                spec = spec->typeSpec(0);
            }
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims, fieldSizes });
    }

    syncToSemanticDB_Struct(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}

void Checker::checkUnionDecl(LucisParser::UnionDeclContext* decl) {
    auto name = decl->IDENTIFIER()->getText();

    if (auto* tpl = decl->typeParamList()) {
        if (genericUnionTemplates_.count(name)) {
            error(decl, "generic union '" + name + "' already defined");
            return;
        }
        GenericUnionTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericUnionTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericUnion(name, savedTypeParams, decl);
        return;
    }

    auto* existing = typeRegistry_.lookup(name);
    bool canUpgradeSkeleton = false;
    if (existing) {
        canUpgradeSkeleton =
            existing->kind == TypeKind::Union &&
            existing->fields.empty() &&
            existing->bitWidth == 0;
        if (!canUpgradeSkeleton) {
            error(decl, "type '" + name + "' already defined");
            return;
        }
    }

    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::Union;
    ti.bitWidth = 0;
    ti.isSigned = false;

    std::unordered_set<std::string> seen;
    for (auto* field : decl->unionField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpec(field->typeSpec(), fieldDims);
        if (!fieldTI) {
            error(field, "unknown field type in union '" + name + "'");
            return;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                             "' in union '" + name + "'");
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    syncToSemanticDB_Union(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}

void Checker::checkEnumDecl(LucisParser::EnumDeclContext* decl) {
    auto name = decl->IDENTIFIER()->getText();

    if (typeRegistry_.lookup(name) || genericEnumTemplates_.count(name)) {
        error(decl, "type '" + name + "' already defined");
        return;
    }

    if (auto* tpl = decl->typeParamList()) {
        GenericEnumTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericEnumTemplates_[name] = std::move(tmpl);
        syncToSemanticDB_GenericEnum(name, savedTypeParams, decl);
        return;
    }

    TypeInfo ti;
    ti.name = name;
    ti.kind = TypeKind::Enum;
    ti.bitWidth = 32;
    ti.isSigned = false;
    ti.builtinSuffix = "i32";

    std::unordered_set<std::string> seen;
    unsigned nextDiscriminant = 0;
    for (auto* variantDecl : decl->enumVariant()) {
        auto variant = variantDecl->IDENTIFIER()->getText();
        if (!seen.insert(variant).second) {
            error(variantDecl, "duplicate enum variant '" + variant +
                                "' in enum '" + name + "'");
            continue;
        }

        ti.enumVariants.push_back(variant);

        EnumVariantInfo info;
        info.name = variant;
        info.isError = hasAttribute(variantDecl->attributeList(), "error");

        if (variantDecl->ASSIGN()) {
            auto discVal = tryEvalUSizeExpr(variantDecl->expression());
            if (!discVal) {
                error(variantDecl->expression(),
                      "enum variant discriminant must be a compile-time constant unsigned integer");
                continue;
            }
            info.discriminant = static_cast<unsigned>(*discVal);
            if (static_cast<uint64_t>(info.discriminant) != *discVal) {
                error(variantDecl->expression(),
                      "enum variant discriminant value too large");
                continue;
            }
            bool dup = false;
            for (auto& existing : ti.enumVariantInfos) {
                if (existing.discriminant == info.discriminant) {
                    error(variantDecl,
                          "duplicate enum variant discriminant " + std::to_string(info.discriminant) +
                          " in enum '" + name + "'");
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
            nextDiscriminant = info.discriminant + 1;
        } else {
            info.discriminant = nextDiscriminant++;
        }

        if (variantDecl->LPAREN()) {
            info.payloadKind = EnumPayloadKind::Tuple;
            auto payloadTypes = variantDecl->typeSpec();
            for (size_t i = 0; i < payloadTypes.size(); i++) {
                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpec(payloadTypes[i], fieldDims);
                if (!fieldTI) {
                    error(payloadTypes[i], "unknown payload type in enum variant '" +
                                          name + "::" + variant + "'");
                    return;
                }
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadTypes[i];
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadTypes[i],
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          name + "::" + variant + "' or switch to 'vec<T>'");
                    return;
                }
                info.payloadFields.push_back({"_" + std::to_string(i), fieldTI, fieldDims, fieldSizes});
            }
        } else if (variantDecl->LBRACE()) {
            info.payloadKind = EnumPayloadKind::Named;
            std::unordered_set<std::string> fieldSeen;
            for (auto* payloadField : variantDecl->enumPayloadField()) {
                auto fieldName = payloadField->IDENTIFIER()->getText();
                if (!fieldSeen.insert(fieldName).second) {
                    error(payloadField, "duplicate payload field '" + fieldName +
                                       "' in enum variant '" + name + "::" + variant + "'");
                    return;
                }

                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpec(payloadField->typeSpec(), fieldDims);
                if (!fieldTI) {
                    error(payloadField, "unknown payload type in enum variant '" +
                                        name + "::" + variant + "'");
                    return;
                }
                std::vector<unsigned> fieldSizes;
                auto* spec = payloadField->typeSpec();
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadField,
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          name + "::" + variant + "' or switch to 'vec<T>'");
                    return;
                }
                info.payloadFields.push_back({fieldName, fieldTI, fieldDims, fieldSizes});
            }
        }

        ti.enumVariantInfos.push_back(std::move(info));
    }

    syncToSemanticDB_Enum(ti, currentModulePath_, decl);
    typeRegistry_.registerType(std::move(ti));
}

void Checker::checkExtendDecl(LucisParser::ExtendDeclContext* decl) {
    auto structName = decl->IDENTIFIER()->getText();

    bool isGenericStruct = genericStructTemplates_.count(structName) != 0;
    bool isGenericEnum = genericEnumTemplates_.count(structName) != 0;
    auto* targetTI = typeRegistry_.lookup(structName);
    if (isGenericEnum) {
        error(decl, "enum '" + structName + "' cannot be extended; only structs and unions support 'extend'");
        return;
    }
    if (!targetTI && !isGenericStruct) {
        error(decl, "cannot extend unknown type '" + structName + "'");
        return;
    }
    if (targetTI && targetTI->kind == TypeKind::Enum) {
        error(decl, "enum '" + structName + "' cannot be extended; only structs and unions support 'extend'");
        return;
    }
    if (targetTI && targetTI->kind != TypeKind::Struct) {
        error(decl, "only structs and unions can be extended; '" + structName + "' is not a supported extend target");
        return;
    }

    // Generic extend block — register as template and skip body registration
    if (auto* tpl = decl->typeParamList()) {
        if (genericExtendTemplates_.count(structName)) {
            error(decl, "generic extend for '" + structName + "' already defined");
            return;
        }
        GenericExtendTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = decl;
        genericExtendTemplates_[structName] = std::move(tmpl);
        syncToSemanticDB_GenericExtend(structName, savedTypeParams, decl);
        return;
    }

    for (auto* method : decl->extendMethod()) {
        auto methodName = method->IDENTIFIER(0)->getText();

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(method->typeSpec(), retDims);
        if (!retType) continue;

        StructMethodInfo info;
        info.name = methodName;
        info.returnType = retType;
        info.isStatic = (method->AMPERSAND() == nullptr);

        // Static methods use paramList, instance methods use direct param()
        std::vector<LucisParser::ParamContext*> params;
        if (info.isStatic) {
            if (auto* pl = method->paramList())
                params = pl->param();
        } else {
            params = method->param();
        }

        for (auto* param : params) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;
            info.paramTypes.push_back(pType);
        }

        if (methodName == "drop" && !info.isStatic && info.paramTypes.empty()) {
            if (auto* mutTI = typeRegistry_.lookupMutable(structName))
                mutTI->dropTracked = true;
        }

        structMethods_[structName].push_back(std::move(info));
    }
    // Phase 1: sync extend methods to SemanticDB
    syncToSemanticDB_Extend(structName, structMethods_[structName]);
}

const Checker::StructMethodInfo* Checker::findMethodInChain(
    const TypeInfo* ti, const std::string& name) const {
    for (auto* t = ti; t; t = t->parentType) {
        auto it = structMethods_.find(t->name);
        if (it == structMethods_.end()) continue;
        for (auto& m : it->second) {
            if (m.name == name) return &m;
        }
    }
    return nullptr;
}

std::vector<const Checker::StructMethodInfo*> Checker::collectMethodsInChain(
    const TypeInfo* ti) const {
    std::vector<const StructMethodInfo*> result;
    for (auto* t = ti; t; t = t->parentType) {
        auto it = structMethods_.find(t->name);
        if (it == structMethods_.end()) continue;
        for (auto& m : it->second)
            result.push_back(&m);
    }
    return result;
}

void Checker::checkExtendMethodBodies(LucisParser::ExtendDeclContext* decl) {
    // Generic extend blocks are processed lazily during struct instantiation
    if (decl->typeParamList()) return;

    auto structName = decl->IDENTIFIER()->getText();
    auto* structTI = typeRegistry_.lookup(structName);
    if (!structTI || structTI->kind != TypeKind::Struct) return;

    for (auto* method : decl->extendMethod()) {
        locals_.clear();
        for (auto& [name, vi] : globalVars_)
            locals_[name] = vi;

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(method->typeSpec(), retDims);
        if (!retType) retType = typeRegistry_.lookup("void");

        bool isInstance = (method->AMPERSAND() != nullptr);

        // Register 'self' for instance methods as *StructName
        if (isInstance) {
            auto* ptrType = getPointerType(structTI);
            locals_["self"] = {ptrType, 0, {}, true, true, nullptr};
        }

        // Register parameters
        std::vector<LucisParser::ParamContext*> params;
        if (isInstance) {
            params = method->param();
        } else {
            if (auto* pl = method->paramList())
                params = pl->param();
        }

        for (auto* param : params) {
            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;

            if (locals_.count(paramName)) {
                error(param, "duplicate parameter name '" + paramName + "'");
                continue;
            }

            if (param->SPREAD())
                locals_[paramName] = {pType, 1, {}, true, true, nullptr};
            else
locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
        }

        auto* prevRet = currentReturnType_;
        currentReturnType_ = retType;
        checkBlock(method->block(), retType);
        currentReturnType_ = prevRet;

        if (retType->kind != TypeKind::Void) {
            if (!blockAlwaysReturns(method->block())) {
                auto methodName = method->IDENTIFIER(0)->getText();
                error(method, "method '" + structName + "." + methodName +
                              "' must return a value of type '" + retType->name +
                              "' on all code paths");
            }
        }

        warnUnusedLocals(method);
    }
}

void Checker::registerFunctionSignature(LucisParser::FunctionDeclContext* func) {
    if (!func || func->IDENTIFIER().empty()) {
        error(func, "function must have a valid name");
        return;
    }
    auto funcName = func->IDENTIFIER(0)->getText();

    // Comptime functions: register in comptime registry, skip normal registration
    if (func->COMPTIME()) {
        comptimeRegistry_.registerFunction(funcName,
            static_cast<void*>(func),
            func->typeParamList() != nullptr);
        return;
    }

    // Generic function template — register as template, not as a concrete function
    if (auto* tpl = func->typeParamList()) {
        if (genericFuncTemplates_.count(funcName)) {
            error(func, "generic function '" + funcName + "' already defined");
            return;
        }
        GenericFuncTemplate tmpl;
        for (auto* tp : tpl->typeParam())
            tmpl.typeParams.push_back(tp->IDENTIFIER(0)->getText());
        auto savedTypeParams = tmpl.typeParams;
        tmpl.decl = func;
        genericFuncTemplates_[funcName] = std::move(tmpl);
        syncToSemanticDB_GenericFunc(funcName, savedTypeParams, func);
        return;
    }

    // Detect duplicate function definitions (skip builtins/externs)
    if (functions_.count(funcName) && !globalBuiltins_.count(funcName)) {
        std::string errorMsg = "function '" + funcName + "' already defined";
        if (auto prevDecl = functionDecls_[funcName]) {
            auto prevLine = prevDecl->getStart()->getLine();
            errorMsg += " (first defined at line " + std::to_string(prevLine) + ")";
        }
        error(func, errorMsg);
        return;
    }

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(func->typeSpec(), retDims);
    if (!retType) return;

    bool isVariadic = false;
    bool isTypedVariadic = false;
    const TypeInfo* variadicElemType = nullptr;
    size_t variadicIndex = 0;
    std::vector<const TypeInfo*> paramTypes;
    if (auto* paramList = func->paramList()) {
        auto params = paramList->param();
        for (size_t i = 0; i < params.size(); i++) {
            auto* param = params[i];

            // Untyped variadic: ...
            if (param->SPREAD() && !param->typeSpec()) {
                isVariadic = true;
                variadicIndex = i;
                break;
            }

            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) return;
            paramTypes.push_back(pType);

            if (param->SPREAD()) {
                // Typed variadic: T ...name
                isVariadic = true;
                isTypedVariadic = true;
                variadicElemType = pType;
                variadicIndex = i;
            }
        }
    }

    if (isVariadic) {
        if (!isTypedVariadic && paramTypes.empty())
            error(func, "untyped variadic function '" + funcName +
                        "' must have at least one fixed parameter before '...'");

        if (auto* paramList = func->paramList()) {
            auto params = paramList->param();
            if (variadicIndex < params.size() - 1)
                error(func, "'...' must be the last parameter in function '" +
                            funcName + "'");
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic, variadicElemType);
    functions_[funcName] = funcType;
    functionDecls_[funcName] = func;
    syncToSemanticDB_Function(funcName, *funcType, currentModulePath_, func);
}

// ═══════════════════════════════════════════════════════════════════════
//  Function body check
// ═══════════════════════════════════════════════════════════════════════

void Checker::checkFunction(LucisParser::FunctionDeclContext* func) {
    // Generic function templates are not checked directly — only their instantiations are.
    if (func->typeParamList()) return;

    // Comptime functions: register (if not already) and type-check the body
    if (func->COMPTIME()) {
        auto funcName = func->IDENTIFIER(0)->getText();
        if (!comptimeRegistry_.isComptime(funcName))
            comptimeRegistry_.registerFunction(funcName, func);
        // Fall through to type-check the body for error reporting
    }

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(func->typeSpec(), retDims);
    if (!retType) {
        // Use void as sentinel so the body still gets checked
        retType = typeRegistry_.lookup("void");
    }

    // Clear labels from previous function
    currentFunctionLabels_.clear();
    asmGotoLabelRefs_.clear();

    // Register parameters as locals (params are always initialized and used)
    if (auto* paramList = func->paramList()) {
        for (auto* param : paramList->param()) {
            // Skip untyped variadic ... (no identifier/type)
            if (param->SPREAD() && !param->IDENTIFIER())
                continue;

            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) continue;

            if (param->SPREAD()) {
                // Typed variadic: T ...name → register as []T slice
                pDims = 1;
            }

            if (locals_.count(paramName)) {
                error(param, "duplicate parameter name '" + paramName + "'");
                continue;
            }

            locals_[paramName] = {pType, pDims, {}, true, true, nullptr};
        }
    }

    auto* prevRet = currentReturnType_;
    currentReturnType_ = retType;
    checkBlock(func->block(), retType);
    currentReturnType_ = prevRet;

    // Validate asm goto label references (forward references are now resolved)
    for (auto& refName : asmGotoLabelRefs_) {
        if (!currentFunctionLabels_.count(refName))
            error(func, "undefined label '" + refName + "' in asm goto");
    }

    if (retType->kind != TypeKind::Void) {
        if (!blockAlwaysReturns(func->block())) {
            if (!func || func->IDENTIFIER().empty()) {
                error(func, "function must return a value of type '" + retType->name +
                            "' on all code paths");
            } else {
                error(func, "function '" + func->IDENTIFIER(0)->getText() +
                            "' must return a value of type '" + retType->name +
                            "' on all code paths");
            }
        }
    }

    // Warn about unused local variables
    warnUnusedLocals(func);
}

bool Checker::blockAlwaysReturns(LucisParser::BlockContext* block) {
    std::function<bool(LucisParser::IfBodyContext*)> ifBodyAlwaysReturns =
    [&](LucisParser::IfBodyContext* body) -> bool {
        if (!body) return false;
        if (auto* b = body->block())
            return blockAlwaysReturns(b);
        if (auto* s = body->statement()) {
            if (s->returnStmt() || s->throwStmt())
                return true;
            if (auto* ls = s->loopStmt())
                return blockAlwaysReturns(ls->block());
            if (auto* nestedIf = s->ifStmt()) {
                if (!nestedIf->elseClause()) return false;
                if (!ifBodyAlwaysReturns(nestedIf->ifBody())) return false;
                for (auto* eif : nestedIf->elseIfClause()) {
                    if (!ifBodyAlwaysReturns(eif->ifBody()))
                        return false;
                }
                return ifBodyAlwaysReturns(nestedIf->elseClause()->ifBody());
            }
        }
        return false;
    };

    auto stmts = block->statement();
    for (auto* stmt : stmts) {
        if (stmt->returnStmt()) {
            return true;
        }

        // throw always exits the current function
        if (stmt->throwStmt()) {
            return true;
        }

        // Detect noreturn function calls: panic(), exit(), unreachable()
        if (auto* cs = stmt->callStmt()) {
            auto name = cs->IDENTIFIER()->getText();
            if (name == "panic" || name == "exit" || name == "unreachable")
                return true;
        }
        if (auto* es = stmt->exprStmt()) {
            if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(es->expression())) {
                if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                    auto name = ident->IDENTIFIER()->getText();
                    if (name == "panic" || name == "exit" || name == "unreachable")
                        return true;
                }
            }
            // lucis::core::trap() is a noreturn intrinsic
            if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(es->expression())) {
                auto ids = smc->IDENTIFIER();
                std::vector<std::string> idTexts;
                for (auto* id : ids) idTexts.push_back(id->getText());
                std::string ns, funcName;
                if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                    if (ns == "core" && funcName == "trap")
                        return true;
                }
            }
        }

        // loop { body } — if the body always returns, the loop always returns
        if (auto* ls = stmt->loopStmt()) {
            if (blockAlwaysReturns(ls->block()))
                return true;
        }

        if (auto* ifS = stmt->ifStmt()) {
            // if/else-if/else chain: all branches must return
            if (!ifS->elseClause()) continue;

            bool ifReturns = ifBodyAlwaysReturns(ifS->ifBody());
            if (!ifReturns) continue;

            bool allElseIfsReturn = true;
            for (auto* elseIf : ifS->elseIfClause()) {
                if (!ifBodyAlwaysReturns(elseIf->ifBody())) {
                    allElseIfsReturn = false;
                    break;
                }
            }
            if (!allElseIfsReturn) continue;

            if (ifBodyAlwaysReturns(ifS->elseClause()->ifBody()))
                return true;
        }

        if (auto* sw = stmt->switchStmt()) {
            // switch with default: all cases + default must return
            if (!sw->defaultClause()) continue;

            bool allReturn = true;
            for (auto* cc : sw->caseClause()) {
                if (!blockAlwaysReturns(cc->block())) {
                    allReturn = false;
                    break;
                }
            }
            if (allReturn && blockAlwaysReturns(sw->defaultClause()->block()))
                return true;
        }

        // try/catch: if all catch blocks + try block return, the whole thing returns
        if (auto* tc = stmt->tryCatchStmt()) {
            bool tryReturns = blockAlwaysReturns(tc->block());
            if (tryReturns) {
                bool allCatchReturn = true;
                for (auto* cc : tc->catchClause()) {
                    if (!blockAlwaysReturns(cc->block())) {
                        allCatchReturn = false;
                        break;
                    }
                }
                if (allCatchReturn) {
                    // finally doesn't affect return — it always runs regardless
                    return true;
                }
            }
        }
    }
    return false;
}

bool Checker::isTerminatorStmt(LucisParser::StatementContext* stmt) {
    if (stmt->returnStmt()) return true;
    if (stmt->throwStmt())  return true;
    if (stmt->breakStmt())  return true;
    if (stmt->continueStmt()) return true;

    // Noreturn function calls: panic(), exit(), unreachable()
    if (auto* cs = stmt->callStmt()) {
        auto name = cs->IDENTIFIER()->getText();
        if (name == "panic" || name == "exit" || name == "unreachable")
            return true;
    }
    if (auto* es = stmt->exprStmt()) {
        if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(es->expression())) {
            if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
                auto name = ident->IDENTIFIER()->getText();
                if (name == "panic" || name == "exit" || name == "unreachable")
                    return true;
            }
        }
        // lucis::core::trap() is a noreturn intrinsic
        if (auto* smc = dynamic_cast<LucisParser::StaticMethodCallExprContext*>(es->expression())) {
            auto ids = smc->IDENTIFIER();
            std::vector<std::string> idTexts;
            for (auto* id : ids) idTexts.push_back(id->getText());
            std::string ns, funcName;
            if (IntrinsicRegistry::parseIntrinsicPath(idTexts, ns, funcName)) {
                if (ns == "core" && funcName == "trap")
                    return true;
            }
        }
    }
    return false;
}

void Checker::checkBlock(LucisParser::BlockContext* block,
                         const TypeInfo* retType,
                         std::unordered_set<std::string>* initCapture) {
    auto savedLocals = locals_;
    auto savedEnumImports = enumVariantImports_;
    ++scopeDepth_;
    bool terminated = false;
    for (auto* stmt : block->statement()) {
        if (terminated) {
            warning(stmt, "unreachable code");
            break;
        }
        checkStmt(stmt, retType, terminated);
    }
    --scopeDepth_;
    // Propagate 'used' flags to outer-scope variables before restoring.
    // Also capture newly-initialized outer-scope variables if requested.
    for (auto& [name, info] : savedLocals) {
        auto it = locals_.find(name);
        if (it != locals_.end()) {
            info.used = info.used || it->second.used;
            if (initCapture && !info.initialized && it->second.initialized)
                initCapture->insert(name);
        }
    }
    // Full restore: removes inner-scope variables and restores outer values.
    locals_ = savedLocals;
    enumVariantImports_ = savedEnumImports;
}

void Checker::checkStmt(LucisParser::StatementContext* stmt,
                        const TypeInfo* retType,
                        bool& terminated) {
    if (auto* ud = stmt->useDecl()) {
        if (auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(ud)) {
            unsigned arrayDims = 0;
            auto* enumType = resolveTypeSpec(ew->typeSpec(), arrayDims);
            if (!enumType) {
                error(ew, "unknown type in 'use' wildcard");
            } else if (enumType->kind != TypeKind::Enum) {
                error(ew, "type '" + enumType->name + "' is not an enum");
            } else if (arrayDims > 0) {
                error(ew, "cannot use array type in 'use' wildcard");
            } else {
                for (const auto& vi : enumType->enumVariantInfos) {
                    auto [it, inserted] = enumVariantImports_.try_emplace(
                        vi.name,
                        InjectedVariant{ enumType, &vi });
                    if (!inserted) {
                        error(ew, "ambiguous variant name '" + vi.name +
                                   "': already imported from '" +
                                   it->second.enumType->name + "'");
                    }
                }
            }
        }
        return;
    }

    if (auto* constDecl = stmt->constDeclStmt()) {
        checkConstDeclStmt(constDecl);
    } else if (auto* varDecl = stmt->varDeclStmt()) {
        checkVarDeclStmt(varDecl);
    } else if (auto* assign = stmt->assignStmt()) {
        checkAssignStmt(assign);
    } else if (auto* compound = stmt->compoundAssignStmt()) {
        checkCompoundAssignStmt(compound);
    } else if (auto* fieldCompound = stmt->fieldCompoundAssignStmt()) {
        checkFieldCompoundAssignStmt(fieldCompound);
    } else if (auto* fieldAssign = stmt->fieldAssignStmt()) {
        checkFieldAssignStmt(fieldAssign);
    } else if (auto* arrowAssign = stmt->arrowAssignStmt()) {
        checkArrowAssignStmt(arrowAssign);
    } else if (auto* arrowCompound = stmt->arrowCompoundAssignStmt()) {
        checkArrowCompoundAssignStmt(arrowCompound);
    } else if (auto* derefAssign = stmt->derefAssignStmt()) {
        checkDerefAssignStmt(derefAssign);
    } else if (auto* derefCompound = stmt->derefCompoundAssignStmt()) {
        checkDerefCompoundAssignStmt(derefCompound);
    } else if (auto* call = stmt->callStmt()) {
        checkCallStmt(call);
    } else if (auto* asmS = stmt->asmStmt()) {
        checkAsmStmt(asmS);
    } else if (auto* label = stmt->labelDef()) {
        auto name = label->IDENTIFIER()->getText();
        if (currentFunctionLabels_.count(name))
            error(label, "duplicate label '" + name + "'");
        else
            currentFunctionLabels_.insert(name);
        checkStmt(label->statement(), retType, terminated);
    } else if (auto* exprS = stmt->exprStmt()) {
        checkExprStmt(exprS);
    } else if (auto* ret = stmt->returnStmt()) {
        checkReturnStmt(ret, retType);
    } else if (auto* ifS = stmt->ifStmt()) {
        checkIfStmt(ifS, retType);
    } else if (auto* forS = stmt->forStmt()) {
        if (auto* forIn = dynamic_cast<LucisParser::ForInStmtContext*>(forS))
            checkForInStmt(forIn, retType);
        else if (auto* forC = dynamic_cast<LucisParser::ForClassicStmtContext*>(forS))
            checkForClassicStmt(forC, retType);
    } else if (stmt->loopStmt()) {
        loopDepth_++;
        checkBlock(stmt->loopStmt()->block(), retType);
        loopDepth_--;
    } else if (auto* ws = stmt->whileStmt()) {
        auto* wCondType = resolveExprType(ws->expression());
        if (wCondType && !isConditionType(wCondType))
            error(ws, "condition has type '" + wCondType->name +
                      "', expected 'bool' or numeric type");
        loopDepth_++;
        checkBlock(ws->block(), retType);
        loopDepth_--;
    } else if (auto* dw = stmt->doWhileStmt()) {
        loopDepth_++;
        checkBlock(dw->block(), retType);
        loopDepth_--;
        auto* dwCondType = resolveExprType(dw->expression());
        if (dwCondType && !isConditionType(dwCondType))
            error(dw, "condition has type '" + dwCondType->name +
                      "', expected 'bool' or numeric type");
    } else if (stmt->breakStmt()) {
        if (loopDepth_ == 0)
            error(stmt, "'break' used outside of a loop");
    } else if (stmt->continueStmt()) {
        if (loopDepth_ == 0)
            error(stmt, "'continue' used outside of a loop");
    } else if (auto* sw = stmt->switchStmt()) {
        checkSwitchStmt(sw, retType);
    } else if (auto* lk = stmt->lockStmt()) {
        resolveExprType(lk->expression());
        checkBlock(lk->block(), retType);
    } else if (auto* tc = stmt->tryCatchStmt()) {
        checkBlock(tc->block(), retType);
        for (auto* cc : tc->catchClause()) {
            auto catchVarName = cc->IDENTIFIER()->getText();
            unsigned catchDims = 0;
            auto* catchType = resolveTypeSpec(cc->typeSpec(), catchDims);
            auto prev = locals_.find(catchVarName);
            bool hadPrev = prev != locals_.end();
            VarInfo prevInfo;
            if (hadPrev) prevInfo = prev->second;
            if (catchType)
                locals_[catchVarName] = {catchType, catchDims, {}, true, true, nullptr};
            checkBlock(cc->block(), retType);
            if (hadPrev)
                locals_[catchVarName] = prevInfo;
            else
                locals_.erase(catchVarName);
        }
        if (auto* fin = tc->finallyClause()) {
            checkBlock(fin->block(), retType);
        }
    } else if (stmt->throwStmt()) {
        resolveExprType(stmt->throwStmt()->expression());
    } else if (auto* ifa = stmt->indexFieldAssignStmt()) {
        for (auto* e : ifa->expression()) {
            resolveExprType(e);
        }
    } else if (auto* fia = stmt->fieldIndexAssignStmt()) {
        for (auto* e : fia->expression()) {
            resolveExprType(e);
        }
    } else if (auto* def = stmt->deferStmt()) {
        if (def->callStmt())
            checkCallStmt(def->callStmt());
        else if (def->exprStmt())
            checkExprStmt(def->exprStmt());

    // ── Structural Blocks ──────────────────────────────────────────────

    } else if (auto* nb = stmt->nakedBlockStmt()) {
        // {} — lexical scope: variables declared inside do NOT leak out
        auto savedLocals = locals_;
        auto savedEnumImports = enumVariantImports_;
        ++scopeDepth_;
        bool innerTerminated = false;
        for (auto* inner : nb->statement()) {
            if (innerTerminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, innerTerminated);
        }
        --scopeDepth_;
        for (auto& [name, info] : savedLocals) {
            auto it = locals_.find(name);
            if (it != locals_.end()) info.used = info.used || it->second.used;
        }
        locals_ = savedLocals;
        enumVariantImports_ = savedEnumImports;

    } else if (auto* ib = stmt->inlineBlockStmt()) {
        // #inline {} — variables are injected into parent scope
        for (auto* inner : ib->statement()) {
            if (terminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, terminated);
        }

    } else if (auto* sb = stmt->scopeBlockStmt()) {
        // #scope (callbacks) {} — callbacks execute at scope exit, so they can
        // reference variables declared inside the body. Process body first, then
        // validate callbacks with body locals visible.
        auto savedLocals = locals_;
        auto savedEnumImports = enumVariantImports_;
        ++scopeDepth_;
        bool innerTerminated = false;
        for (auto* inner : sb->statement()) {
            if (innerTerminated) {
                warning(inner, "unreachable code");
                break;
            }
            checkStmt(inner, retType, innerTerminated);
        }
        // Validate callbacks with body locals in scope
        if (auto* cbs = sb->scopeCallbackList()) {
            for (auto* cb : cbs->scopeCallback()) {
                if (cb->DOT()) {
                    // dot-access: varName.methodName(args)
                    auto varName    = cb->IDENTIFIER().size() > 0 ? cb->IDENTIFIER(0)->getText() : "";
                    auto methodName = cb->IDENTIFIER().size() > 1 ? cb->IDENTIFIER(1)->getText() : "";
                    if (locals_.find(varName) == locals_.end())
                        warning(cb, "unknown variable '" + varName + "' in #scope callback");
                } else {
                    // plain call: funcName(args)
                    auto funcName = cb->IDENTIFIER(0)->getText();
                    if (!isKnownFunction(funcName) && !(cBindings_ && cBindings_->findFunction(funcName)))
                        warning(cb, "unknown function '" + funcName + "' in #scope callback");
                }
                if (cb->argList())
                    for (auto* arg : cb->argList()->expression())
                        resolveExprType(arg);
            }
        }
        --scopeDepth_;
        for (auto& [name, info] : savedLocals) {
            auto it = locals_.find(name);
            if (it != locals_.end()) info.used = info.used || it->second.used;
        }
        locals_ = savedLocals;
        enumVariantImports_ = savedEnumImports;

    } else if (auto* cm = stmt->cMacroBlock()) {
        checkCMacroBlock(cm);
    }

    if (isTerminatorStmt(stmt))
        terminated = true;
}

void Checker::checkSwitchStmt(LucisParser::SwitchStmtContext* stmt,
                              const TypeInfo* retType) {
    auto* exprType = resolveExprType(stmt->expression());
    if (exprType && !isInteger(exprType) && exprType->name != "char" &&
        exprType->kind != TypeKind::Enum) {
        error(stmt, "switch expression must be an integer, char or enum type, got '" +
              exprType->name + "'");
    }
    for (auto* cc : stmt->caseClause()) {
        for (auto* caseExpr : cc->expression()) {
            resolveExprType(caseExpr);
        }
        checkBlock(cc->block(), retType);
    }
    if (auto* dc = stmt->defaultClause()) {
        checkBlock(dc->block(), retType);
    }

    // Enum exhaustiveness check: warn if switch on enum has no default
    // and doesn't cover all variants
    if (exprType && exprType->kind == TypeKind::Enum && !stmt->defaultClause()) {
        std::unordered_set<std::string> coveredVariants;
        for (auto* cc : stmt->caseClause()) {
            for (auto* caseExpr : cc->expression()) {
                // Enum access: Enum::Variant
                if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(caseExpr)) {
                    auto ids = ea->IDENTIFIER();
                    if (ids.size() >= 2)
                        coveredVariants.insert(ids[1]->getText());
                }
            }
        }

        std::vector<std::string> missing;
        for (auto& variant : exprType->enumVariants) {
            if (!coveredVariants.count(variant))
                missing.push_back(variant);
        }

        if (!missing.empty()) {
            std::string msg = "switch on enum '" + exprType->name +
                "' is not exhaustive. Missing: ";
            for (size_t i = 0; i < missing.size(); i++) {
                if (i > 0) msg += ", ";
                msg += "'" + exprType->name + "::" + missing[i] + "'";
            }
            msg += ". Add a 'default' clause or handle all variants";
            warning(stmt, msg);
        }
    }
}

void Checker::checkIfStmt(LucisParser::IfStmtContext* stmt,
                           const TypeInfo* retType) {
    // Run a branch body and capture which outer-scope variables become initialized.
    auto checkIfBodyTracked = [&](LucisParser::IfBodyContext* body,
                                   std::unordered_set<std::string>& inited) {
        if (!body) return;
        if (auto* b = body->block()) {
            checkBlock(b, retType, &inited);
            return;
        }
        if (auto* s = body->statement()) {
            auto savedLocals = locals_;
            ++scopeDepth_;
            bool branchTerminated = false;
            checkStmt(s, retType, branchTerminated);
            --scopeDepth_;
            for (auto& [name, info] : savedLocals) {
                auto it = locals_.find(name);
                if (it != locals_.end()) {
                    info.used = info.used || it->second.used;
                    if (!info.initialized && it->second.initialized)
                        inited.insert(name);
                }
            }
            locals_ = savedLocals;
        }
    };

    // Extract is-binding from an expression (if it's `expr is Type::Variant(name)`).
    // Returns {bindingName, payloadTypeInfo} or {"", nullptr}.
    auto extractIsBinding = [&](LucisParser::ExpressionContext* condExpr)
            -> std::pair<std::string, const TypeInfo*> {
        auto* isE = dynamic_cast<LucisParser::IsExprContext*>(condExpr);
        if (!isE || !isE->SCOPE() || !isE->LPAREN() || !isE->IDENTIFIER(1))
            return {"", nullptr};
        auto variantName = isE->IDENTIFIER(0)->getText();
        unsigned dims = 0;
        auto* rhsType = resolveTypeSpec(isE->typeSpec(), dims);
        if (!rhsType || rhsType->kind != TypeKind::Enum) return {"", nullptr};
        auto* variantInfo = findEnumVariantInfo(rhsType, variantName);
        if (!variantInfo || variantInfo->payloadFields.empty()) return {"", nullptr};
        // Single-field tuple payload
        if (variantInfo->payloadKind == EnumPayloadKind::Tuple &&
            variantInfo->payloadFields.size() == 1) {
            return {isE->IDENTIFIER(1)->getText(),
                    variantInfo->payloadFields[0].typeInfo};
        }
        return {"", nullptr};
    };

    // Check the if condition
    auto* condType = resolveExprType(stmt->expression());
    if (condType && !isConditionType(condType))
        error(stmt, "condition has type '" + condType->name +
                    "', expected 'bool' or numeric type");

    // Collect init sets from each branch for flow-sensitive init analysis.
    // Only propagate if ALL branches (including else) initialize a variable.
    std::vector<std::unordered_set<std::string>> branchInits;

    // Check if-body
    auto [ifBindName, ifBindTI] = extractIsBinding(stmt->expression());
    if (!ifBindName.empty() && ifBindTI)
        locals_[ifBindName] = {ifBindTI, 0, {}, true, true, nullptr};
    branchInits.emplace_back();
    checkIfBodyTracked(stmt->ifBody(), branchInits.back());
    if (!ifBindName.empty()) locals_.erase(ifBindName);

    // Check else-if clauses
    for (auto* elseIf : stmt->elseIfClause()) {
        auto* eifCondType = resolveExprType(elseIf->expression());
        if (eifCondType && !isConditionType(eifCondType))
            error(elseIf, "condition has type '" + eifCondType->name +
                          "', expected 'bool' or numeric type");
        auto [eifBindName, eifBindTI] = extractIsBinding(elseIf->expression());
        if (!eifBindName.empty() && eifBindTI)
            locals_[eifBindName] = {eifBindTI, 0, {}, true, true, nullptr};
        branchInits.emplace_back();
        checkIfBodyTracked(elseIf->ifBody(), branchInits.back());
        if (!eifBindName.empty()) locals_.erase(eifBindName);
    }

    // Check else clause
    bool hasElse = (stmt->elseClause() != nullptr);
    if (hasElse) {
        branchInits.emplace_back();
        checkIfBodyTracked(stmt->elseClause()->ifBody(), branchInits.back());
    }

    // Flow-sensitive init propagation: if all branches (with else = full coverage)
    // initialize a variable, mark it initialized in the outer scope.
    if (hasElse && !branchInits.empty()) {
        // Start with the first branch's init set, intersect with all others.
        std::unordered_set<std::string> intersection = branchInits[0];
        for (size_t bi = 1; bi < branchInits.size(); ++bi) {
            for (auto it = intersection.begin(); it != intersection.end(); ) {
                if (!branchInits[bi].count(*it))
                    it = intersection.erase(it);
                else
                    ++it;
            }
        }
        for (const auto& name : intersection) {
            auto lit = locals_.find(name);
            if (lit != locals_.end())
                lit->second.initialized = true;
        }
    }
}

void Checker::checkForInStmt(LucisParser::ForInStmtContext* stmt,
                              const TypeInfo* retType) {
    unsigned dims = 0;
    auto* iterType = [&]() -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, dims);
        return resolveTypeSpec(stmt->typeSpec(), dims);
    }();
    auto iterName = stmt->IDENTIFIER()->getText();

    auto* iterableType = resolveExprType(stmt->expression());

    // Range expressions (start..end, start..=end) are always valid for iteration
    bool isRange = dynamic_cast<LucisParser::RangeExprContext*>(stmt->expression()) ||
                   dynamic_cast<LucisParser::RangeInclExprContext*>(stmt->expression());

    // Validate the iterable expression type
    if (!isRange && iterableType) {
        bool isValid = false;

        if (iterableType->kind == TypeKind::Extended) {
            // Collection iteration: Vec<T>, Map<K,V>, Set<T>
            isValid = true;
        } else if (iterableType->kind == TypeKind::Integer) {
            // Integer range iteration: for TYPE i in N
            if (iterType && iterType->kind != TypeKind::Integer) {
                error(stmt, "cannot iterate over integer '" + iterableType->name +
                      "' with loop variable of type '" + iterType->name +
                      "', expected an integer type");
            }
            isValid = true;
        }

        if (!isValid) {
            error(stmt, "cannot iterate over type '" + iterableType->name +
                  "', expected a Vec, Map, Set, or integer range");
        }
    }

    // Register the loop variable
    if (iterType)
        locals_[iterName] = {iterType, dims, {}, true, true, nullptr};

    loopDepth_++;
    checkBlock(stmt->block(), retType);
    loopDepth_--;

    locals_.erase(iterName);
}

void Checker::checkForClassicStmt(LucisParser::ForClassicStmtContext* stmt,
                                   const TypeInfo* retType) {
    unsigned dims = 0;
    auto* varType = [&]() -> const TypeInfo* {
        if (!activeTypeSubst_.empty())
            return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, dims);
        return resolveTypeSpec(stmt->typeSpec(), dims);
    }();
    auto varName = stmt->IDENTIFIER()->getText();

    // Check init expression
    auto exprs = stmt->expression();
    auto* initType = resolveExprType(exprs[0]);

    if (varType && initType && !isAssignable(varType, initType))
        error(stmt, "for init type mismatch: expected '" +
                         varType->name + "', got '" + initType->name + "'");

    // Register the loop variable
    if (varType)
        locals_[varName] = {varType, dims, {}, true, true, nullptr};

    // Check condition expression
    auto* condType = resolveExprType(exprs[1]);
    if (condType && !isConditionType(condType))
        error(stmt, "for condition has type '" + condType->name +
                    "', expected 'bool' or numeric type");

    // Check update expression
    resolveExprType(exprs[2]);

    loopDepth_++;
    checkBlock(stmt->block(), retType);
    loopDepth_--;

    locals_.erase(varName);
}

// ═══════════════════════════════════════════════════════════════════════
//  Statement checks
// ═══════════════════════════════════════════════════════════════════════

// ── Const expression evaluator ─────────────────────────────────────────────
// Walks the AST and computes the compile-time value of a const expression.
// Returns std::nullopt if the expression cannot be evaluated at compile time.
static int64_t parseIntLiteral(const std::string& text, int base = 0) {
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
        if (c == '\'' && base != 16) continue; // digit separators (exclude hex 'a'..'f')
        clean += c;
    }
    auto pos = clean.find_first_not_of("+-xXoObB");
    if (pos == std::string::npos) return 0;
    std::string num = clean.substr(pos);
    try {
        return std::stoll(num, nullptr, base);
    } catch (...) { return 0; }
}

static std::string stripSuffix(const std::string& text) {
    static const char* suffixes[] = {
        "i8","i16","i32","i64","i128","iinf","isize",
        "u8","u16","u32","u64","u128","usize",
        "f32","f64","f80","f128", nullptr
    };
    for (const char** s = suffixes; *s; ++s) {
        auto slen = strlen(*s);
        if (text.size() > slen && text.compare(text.size() - slen, slen, *s) == 0)
            return text.substr(0, text.size() - slen);
    }
    return text;
}

std::optional<ComptimeValue> Checker::evaluateConstExpr(LucisParser::ExpressionContext* expr) {
    if (!expr) return std::nullopt;

    // ── Unsuffixed integer literals ────────────────────────────────
    auto evalIntLit = [](const std::string& text, int base) -> std::optional<ComptimeValue> {
        return ComptimeValue::intVal(parseIntLiteral(text, base));
    };

    if (auto* n = dynamic_cast<LucisParser::IntLitExprContext*>(expr))
        return evalIntLit(n->INT_LIT()->getText(), 0);
    if (auto* n = dynamic_cast<LucisParser::HexLitExprContext*>(expr))
        return evalIntLit(n->HEX_LIT()->getText(), 16);
    if (auto* n = dynamic_cast<LucisParser::OctLitExprContext*>(expr))
        return evalIntLit(n->OCT_LIT()->getText(), 8);
    if (auto* n = dynamic_cast<LucisParser::BinLitExprContext*>(expr))
        return evalIntLit(n->BIN_LIT()->getText(), 2);

    // ── Suffixed integer literals ───────────────────────────────────
    if (auto* n = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_INT()->getText()), 0));
    if (auto* n = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_HEX()->getText()), 16));
    if (auto* n = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_OCT()->getText()), 8));
    if (auto* n = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr))
        return ComptimeValue::intVal(parseIntLiteral(stripSuffix(n->SUFFIXED_BIN()->getText()), 2));

    // ── Float literals ──────────────────────────────────────────────
    auto evalFloatLit = [](const std::string& text) -> std::optional<ComptimeValue> {
        try { return ComptimeValue::floatVal(std::stod(text)); } catch (...) { return std::nullopt; }
    };
    if (dynamic_cast<LucisParser::FloatLitExprContext*>(expr)) {
        auto raw = static_cast<LucisParser::FloatLitExprContext*>(expr)->FLOAT_LIT()->getText();
        return evalFloatLit(raw);
    }
    if (auto* ld = dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr)) {
        if (ld->INT_LIT()) {
            return evalFloatLit("0." + ld->INT_LIT()->getText());
        }
        return evalFloatLit("0.0");
    }
    if (auto* n = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_FLOAT()->getText()));
    if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr)) {
        auto raw = sd->SUFFIXED_DOT_FLOAT()->getText();
        return evalFloatLit("0." + stripSuffix(raw));
    }
    if (auto* n = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_FLOAT_INT()->getText()));
    if (auto* n = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr))
        return evalFloatLit(stripSuffix(n->SUFFIXED_INT_FLOAT()->getText()));

    // ── Bool, char, null literals ───────────────────────────────────
    if (dynamic_cast<LucisParser::BoolLitExprContext*>(expr)) {
        auto raw = static_cast<LucisParser::BoolLitExprContext*>(expr)->BOOL_LIT()->getText();
        return ComptimeValue::boolVal(raw == "true");
    }
    if (auto* ch = dynamic_cast<LucisParser::CharLitExprContext*>(expr)) {
        auto raw = ch->CHAR_LIT()->getText();
        if (raw.size() >= 2 && raw[0] == '\'') {
            if (raw.size() == 3 && raw[1] != '\\')
                return ComptimeValue::intVal(static_cast<uint8_t>(raw[1]));
            if (raw.size() > 2 && raw[1] == '\\') {
                if (raw[2] == 'n') return ComptimeValue::intVal(10);
                if (raw[2] == 't') return ComptimeValue::intVal(9);
                if (raw[2] == '0') return ComptimeValue::intVal(0);
                if (raw[2] == '\\') return ComptimeValue::intVal(92);
                if (raw[2] == '\'') return ComptimeValue::intVal(39);
                return ComptimeValue::intVal(static_cast<uint8_t>(raw[2]));
            }
        }
        return ComptimeValue::intVal(0);
    }
    if (dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return ComptimeValue::intVal(0);

    // ── String literals ─────────────────────────────────────────────
    if (auto* s = dynamic_cast<LucisParser::StrLitExprContext*>(expr)) {
        auto raw = s->STR_LIT()->getText();
        if (raw.size() >= 2) {
            std::string unescaped;
            for (size_t i = 1; i < raw.size() - 1; i++) {
                if (raw[i] == '\\' && i + 1 < raw.size() - 1) {
                    switch (raw[++i]) {
                        case 'n': unescaped += '\n'; break;
                        case 't': unescaped += '\t'; break;
                        case '0': unescaped += '\0'; break;
                        case '\\': unescaped += '\\'; break;
                        case '"': unescaped += '"'; break;
                        default: unescaped += raw[i]; break;
                    }
                } else {
                    unescaped += raw[i];
                }
            }
            return ComptimeValue::stringVal(unescaped);
        }
        return ComptimeValue::stringVal("");
    }

    // ── Identifier reference (other const) ──────────────────────────
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        auto it = compileTimeValues_.find(name);
        if (it != compileTimeValues_.end())
            return it->second;
        return std::nullopt;
    }

    // ── Parenthesized ───────────────────────────────────────────────
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return evaluateConstExpr(paren->expression());

    // ── Unary operators ─────────────────────────────────────────────
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr)) {
        auto v = evaluateConstExpr(neg->expression());
        if (!v) return std::nullopt;
        if (v->kind() == ComptimeValue::Kind::Int) return ComptimeValue::intVal(-v->asInt());
        if (v->kind() == ComptimeValue::Kind::Float) return ComptimeValue::floatVal(-v->asFloat());
        return std::nullopt;
    }
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr)) {
        auto v = evaluateConstExpr(lnot->expression());
        if (!v) return std::nullopt;
        if (v->kind() == ComptimeValue::Kind::Bool) return ComptimeValue::boolVal(!v->asBool());
        if (v->kind() == ComptimeValue::Kind::Int) return ComptimeValue::boolVal(!v->asInt());
        return std::nullopt;
    }
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr)) {
        auto v = evaluateConstExpr(bnot->expression());
        if (!v || v->kind() != ComptimeValue::Kind::Int) return std::nullopt;
        return ComptimeValue::intVal(~v->asInt());
    }

    // ── Binary operators ────────────────────────────────────────────
    auto evalBinary = [this](auto* ctx, auto opFn) -> std::optional<ComptimeValue> {
        auto sub = ctx->expression();
        if (sub.size() != 2) return std::nullopt;
        auto lhs = evaluateConstExpr(sub[0]);
        auto rhs = evaluateConstExpr(sub[1]);
        if (!lhs || !rhs) return std::nullopt;
        return opFn(*lhs, *rhs);
    };

    // Arithmetic
    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr)) {
        return evalBinary(mul, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                // Determine op from token text
                auto raw = mul->getText();
                if (raw.find('*') != std::string::npos) return ComptimeValue::intVal(a.asInt() * b.asInt());
                if (raw.find('/') != std::string::npos) {
                    if (b.asInt() == 0) return std::nullopt;
                    return ComptimeValue::intVal(a.asInt() / b.asInt());
                }
                if (raw.find('%') != std::string::npos) {
                    if (b.asInt() == 0) return std::nullopt;
                    return ComptimeValue::intVal(a.asInt() % b.asInt());
                }
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                auto raw = mul->getText();
                if (raw.find('*') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() * b.asFloat());
                if (raw.find('/') != std::string::npos) {
                    if (b.asFloat() == 0.0) return std::nullopt;
                    return ComptimeValue::floatVal(a.asFloat() / b.asFloat());
                }
                return std::nullopt;
            }
            return std::nullopt;
        });
    }

    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr)) {
        return evalBinary(add, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                auto raw = add->getText();
                if (raw.find('+') != std::string::npos) return ComptimeValue::intVal(a.asInt() + b.asInt());
                if (raw.find('-') != std::string::npos) return ComptimeValue::intVal(a.asInt() - b.asInt());
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                auto raw = add->getText();
                if (raw.find('+') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() + b.asFloat());
                if (raw.find('-') != std::string::npos) return ComptimeValue::floatVal(a.asFloat() - b.asFloat());
                return std::nullopt;
            }
            if (a.kind() == ComptimeValue::Kind::String && b.kind() == ComptimeValue::Kind::String) {
                return ComptimeValue::stringVal(a.asString() + b.asString());
            }
            return std::nullopt;
        });
    }

    // Shift
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr)) {
        return evalBinary(lsh, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() << b.asInt());
        });
    }
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr)) {
        return evalBinary(rsh, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            // Use logical shift for unsigned, arithmetic for signed
            // We default to arithmetic (signed) for simplicity
            return ComptimeValue::intVal(a.asInt() >> b.asInt());
        });
    }

    // Relational
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        return evalBinary(rel, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            bool result = false;
            auto raw = rel->getText();
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int) {
                if (raw.find("<=") != std::string::npos) result = a.asInt() <= b.asInt();
                else if (raw.find(">=") != std::string::npos) result = a.asInt() >= b.asInt();
                else if (raw.find('<') != std::string::npos) result = a.asInt() < b.asInt();
                else if (raw.find('>') != std::string::npos) result = a.asInt() > b.asInt();
                return ComptimeValue::boolVal(result);
            }
            if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float) {
                if (raw.find("<=") != std::string::npos) result = a.asFloat() <= b.asFloat();
                else if (raw.find(">=") != std::string::npos) result = a.asFloat() >= b.asFloat();
                else if (raw.find('<') != std::string::npos) result = a.asFloat() < b.asFloat();
                else if (raw.find('>') != std::string::npos) result = a.asFloat() > b.asFloat();
                return ComptimeValue::boolVal(result);
            }
            return std::nullopt;
        });
    }

    // Equality
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        return evalBinary(eq, [&](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            bool result = false;
            auto raw = eq->getText();
            bool isEq = raw.find("==") != std::string::npos;
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                result = (a.asInt() == b.asInt());
            else if (a.kind() == ComptimeValue::Kind::Float && b.kind() == ComptimeValue::Kind::Float)
                result = (a.asFloat() == b.asFloat());
            else if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                result = (a.asBool() == b.asBool());
            else if (a.kind() == ComptimeValue::Kind::String && b.kind() == ComptimeValue::Kind::String)
                result = (a.asString() == b.asString());
            else return std::nullopt;
            return ComptimeValue::boolVal(isEq ? result : !result);
        });
    }

    // Bitwise
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr)) {
        return evalBinary(ba, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() & b.asInt());
        });
    }
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr)) {
        return evalBinary(bx, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() ^ b.asInt());
        });
    }
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr)) {
        return evalBinary(bo, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() != ComptimeValue::Kind::Int || b.kind() != ComptimeValue::Kind::Int)
                return std::nullopt;
            return ComptimeValue::intVal(a.asInt() | b.asInt());
        });
    }

    // Logical
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        return evalBinary(la, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                return ComptimeValue::boolVal(a.asBool() && b.asBool());
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                return ComptimeValue::boolVal(a.asInt() && b.asInt());
            return std::nullopt;
        });
    }
    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        return evalBinary(lo, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Bool && b.kind() == ComptimeValue::Kind::Bool)
                return ComptimeValue::boolVal(a.asBool() || b.asBool());
            if (a.kind() == ComptimeValue::Kind::Int && b.kind() == ComptimeValue::Kind::Int)
                return ComptimeValue::boolVal(a.asInt() || b.asInt());
            return std::nullopt;
        });
    }
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr)) {
        return evalBinary(nc, [](const ComptimeValue& a, const ComptimeValue& b)
                         -> std::optional<ComptimeValue> {
            if (a.kind() == ComptimeValue::Kind::Int)
                return a.asInt() != 0 ? a : b;
            if (a.kind() == ComptimeValue::Kind::Bool)
                return a.asBool() ? a : b;
            return b;
        });
    }

    // ── Ternary ─────────────────────────────────────────────────────
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto sub = tern->expression();
        if (sub.size() != 3) return std::nullopt;
        auto cond = evaluateConstExpr(sub[0]);
        if (!cond) return std::nullopt;
        bool isTrue = (cond->kind() == ComptimeValue::Kind::Bool) ? cond->asBool()
                    : (cond->kind() == ComptimeValue::Kind::Int) ? (cond->asInt() != 0)
                    : false;
        return evaluateConstExpr(isTrue ? sub[1] : sub[2]);
    }

    // ── sizeof / typeof ─────────────────────────────────────────────
    if (auto* sz = dynamic_cast<LucisParser::SizeofExprContext*>(expr)) {
        if (auto* ts = sz->typeSpec()) {
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(ts, dims);
            if (!ti) return std::nullopt;
            // Estimate size from bitWidth
            if (ti->bitWidth > 0)
                return ComptimeValue::intVal(ti->bitWidth / 8);
            if (ti->kind == TypeKind::Pointer)
                return ComptimeValue::intVal(8); // assume 64-bit pointer
            return ComptimeValue::intVal(1);
        }
    }

    // ── Enum access: EnumType::Variant ──────────────────────────────
    auto findEnumVariant = [&](const TypeInfo* enumTI,
                               const std::string& varName) -> std::optional<ComptimeValue> {
        if (!enumTI || enumTI->kind != TypeKind::Enum) return std::nullopt;
        for (auto& v : enumTI->enumVariantInfos) {
            if (v.name == varName)
                return ComptimeValue::intVal(static_cast<int64_t>(v.discriminant));
        }
        return std::nullopt;
    };
    if (auto* ea = dynamic_cast<LucisParser::EnumAccessExprContext*>(expr)) {
        auto ids = ea->IDENTIFIER();
        if (ids.size() >= 2) {
            return findEnumVariant(typeRegistry_.lookup(ids[0]->getText()),
                                   ids[1]->getText());
        }
        return std::nullopt;
    }
    if (auto* gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr)) {
        auto ids = gea->IDENTIFIER();
        if (ids.size() >= 2) {
            return findEnumVariant(typeRegistry_.lookup(ids[0]->getText()),
                                   ids.back()->getText());
        }
        return std::nullopt;
    }

    return std::nullopt;
}

bool Checker::isValidConstExpr(LucisParser::ExpressionContext* expr) {
    // Literals — always valid
    if (dynamic_cast<LucisParser::IntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::HexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::OctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::BinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr) ||
        dynamic_cast<LucisParser::FloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr) ||
        dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr) ||
        dynamic_cast<LucisParser::BoolLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::CharLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::StrLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::CStrLitExprContext*>(expr) ||
        dynamic_cast<LucisParser::NullLitExprContext*>(expr))
        return true;

    // Identifier reference (other const)
    if (dynamic_cast<LucisParser::IdentExprContext*>(expr))
        return true;

    // Enum variant without payload
    if (dynamic_cast<LucisParser::EnumAccessExprContext*>(expr) ||
        dynamic_cast<LucisParser::GenericEnumAccessExprContext*>(expr))
        return true;

    // Parenthesized
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return isValidConstExpr(paren->expression());

    // Unary operators
    if (auto* neg = dynamic_cast<LucisParser::NegExprContext*>(expr))
        return isValidConstExpr(neg->expression());
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr))
        return isValidConstExpr(lnot->expression());
    if (auto* bnot = dynamic_cast<LucisParser::BitNotExprContext*>(expr))
        return isValidConstExpr(bnot->expression());

    // Function call to a comptime function — allowed
    if (auto* call = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(call->expression())) {
            auto calleeName = ident->IDENTIFIER()->getText();
            if (comptimeRegistry_.isComptime(calleeName)) {
                // Also validate all arguments
                if (auto* argList = call->argList()) {
                    for (auto* arg : argList->expression()) {
                        if (!isValidConstExpr(arg)) return false;
                    }
                }
                return true;
            }
        }
        return false;
    }

    // Binary expressions
    auto checkBinary = [this](auto* ctx) -> bool {
        if (!ctx || ctx->expression().size() != 2) return false;
        return isValidConstExpr(ctx->expression()[0]) &&
               isValidConstExpr(ctx->expression()[1]);
    };

    if (auto* mul = dynamic_cast<LucisParser::MulExprContext*>(expr))
        return checkBinary(mul);
    if (auto* add = dynamic_cast<LucisParser::AddSubExprContext*>(expr))
        return checkBinary(add);
    if (auto* lsh = dynamic_cast<LucisParser::LshiftExprContext*>(expr))
        return checkBinary(lsh);
    if (auto* rsh = dynamic_cast<LucisParser::RshiftExprContext*>(expr))
        return checkBinary(rsh);
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr))
        return checkBinary(rel);
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr))
        return checkBinary(eq);
    if (auto* ba = dynamic_cast<LucisParser::BitAndExprContext*>(expr))
        return checkBinary(ba);
    if (auto* bx = dynamic_cast<LucisParser::BitXorExprContext*>(expr))
        return checkBinary(bx);
    if (auto* bo = dynamic_cast<LucisParser::BitOrExprContext*>(expr))
        return checkBinary(bo);
    if (auto* la = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr))
        return checkBinary(la);
    if (auto* lo = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr))
        return checkBinary(lo);
    if (auto* nc = dynamic_cast<LucisParser::NullCoalExprContext*>(expr))
        return checkBinary(nc);

    // Ternary expression — all three sub-expressions must be valid
    if (auto* tern = dynamic_cast<LucisParser::TernaryExprContext*>(expr)) {
        auto sub = tern->expression();
        return sub.size() == 3 &&
               isValidConstExpr(sub[0]) &&
               isValidConstExpr(sub[1]) &&
               isValidConstExpr(sub[2]);
    }

    // sizeof / typeof — always valid
    if (dynamic_cast<LucisParser::SizeofExprContext*>(expr) ||
        dynamic_cast<LucisParser::TypeofExprContext*>(expr))
        return true;

    // Struct/union positional init: Point { 10, 20 }
    if (auto* spi = dynamic_cast<LucisParser::StructPosInitExprContext*>(expr)) {
        for (auto* e : spi->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Struct/union named init: Point { x: 10, y: 20 }
    if (auto* sl = dynamic_cast<LucisParser::StructLitExprContext*>(expr)) {
        for (auto* e : sl->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Qualified positional init: Namespace::Point { 10, 20 }
    if (auto* qspi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext*>(expr)) {
        for (auto* e : qspi->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    // Qualified named init: Namespace::Point { x: 10, y: 20 }
    if (auto* qsn = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext*>(expr)) {
        for (auto* e : qsn->expression())
            if (!isValidConstExpr(e)) return false;
        return true;
    }

    return false;
}

// ── Attribute validation ────────────────────────────────────────

void Checker::validateAttributeList(AttributeListContext* attrs, const std::string& contextName) {
    if (!attrs) return;
    for (auto* a : attrs->attribute()) {
        auto attrName = a->IDENTIFIER()->getText();
        if (!attrRegistry_.isKnown(attrName)) {
            error(a, "unknown attribute '" + attrName + "'");
            continue;
        }
        // TODO: validate arguments per-attribute type via registry handler
        auto* handler = attrRegistry_.lookup(attrName);
        if (handler && handler->validate) {
            // Build Attribute struct from AST node for validation
            Attribute attr;
            attr.name = attrName;
            attr.line = a->getStart() ? a->getStart()->getLine() : 0;
            attr.col  = a->getStart() ? a->getStart()->getCharPositionInLine() : 0;
            if (auto* argList = a->attrArgList()) {
                for (auto* arg : argList->attrArg()) {
                    if (arg->IDENTIFIER())
                        attr.args.push_back({AttributeArg::Ident, arg->getText(), "", 0, 0.0});
                    else if (arg->STR_LIT() || arg->C_STR_LIT())
                        attr.args.push_back({AttributeArg::String, "", arg->getText(), 0, 0.0});
                    else if (arg->INT_LIT() || arg->HEX_LIT() || arg->OCT_LIT() || arg->BIN_LIT())
                        attr.args.push_back({AttributeArg::Int, "", "", std::stoll(arg->getText()), 0.0});
                    else if (arg->FLOAT_LIT())
                        attr.args.push_back({AttributeArg::Float, "", "", 0, std::stod(arg->getText())});
                    else if (arg->BOOL_LIT())
                        attr.args.push_back({AttributeArg::Int, "", "", arg->getText() == "true" ? 1 : 0, 0.0});
                }
            }
            std::vector<std::string> errors;
            if (!handler->validate(attr, nullptr, errors)) {
                for (auto& err : errors)
                    error(a, "invalid attribute '" + attrName + "': " + err);
                if (errors.empty())
                    error(a, "invalid attribute '" + attrName + "'");
            }
        }
    }
}

bool Checker::hasAttribute(AttributeListContext* attrs, const std::string& name) const {
    if (!attrs) return false;
    for (auto* a : attrs->attribute()) {
        if (a->IDENTIFIER()->getText() == name)
            return true;
    }
    return false;
}

void Checker::checkConstDeclStmt(LucisParser::ConstDeclStmtContext* stmt) {
    auto decls = stmt->constDeclarator();
    if (decls.empty()) return;

    for (auto* d : decls) {
        auto name = d->IDENTIFIER()->getText();
        auto it = locals_.find(name);

        // Function-scoped const: register if not already in locals_
        bool isFunctionScoped = (it == locals_.end());
        if (isFunctionScoped) {
            VarInfo vi{typeRegistry_.lookup("int32"), 0, {}, true, false, nullptr};
            vi.isConst = true;
            vi.scopeDepth = scopeDepth_;
            vi.declToken = d->IDENTIFIER()->getSymbol();
            locals_[name] = vi;
            it = locals_.find(name);
        }

        // Validate that the initializer is a constant expression
        if (d->expression() && !isValidConstExpr(d->expression())) {
            error(d->expression(), "constant '" + name + "' initializer must be a compile-time constant expression");
            continue;
        }

        if (d->COLON() && d->typeSpec()) {
            // Explicit type: const NAME: TYPE = VALUE;
            unsigned dims = 0;
            auto* ti = resolveTypeSpec(d->typeSpec(), dims);
            if (!ti) continue;

            if (d->expression()) {
                auto* initTI = resolveExprType(d->expression());
                if (initTI) {
                    if (ti->kind == TypeKind::Integer && initTI->kind == TypeKind::Integer) {
                        // allow int-to-int coercions
                    } else if (ti->name != initTI->name) {
                        error(d, "type mismatch: cannot initialize '" + name +
                                 "' of type '" + ti->name + "' with expression of type '" +
                                 initTI->name + "'");
                        continue;
                    }
                }
            }
            it->second.type = ti;
            it->second.arrayDims = dims;
            it->second.initialized = (d->expression() != nullptr);
            it->second.declToken = d->IDENTIFIER()->getSymbol();
            if (!isFunctionScoped)
                globalVars_[name] = it->second;
        } else if (d->expression()) {
            // Auto type: const NAME = VALUE;
            auto* initTI = resolveExprType(d->expression());
            if (!initTI || initTI->kind == TypeKind::Void) {
                error(d, "cannot infer type for constant '" + name + "'");
                continue;
            }

            unsigned dims = 0;
            if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression())) {
                if (!arrLit->expression().empty()) dims = 1;
            } else {
                dims = resolveExprArrayDims(d->expression());
            }

            it->second.type = initTI;
            it->second.arrayDims = dims;
            it->second.initialized = true;
            it->second.declToken = d->IDENTIFIER()->getSymbol();
            if (!isFunctionScoped)
                globalVars_[name] = it->second;
        }

        // Store compile-time evaluated value for use in array dimensions and other const exprs
        if (d->expression()) {
            auto ev = evaluateConstExpr(d->expression());
            if (ev) {
                compileTimeValues_[name] = *ev;
            }
        }
    }
}

void Checker::checkVarDeclStmt(LucisParser::VarDeclStmtContext* stmt) {
    // ── Tuple destructuring: auto (x, y) = expr; ─────────────────────
    if (stmt->LPAREN()) {
        auto ids = stmt->IDENTIFIER();
        if (!stmt->expression()) {
            error(stmt, "tuple destructuring requires an initializer");
            return;
        }
        auto* initType = resolveExprType(stmt->expression());
        if (!initType || initType->kind != TypeKind::Tuple) {
            error(stmt, "tuple destructuring requires a tuple expression");
            return;
        }
        if (ids.size() != initType->tupleElements.size()) {
            error(stmt, "tuple destructuring expects " +
                        std::to_string(initType->tupleElements.size()) +
                        " variables, got " + std::to_string(ids.size()));
            return;
        }
        for (size_t i = 0; i < ids.size(); i++) {
            auto varName = ids[i]->getText();
            auto it = locals_.find(varName);
            if (it != locals_.end() && it->second.scopeDepth == scopeDepth_) {
                error(stmt, "variable '" + varName + "' already declared in this scope");
                continue;
            }
            VarInfo vi{initType->tupleElements[i], 0, {}, true, false, nullptr};
            vi.declToken = ids[i]->getSymbol();
            vi.scopeDepth = scopeDepth_;
            updateOwnershipOnInitialization(vi, stmt->expression());
            locals_[varName] = vi;
        }
        markExprAsMoved(stmt->expression(), stmt);
        return;
    }

    // ── Detect module prefix (qualified type: LIB::Point) ────────
    bool hasNsPrefix = stmt->SCOPE() != nullptr;
    std::string nsPrefix = hasNsPrefix && stmt->IDENTIFIER().size() > 0
        ? stmt->IDENTIFIER(0)->getText() : "";

    // ── Detect auto type ─────────────────────────────────────────────
    bool isAutoType = stmt->typeSpec() && stmt->typeSpec()->AUTO() != nullptr;

    // ── Resolve type for explicit types ──────────────────────────────
    const TypeInfo* typeInfo = nullptr;
    unsigned arrayDims = 0;
    if (!isAutoType) {
        typeInfo = [&]() -> const TypeInfo* {
            if (!activeTypeSubst_.empty())
                return resolveTypeSpecWithSubst(stmt->typeSpec(), activeTypeSubst_, arrayDims);
            if (hasNsPrefix) {
                auto* ts = stmt->typeSpec();
                if (ts && ts->IDENTIFIER()) {
                    auto typeName = ts->IDENTIFIER()->getText();
                    if (moduleRegistry_) {
                        auto* sym = moduleRegistry_->findSymbol(nsPrefix, typeName);
                        if (!sym) {
                            error(stmt, "'" + nsPrefix + "::" + typeName + "' is not exported");
                            return nullptr;
                        }
                    }
                    auto* ti = typeRegistry_.lookup(typeName);
                    if (!ti) {
                        error(stmt, "type '" + typeName + "' from module '" + nsPrefix +
                                     "' is not imported; add 'use " + nsPrefix + "::" + typeName + ";'");
                        return nullptr;
                    }
                    return ti;
                }
            }
            return resolveTypeSpec(stmt->typeSpec(), arrayDims);
        }();
        if (!typeInfo) return;
    }

    // ── Iterate varDeclarators ──────────────────────────────────────
    auto decls = stmt->varDeclarator();
    if (decls.empty()) return;

    // Find the last declarator that has an initializer (propagation source)
    LucisParser::VarDeclaratorContext* lastInitDecl = nullptr;
    for (auto it = decls.rbegin(); it != decls.rend(); ++it) {
        if ((*it)->expression()) { lastInitDecl = *it; break; }
    }

    // For auto, infer type from the first declarator that has an init
    if (isAutoType && !typeInfo) {
        for (auto* d : decls) {
            if (d->expression()) {
                auto* initType = resolveExprType(d->expression());
                if (!initType || initType->kind == TypeKind::Void) {
                    error(d, "cannot infer auto type for variable '" +
                             d->IDENTIFIER()->getText() + "'");
                    return;
                }
                typeInfo = initType;
                arrayDims = resolveExprArrayDims(d->expression());
                break;
            }
        }
        if (!typeInfo) {
            error(stmt, "type 'auto' requires at least one initializer expression");
            return;
        }
    }

    for (auto* d : decls) {
        auto varName = d->IDENTIFIER()->getText();

        // Check redeclaration in same scope
        auto it = locals_.find(varName);
        if (it != locals_.end() && it->second.scopeDepth == scopeDepth_) {
            error(d, "variable '" + varName + "' already declared in this scope");
            continue;
        }

        if (isAutoType) {
            // auto: use own init, or propagate from last init
            if (!d->expression()) {
                if (!lastInitDecl) {
                    error(d, "type 'auto' requires an initializer for '" + varName + "'");
                    continue;
                }
std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{typeInfo, arrayDims, arraySizes, true, false, nullptr};
                vi.declToken = d->IDENTIFIER()->getSymbol();
                vi.scopeDepth = scopeDepth_;
                updateOwnershipOnInitialization(vi, lastInitDecl->expression());
                locals_[varName] = vi;
                continue;
            }
            auto* initType = resolveExprType(d->expression());
            if (dynamic_cast<LucisParser::RangeExprContext*>(d->expression()) ||
                dynamic_cast<LucisParser::RangeInclExprContext*>(d->expression())) {
                error(d, "cannot infer type for '" + varName +
                         "': range expression has no concrete type");
                continue;
            }
            if (!initType) {
                error(d, "cannot infer type for '" + varName + "'");
                continue;
            }
            if (initType->kind == TypeKind::Void) {
                error(d, "cannot infer type: initializer for '" + varName +
                         "' has type 'void'");
                continue;
            }
            std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{initType, arrayDims, arraySizes, true, false, nullptr};
            vi.declToken = d->IDENTIFIER()->getSymbol();
            vi.scopeDepth = scopeDepth_;
            updateOwnershipOnInitialization(vi, d->expression());
            locals_[varName] = vi;
            // Store compile-time value for use in array dimensions
            {
                auto cv = evaluateConstExpr(d->expression());
                if (cv) compileTimeValues_[varName] = *cv;
            }
            markExprAsMoved(d->expression(), stmt);
            trackVarBufferFromExpr(varName, d->expression(), initType);
            trackVarNumericRangeFromExpr(varName, d->expression(), initType);
            continue;
        }

        // Explicit type: without own initializer — may receive propagated value
        if (!d->expression()) {
            bool hasPropagated = (lastInitDecl != nullptr);
            bool autoInit = hasPropagated ||
                (typeInfo->kind == TypeKind::Extended ||
                 typeInfo->kind == TypeKind::Struct ||
                 arrayDims > 0);
            std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());
            VarInfo vi{typeInfo, arrayDims, arraySizes, autoInit, false, nullptr};
            vi.declToken = d->IDENTIFIER()->getSymbol();
            vi.scopeDepth = scopeDepth_;
            vi.ownership = autoInit && isDropTrackedType(typeInfo, arrayDims)
                ? VarInfo::OwnershipState::Owned
                : VarInfo::OwnershipState::BorrowedImm;
            locals_[varName] = vi;
            continue;
        }

        // Explicit type with initializer — validate
        auto* initType = resolveExprType(d->expression());
        if (dynamic_cast<LucisParser::RangeExprContext*>(d->expression()) ||
            dynamic_cast<LucisParser::RangeInclExprContext*>(d->expression())) {
            error(d, "type mismatch: cannot initialize variable '" + varName +
                     "' with a range expression");
            continue;
        }
        std::vector<unsigned> arraySizes = extractArraySizesFromSpec(stmt->typeSpec());

        if (typeInfo->kind == TypeKind::Extended &&
            dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression())) {
            if (typeInfo->extendedKind == "Map") {
                error(d, "Map cannot be initialized with a literal; "
                         "use method 'set' to add entries");
                continue;
            }
            auto* arrExpr = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression());
            for (auto* e : arrExpr->expression()) {
                auto* et = resolveExprType(e);
                if (et && typeInfo->elementType &&
                    !isAssignable(typeInfo->elementType, et)) {
                    error(e, "element type mismatch: expected '" +
                             typeInfo->elementType->name + "', got '" + et->name + "'");
                }
            }
        } else if (initType && !isAssignable(typeInfo, initType)) {
            // Special case: [N]uint8 = c"..." — allow if sizes match
            bool cstrToArrayOk = false;
            if (arrayDims > 0 && initType->kind == TypeKind::Pointer) {
                if (auto* cexpr = dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression())) {
                    // typeInfo is the element type (e.g., uint8) when arrayDims > 0
                    if (typeInfo && (typeInfo->name == "uint8" || typeInfo->name == "char")) {
                        auto cstrLen = tryGetCStringLiteralLen(d->expression());
                        // c-string includes null terminator: content + 1
                        if (cstrLen) {
                            unsigned inferred = static_cast<unsigned>(*cstrLen + 1);
                            if (arraySizes.empty()) {
                                // []uint8 = c"..." → infer size
                                arraySizes.push_back(inferred);
                                cstrToArrayOk = true;
                            } else if (arraySizes[0] == inferred) {
                                cstrToArrayOk = true;
                            } else {
                                error(d, "c-string literal length (" + std::to_string(inferred) +
                                      ") does not match array size (" + std::to_string(arraySizes[0]) + ")");
                            }
                        }
                    }
                }
            }
            if (!cstrToArrayOk) {
                error(d, "type mismatch: cannot assign '" + initType->name +
                      "' to variable '" + varName + "' of type '" + typeInfo->name + "'");
            }
        } else if (initType) {
            bool isArrayLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(d->expression()) != nullptr;
            if (arrayDims == 0 && isArrayLit) {
                error(d, "type mismatch: cannot assign array literal to non-array variable '" +
                          varName + "'");
            } else if (arrayDims > 0 && !isArrayLit) {
                bool isScalarLit =
                    dynamic_cast<LucisParser::IntLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::HexLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::OctLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::BinLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::FloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::BoolLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::CharLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::StrLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::CStrLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(d->expression()) ||
                    dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(d->expression());
                if (isScalarLit) {
                    error(d, "type mismatch: cannot assign scalar to array variable '" +
                              varName + "'");
                }
            }
        }

        checkNegativeToUnsigned(typeInfo, d->expression(), d);

            VarInfo vi{typeInfo, arrayDims, arraySizes, true, false, nullptr};
        vi.declToken = d->IDENTIFIER()->getSymbol();
        vi.scopeDepth = scopeDepth_;
        updateOwnershipOnInitialization(vi, d->expression());
        locals_[varName] = vi;
        // Store compile-time value for use in array dimensions
        {
            auto cv = evaluateConstExpr(d->expression());
            if (cv) compileTimeValues_[varName] = *cv;
        }
        markExprAsMoved(d->expression(), stmt);
        trackVarBufferFromExpr(varName, d->expression(), typeInfo);
        trackVarNumericRangeFromExpr(varName, d->expression(), typeInfo);
    }
}

void Checker::checkAssignStmt(LucisParser::AssignStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();
    auto it = locals_.find(name);

    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + name + "'");
        return;
    }

    if (it->second.isConst) {
        error(stmt, "cannot assign to constant '" + name + "'");
        return;
    }

    // Mark variable as initialized on assignment
    it->second.initialized = true;

    // Validate index expressions and RHS
    auto indexExprs = stmt->expression();

    auto* varType = it->second.type;

    // For Map<K,V>, subscript assignment: m[key] = val
    if (indexExprs.size() == 2 && varType &&
        varType->kind == TypeKind::Extended && varType->keyType) {
        auto* keyType = resolveExprType(indexExprs[0]);
        auto* rhsType = resolveExprType(indexExprs[1]);

        if (keyType && !isAssignable(varType->keyType, keyType))
            error(stmt, "map key type mismatch: expected '" +
                         varType->keyType->name + "', got '" + keyType->name + "'");
        if (rhsType && varType->valueType && !isAssignable(varType->valueType, rhsType))
            error(stmt, "map value type mismatch: expected '" +
                         varType->valueType->name + "', got '" + rhsType->name + "'");
        return;
    }

    for (size_t i = 0; i + 1 < indexExprs.size(); i++) {
        auto* idxType = resolveExprType(indexExprs[i]);
        if (idxType && !isInteger(idxType))
            error(stmt, "index must be integer, got '" +
                             idxType->name + "'");
    }

    if (!indexExprs.empty()) {
        auto* rhsType = resolveExprType(indexExprs.back());
        // Resolve expected assignment type after each index operation.
        auto* expectedType = varType;
        for (size_t i = 0; i + 1 < indexExprs.size(); i++) {
            if (!expectedType) break;

            // Auto-dereference pointers before resolving element type
            if (expectedType->kind == TypeKind::Pointer && expectedType->pointeeType)
                expectedType = expectedType->pointeeType;

            // m[key] = value
            if (expectedType->kind == TypeKind::Extended && expectedType->keyType) {
                expectedType = expectedType->valueType;
                continue;
            }

            // vec[i] = value
            if (expectedType->kind == TypeKind::Extended && expectedType->elementType) {
                expectedType = expectedType->elementType;
                continue;
            }
        }
        if (rhsType && expectedType && !isAssignable(expectedType, rhsType)) {
            error(stmt, "type mismatch: cannot assign '" + rhsType->name +
                             "' to variable '" + name + "' of type '" +
                             expectedType->name + "'");
        } else if (rhsType && expectedType && indexExprs.size() == 1) {
            // Whole-variable assignment: check array dimension mismatch
            unsigned varDims = it->second.arrayDims;
            bool isArrayLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(indexExprs.back()) != nullptr;
            if (varDims == 0 && isArrayLit) {
                error(stmt, "type mismatch: cannot assign array literal to non-array variable '" +
                                 name + "'");
            } else if (varDims > 0 && !isArrayLit) {
                bool isScalarLit =
                    dynamic_cast<LucisParser::IntLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::HexLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::OctLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::BinLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::FloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::LeadingDotFloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::BoolLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::CharLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::StrLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::CStrLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(indexExprs.back()) ||
                    dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(indexExprs.back());
                if (isScalarLit) {
                    error(stmt, "type mismatch: cannot assign scalar to array variable '" +
                                     name + "'");
                }
            }
        }
        // Check: assigning a negative constant to an unsigned integer type
        checkNegativeToUnsigned(expectedType, indexExprs.back(), stmt);

        // Whole-variable assignment (x = expr) can change tracked buffer state.
        if (indexExprs.size() == 1) {
            if (isDropTrackedType(it->second.type, it->second.arrayDims)) {
                updateOwnershipOnInitialization(it->second, indexExprs.back());
            }
            markExprAsMoved(indexExprs.back(), stmt);
            trackVarBufferFromExpr(name, indexExprs.back(), varType);
            trackVarNumericRangeFromExpr(name, indexExprs.back(), varType);
        }
    }
}

void Checker::checkCompoundAssignStmt(LucisParser::CompoundAssignStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();
    auto it = locals_.find(name);

    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + name + "'");
        return;
    }

    auto* varType = it->second.type;
    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: ptr ??= default
    if (opText == "??=") {
        if (varType && varType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer variable, got '" +
                             varType->name + "'");
        if (rhsType && varType && !isAssignable(varType, rhsType))
            error(stmt, "'??=' type mismatch: variable type '" +
                             varType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool isPtrArith = varType && varType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && varType && !isNumeric(varType))
            error(stmt, "operator '" + opText +
                             "' requires numeric variable, got '" + varType->name + "'");
        if (needsInteger && varType && !isIntegerOrPointer(varType))
            error(stmt, "operator '" + opText +
                             "' requires integer variable, got '" + varType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between variable and RHS
        if (rhsType && varType && !isAssignable(varType, rhsType))
            error(stmt, "type mismatch in compound assignment: variable type '" +
                        varType->name + "', cannot use '" + rhsType->name + "'");
    }

    // Compile-time division by zero check
    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}

void Checker::checkFieldAssignStmt(LucisParser::FieldAssignStmtContext* stmt) {
    auto identifiers = stmt->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    if (it->second.isConst) {
        error(stmt, "cannot assign to constant '" + varName + "'");
        return;
    }

    // Walk the field chain: p.x.y = val
    auto* currentType = it->second.type;
    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();

        if (currentType && currentType->kind == TypeKind::Pointer &&
            currentType->pointeeType &&
            (currentType->pointeeType->kind == TypeKind::Struct ||
             currentType->pointeeType->kind == TypeKind::Union)) {
            currentType = currentType->pointeeType;
        }

        if (!currentType || (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "'" +
                             (i == 1 ? varName : identifiers[i-1]->getText()) +
                             "' is not a struct or union type");
            return;
        }

        bool found = false;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                currentType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }
    }

    auto* rhsType = resolveExprType(stmt->expression());
    if (rhsType && currentType && !isAssignable(currentType, rhsType)) {
        error(stmt, "type mismatch in field assignment: expected '" +
                         currentType->name + "', got '" + rhsType->name + "'");
    }
}

void Checker::checkFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext* stmt) {
    auto identifiers = stmt->IDENTIFIER();
    auto varName = identifiers[0]->getText();

    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    // Walk the field chain: p.x.y += val
    auto* currentType = it->second.type;
    for (size_t i = 1; i < identifiers.size(); i++) {
        auto fieldName = identifiers[i]->getText();

        if (currentType && currentType->kind == TypeKind::Pointer &&
            currentType->pointeeType &&
            (currentType->pointeeType->kind == TypeKind::Struct ||
             currentType->pointeeType->kind == TypeKind::Union)) {
            currentType = currentType->pointeeType;
        }

        if (!currentType || (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "'" +
                             (i == 1 ? varName : identifiers[i-1]->getText()) +
                             "' is not a struct or union type");
            return;
        }

        bool found = false;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                currentType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }
    }

    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: obj.field ??= default
    if (opText == "??=") {
        if (currentType && currentType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer field, got '" +
                             currentType->name + "'");
        if (rhsType && currentType && !isAssignable(currentType, rhsType))
            error(stmt, "'??=' type mismatch: field type '" +
                             currentType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool isPtrArith = currentType && currentType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && currentType && !isNumeric(currentType))
            error(stmt, "operator '" + opText +
                             "' requires numeric field, got '" + currentType->name + "'");
        if (needsInteger && currentType && !isIntegerOrPointer(currentType))
            error(stmt, "operator '" + opText +
                             "' requires integer field, got '" + currentType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between field and RHS
        if (rhsType && currentType && !isAssignable(currentType, rhsType))
            error(stmt, "type mismatch in field compound assignment: field type '" +
                        currentType->name + "', cannot use '" + rhsType->name + "'");
    }

    // Compile-time division by zero check
    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}

void Checker::checkArrowAssignStmt(LucisParser::ArrowAssignStmtContext* stmt) {
    auto ids = stmt->IDENTIFIER();
    if (ids.size() < 2) {
        error(stmt, "malformed '->' assignment");
        return;
    }

    auto varName = ids[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    const TypeInfo* currentType = it->second.type;
    bool pointerBase = false;

    if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
        currentType = currentType->pointeeType;
        pointerBase = true;
    }

    // Resolve dotted base before arrow: a.b.c->x
    for (size_t i = 1; i + 1 < ids.size(); i++) {
        auto fieldName = ids[i]->getText();

        if (!currentType ||
            (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "cannot access field '" + fieldName +
                             "' on non-struct type");
            return;
        }

        bool found = false;
        const TypeInfo* nextType = nullptr;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                nextType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found || !nextType) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }

        currentType = nextType;
        if (currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
            currentType = currentType->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentType ||
        (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
        error(stmt, "'->' requires pointer to struct or union");
        return;
    }

    auto arrowField = ids.back()->getText();
    const TypeInfo* fieldType = nullptr;
    for (auto& field : currentType->fields) {
        if (field.name == arrowField) {
            fieldType = field.typeInfo;
            break;
        }
    }
    if (!fieldType) {
        error(stmt, "struct '" + currentType->name +
                         "' has no field '" + arrowField + "'");
        return;
    }

    auto* rhsType = resolveExprType(stmt->expression());
    if (rhsType && fieldType && !isAssignable(fieldType, rhsType)) {
        error(stmt, "type mismatch in arrow assignment: expected '" +
                         fieldType->name + "', got '" + rhsType->name + "'");
    }
}

void Checker::checkArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext* stmt) {
    auto ids = stmt->IDENTIFIER();
    if (ids.size() < 2) {
        error(stmt, "malformed '->' compound assignment");
        return;
    }

    auto varName = ids[0]->getText();
    auto it = locals_.find(varName);
    if (it == locals_.end()) {
        error(stmt, "undefined variable '" + varName + "'");
        return;
    }

    const TypeInfo* currentType = it->second.type;
    bool pointerBase = false;

    if (currentType && currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
        currentType = currentType->pointeeType;
        pointerBase = true;
    }

    for (size_t i = 1; i + 1 < ids.size(); i++) {
        auto fieldName = ids[i]->getText();

        if (!currentType ||
            (currentType->kind != TypeKind::Struct && currentType->kind != TypeKind::Union)) {
            error(stmt, "cannot access field '" + fieldName +
                             "' on non-struct type");
            return;
        }

        bool found = false;
        const TypeInfo* nextType = nullptr;
        for (auto& field : currentType->fields) {
            if (field.name == fieldName) {
                nextType = field.typeInfo;
                found = true;
                break;
            }
        }
        if (!found || !nextType) {
            error(stmt, "struct '" + currentType->name +
                             "' has no field '" + fieldName + "'");
            return;
        }

        currentType = nextType;
        if (currentType->kind == TypeKind::Pointer && currentType->pointeeType) {
            currentType = currentType->pointeeType;
            pointerBase = true;
        }
    }

    if (!pointerBase || !currentType || currentType->kind != TypeKind::Struct) {
        error(stmt, "'->' requires pointer to struct");
        return;
    }

    auto arrowField = ids.back()->getText();
    const TypeInfo* fieldType = nullptr;
    for (auto& field : currentType->fields) {
        if (field.name == arrowField) {
            fieldType = field.typeInfo;
            break;
        }
    }
    if (!fieldType) {
        error(stmt, "struct '" + currentType->name +
                         "' has no field '" + arrowField + "'");
        return;
    }

    auto* rhsType = resolveExprType(stmt->expression());
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: ptr->field ??= default
    if (opText == "??=") {
        if (fieldType && fieldType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer field, got '" +
                             fieldType->name + "'");
        if (rhsType && fieldType && !isAssignable(fieldType, rhsType))
            error(stmt, "'??=' type mismatch: field type '" +
                             fieldType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool isPtrArith = fieldType && fieldType->kind == TypeKind::Pointer &&
                      (opText == "+=" || opText == "-=");

    bool needsNumeric = !isPtrArith && (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (isPtrArith) {
        if (rhsType && !isInteger(rhsType))
            error(stmt, "pointer arithmetic '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
    } else {
        if (needsNumeric && fieldType && !isNumeric(fieldType))
            error(stmt, "operator '" + opText +
                             "' requires numeric field, got '" + fieldType->name + "'");
        if (needsInteger && fieldType && !isInteger(fieldType))
            error(stmt, "operator '" + opText +
                             "' requires integer field, got '" + fieldType->name + "'");
        if (needsNumeric && rhsType && !isNumeric(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires numeric operand, got '" + rhsType->name + "'");
        if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
            error(stmt, "operator '" + opText +
                             "' requires integer operand, got '" + rhsType->name + "'");
        // Type compatibility between field and RHS
        if (rhsType && fieldType && !isAssignable(fieldType, rhsType))
            error(stmt, "type mismatch in arrow compound assignment: field type '" +
                        fieldType->name + "', cannot use '" + rhsType->name + "'");
    }

    if (opText == "/=" || opText == "%=") {
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(stmt->expression())) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}

void Checker::checkDerefAssignStmt(LucisParser::DerefAssignStmtContext* stmt) {
    if (stmt->IDENTIFIER()) {
        // *ptr = value;
        auto varName = stmt->IDENTIFIER()->getText();
        auto it = locals_.find(varName);

        if (it == locals_.end()) {
            error(stmt, "undefined variable '" + varName + "'");
            return;
        }

        if (it->second.type && it->second.type->kind != TypeKind::Pointer) {
            error(stmt, "cannot dereference non-pointer variable '" +
                             varName + "' of type '" + it->second.type->name + "'");
        }

        resolveExprType(stmt->expression(0));
    } else {
        // *(expr) = value;
        auto* ptrType = resolveExprType(stmt->expression(0));
        if (ptrType && ptrType->kind != TypeKind::Pointer) {
            error(stmt, "cannot dereference non-pointer expression of type '" +
                             ptrType->name + "'");
        }
        resolveExprType(stmt->expression(1));
    }
}

void Checker::checkDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext* stmt) {
    const TypeInfo* targetType = nullptr;

    if (stmt->IDENTIFIER()) {
        // *ptr op= value;
        auto varName = stmt->IDENTIFIER()->getText();
        auto it = locals_.find(varName);

        if (it == locals_.end()) {
            error(stmt, "undefined variable '" + varName + "'");
            return;
        }

        if (!it->second.type || it->second.type->kind != TypeKind::Pointer ||
            !it->second.type->pointeeType) {
            error(stmt, "cannot dereference non-pointer variable '" +
                             varName + "' of type '" + it->second.type->name + "'");
            return;
        }

        targetType = it->second.type->pointeeType;
    } else {
        // *(expr) op= value;
        auto* ptrType = resolveExprType(stmt->expression(0));
        if (!ptrType || ptrType->kind != TypeKind::Pointer || !ptrType->pointeeType) {
            error(stmt, "cannot dereference non-pointer expression");
            return;
        }
        targetType = ptrType->pointeeType;
    }

    auto* rhsType = resolveExprType(stmt->expression(stmt->IDENTIFIER() ? 0 : 1));
    auto opText = stmt->op->getText();

    // Null-coalescing assignment: *ptr ??= default
    if (opText == "??=") {
        if (targetType && targetType->kind != TypeKind::Pointer)
            error(stmt, "'??=' requires pointer target (deref of pointer-to-pointer), got '" +
                             targetType->name + "'");
        if (rhsType && targetType && !isAssignable(targetType, rhsType))
            error(stmt, "'??=' type mismatch: target type '" +
                             targetType->name + "', cannot use '" +
                             rhsType->name + "'");
        return;
    }

    bool needsNumeric = (opText == "+=" || opText == "-=" ||
                         opText == "*=" || opText == "/=");
    bool needsInteger = (opText == "%=" || opText == "&=" || opText == "|=" ||
                         opText == "^=" || opText == "<<=" || opText == ">>=");

    if (needsNumeric && targetType && !isNumeric(targetType))
        error(stmt, "operator '" + opText +
                         "' requires numeric pointer target, got '" + targetType->name + "'");
    if (needsInteger && targetType && !isInteger(targetType))
        error(stmt, "operator '" + opText +
                         "' requires integer pointer target, got '" + targetType->name + "'");
    if (needsNumeric && rhsType && !isNumeric(rhsType))
        error(stmt, "operator '" + opText +
                         "' requires numeric operand, got '" + rhsType->name + "'");
    if (needsInteger && rhsType && !isIntegerOrPointer(rhsType))
        error(stmt, "operator '" + opText +
                         "' requires integer operand, got '" + rhsType->name + "'");

    // Type compatibility between deref target and RHS
    if (rhsType && targetType && !isAssignable(targetType, rhsType))
        error(stmt, "type mismatch in deref compound assignment: target type '" +
                    targetType->name + "', cannot use '" + rhsType->name + "'");

    if (opText == "/=" || opText == "%=") {
        auto* rhsExpr = stmt->expression(stmt->IDENTIFIER() ? 0 : 1);
        if (auto* intLit = dynamic_cast<LucisParser::IntLitExprContext*>(rhsExpr)) {
            if (intLit->INT_LIT()->getText() == "0")
                error(stmt, "division by zero");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Global builtins — always available without `use`
// ═══════════════════════════════════════════════════════════════════════

void Checker::registerGlobalBuiltins() {
    auto* voidTy   = typeRegistry_.lookup("void");
    auto* i32Ty    = typeRegistry_.lookup("int32");
    auto* i64Ty    = typeRegistry_.lookup("int64");
    auto* f64Ty    = typeRegistry_.lookup("float64");
    auto* boolTy   = typeRegistry_.lookup("bool");
    auto* strTy    = typeRegistry_.lookup("string");

    // exit(int32) -> void
    functions_["exit"] = makeFunctionType(voidTy, { i32Ty });
    globalBuiltins_.insert("exit");

    // panic(string) -> void
    functions_["panic"] = makeFunctionType(voidTy, { strTy });
    globalBuiltins_.insert("panic");

    // assert(bool) -> void
    functions_["assert"] = makeFunctionType(voidTy, { boolTy });
    globalBuiltins_.insert("assert");

    // assertMsg(bool, string) -> void
    functions_["assertMsg"] = makeFunctionType(voidTy, { boolTy, strTy });
    globalBuiltins_.insert("assertMsg");

    // unreachable() -> void
    functions_["unreachable"] = makeFunctionType(voidTy, {});
    globalBuiltins_.insert("unreachable");

    // toInt(string) -> int64
    functions_["toInt"] = makeFunctionType(i64Ty, { strTy });
    globalBuiltins_.insert("toInt");

    // toFloat(string) -> float64
    functions_["toFloat"] = makeFunctionType(f64Ty, { strTy });
    globalBuiltins_.insert("toFloat");

    // toBool(string) -> bool
    functions_["toBool"] = makeFunctionType(boolTy, { strTy });
    globalBuiltins_.insert("toBool");

    // toString(T) -> string — accepts any primitive type
    // We register it with a dummy param; actual type checking is relaxed
    functions_["toString"] = makeFunctionType(strTy, { i32Ty });
    globalBuiltins_.insert("toString");

    // ── C FFI string conversion builtins ────────────────────────────
    auto* charPtrTy = getPointerType(typeRegistry_.lookup("char"));
    auto* usizeTy   = typeRegistry_.lookup("usize");

    // cstr(string) -> *char
    functions_["cstr"] = makeFunctionType(charPtrTy, { strTy });
    globalBuiltins_.insert("cstr");

    // fromCStr(*char) -> string
    functions_["fromCStr"] = makeFunctionType(strTy, { charPtrTy });
    globalBuiltins_.insert("fromCStr");

    // fromCStrCopy(*char) -> string
    functions_["fromCStrCopy"] = makeFunctionType(strTy, { charPtrTy });
    globalBuiltins_.insert("fromCStrCopy");

    // fromCStrLen(*char, usize) -> string
    functions_["fromCStrLen"] = makeFunctionType(strTy, { charPtrTy, usizeTy });
    globalBuiltins_.insert("fromCStrLen");

    // freeStr(string) -> void
    functions_["freeStr"] = makeFunctionType(voidTy, { strTy });
    globalBuiltins_.insert("freeStr");
}

// ═══════════════════════════════════════════════════════════════════════
//  FFI: extern function declarations
// ═══════════════════════════════════════════════════════════════════════

void Checker::checkExternDecl(LucisParser::ExternDeclContext* decl) {
    auto funcName = decl->IDENTIFIER()->getText();

    unsigned retDims = 0;
    auto* retType = resolveTypeSpec(decl->typeSpec(), retDims);
    if (!retType) return;

    bool isVariadic = (decl->SPREAD() != nullptr);

    std::vector<const TypeInfo*> paramTypes;
    if (auto* paramList = decl->externParamList()) {
        for (auto* param : paramList->externParam()) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpec(param->typeSpec(), pDims);
            if (!pType) return;
            paramTypes.push_back(pType);
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic);
    functions_[funcName] = funcType;
    globalBuiltins_.insert(funcName);
}

void Checker::checkCallStmt(LucisParser::CallStmtContext* stmt) {
    auto name = stmt->IDENTIFIER()->getText();

    // Check if name is a local variable with a function/closure type
    auto lit = locals_.find(name);
    if (lit != locals_.end()) {
        auto* localType = lit->second.type;
        if (localType && (localType->kind == TypeKind::Function ||
                          localType->kind == TypeKind::Closure)) {
            // Resolve all argument types
            std::vector<const TypeInfo*> argTypes;
            std::vector<LucisParser::ExpressionContext*> argExprs;
            if (auto* argList = stmt->argList()) {
                for (auto* argExpr : argList->expression()) {
                    argExprs.push_back(argExpr);
                    argTypes.push_back(resolveExprType(argExpr));
                }
            }

            // A value argument cannot have type void.
            for (size_t i = 0; i < argTypes.size(); i++) {
                auto* ti = argTypes[i];
                if (ti && ti->kind == TypeKind::Void) {
                    error(argExprs[i],
                          "argument " + std::to_string(i + 1) +
                          " has type 'void'; functions returning void cannot be used as values");
                    return;
                }
            }

            size_t argCount = argTypes.size();
            size_t paramCount = localType->paramTypes.size();

            if (argCount != paramCount) {
                error(stmt, "function '" + name + "' expects " +
                                 std::to_string(paramCount) +
                                 " arguments " + formatParamTypes(localType->paramTypes) +
                                 ", got " + std::to_string(argCount));
                return;
            }
            for (size_t i = 0; i < argCount; i++) {
                if (argTypes[i] && localType->paramTypes[i] &&
                    !isAssignable(localType->paramTypes[i], argTypes[i])) {
                    error(stmt,
                        "argument " + std::to_string(i + 1) +
                        " type mismatch in '" + name + "': expected '" +
                        localType->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            applyCallOwnershipEffects(name, argExprs, stmt);
            return;
        }
    }

    // Check for function-like macro calls (e.g. SQUARE(3))
    if (cFunctionLikeMacros_.count(name)) {
        auto* flm = cFunctionLikeMacros_[name];
        size_t argCount = stmt->argList() ? stmt->argList()->expression().size() : 0;
        if (argCount != flm->paramNames.size()) {
            error(stmt, "function-like macro '" + name + "' expects " +
                  std::to_string(flm->paramNames.size()) + " argument(s), got " +
                  std::to_string(argCount));
        }
        return;
    }

    if (!isKnownFunction(name)) {
        std::string hint = ImportResolver::suggestImport(name);
        std::string msg = "call to undeclared function '" + name + "'";
        if (!hint.empty())
            msg += ". Did you forget '" + hint + "'?";
        error(stmt, msg);
        return;
    }

    // Resolve all argument types
    std::vector<const TypeInfo*> argTypes;
    std::vector<LucisParser::ExpressionContext*> argExprs;
    if (auto* argList = stmt->argList()) {
        for (auto* argExpr : argList->expression()) {
            argExprs.push_back(argExpr);
            argTypes.push_back(resolveExprType(argExpr));
        }
    }

    // A value argument cannot have type void.
    for (size_t i = 0; i < argTypes.size(); i++) {
        auto* ti = argTypes[i];
        if (ti && ti->kind == TypeKind::Void) {
            error(argExprs[i],
                  "argument " + std::to_string(i + 1) +
                  " has type 'void'; functions returning void cannot be used as values");
            return;
        }
    }

    // Guard against invalid ABI lowering for array values in std::log print calls.
    // Arrays do not map to scalar print builtins; require explicit conversion.
    if ((name == "print" || name == "println" || name == "eprint" || name == "eprintln") &&
        (imports_.isImported(name) || functions_.count(name))) {
        for (size_t i = 0; i < argExprs.size(); i++) {
            if (resolveExprArrayDims(argExprs[i]) > 0) {
                error(stmt, "function '" + name + "' does not accept array arguments directly; "
                            "use '.toString()' (or '.join(...)' for strings) before printing");
                return;
            }
        }
    }
    // Lucis std::log::sprintf expects Lucis `string` values, not `*char`.
    // Passing `*char` reaches invalid IR lowering; fail early in checker.
    if (name == "sprintf" && imports_.isImported("sprintf")) {
        for (size_t i = 0; i < argTypes.size(); i++) {
            auto* ti = argTypes[i];
            if (!ti) continue;
            if (ti->kind == TypeKind::Pointer &&
                ti->pointeeType &&
                ti->pointeeType->kind == TypeKind::Char) {
                error(argExprs[i],
                      "'sprintf' from Lucis does not accept '*char'; "
                      "pass 'string' values (use 'fromCStr(...)' if needed)");
            }
        }
    }

    // Generic function call with inferred type arguments: foo(10);
    auto gfit = genericFuncTemplates_.find(name);
    if (gfit != genericFuncTemplates_.end()) {
        std::vector<LucisParser::ParamContext*> formalParams;
        if (auto* paramList = gfit->second.decl->paramList())
            formalParams = paramList->param();

        // Check for typed variadic last param: T ...args
        bool hasTypedVariadic = false;
        if (!formalParams.empty()) {
            auto* last = formalParams.back();
            hasTypedVariadic = last && last->SPREAD() && last->typeSpec();
        }
        size_t fixedCount = hasTypedVariadic ? formalParams.size() - 1 : formalParams.size();

        if (!hasTypedVariadic && argTypes.size() != formalParams.size()) {
            error(stmt, "generic function '" + name + "' expects " +
                         std::to_string(formalParams.size()) +
                         " argument(s), got " + std::to_string(argTypes.size()));
            return;
        }
        if (hasTypedVariadic && argTypes.size() < fixedCount) {
            error(stmt, "generic function '" + name + "' expects at least " +
                         std::to_string(fixedCount) +
                         " argument(s), got " + std::to_string(argTypes.size()));
            return;
        }

        auto inferred = inferGenericTypeArgs(
            name,
            gfit->second.typeParams,
            gfit->second.decl->typeParamList(),
            formalParams,
            argTypes,
            stmt);
        if (!inferred) return;

        instantiateGenericFunc(name, gfit->second, *inferred, stmt);
        applyCallOwnershipEffects(name, argExprs, stmt);
        return;
    }

    // Check user-defined function signatures
    auto fit = functions_.find(name);
    if (fit != functions_.end() && fit->second->kind == TypeKind::Function) {
        size_t argCount = argTypes.size();
        auto* fnType = fit->second;
        size_t paramCount = fnType->paramTypes.size();

        if (fnType->isVariadic) {
            bool isTyped = fnType->variadicElementType != nullptr;
            size_t fixedCount = isTyped && paramCount > 0 ? paramCount - 1 : paramCount;

            // Typed variadic: fixed params are paramCount-1 (last is element type)
            if (argCount < fixedCount) {
                error(stmt, "function '" + name + "' expects at least " +
                                 std::to_string(fixedCount) +
                                 " arguments " + formatParamTypes(fnType->paramTypes) +
                                 ", got " + std::to_string(argCount));
            }
            // Validate fixed parameter types
            for (size_t i = 0; i < std::min(argCount, fixedCount); i++) {
                if (argTypes[i] && fnType->paramTypes[i] &&
                    !isAssignable(fnType->paramTypes[i], argTypes[i])) {
                    error(stmt,
                        "argument " + std::to_string(i + 1) +
                        " type mismatch in '" + name + "': expected '" +
                        fnType->paramTypes[i]->name + "', got '" +
                        argTypes[i]->name + "'");
                }
            }
            // Validate variadic args against element type (typed variadic only)
            if (isTyped) {
                for (size_t i = fixedCount; i < argCount; i++) {
                    if (argTypes[i] && fnType->variadicElementType &&
                        !isAssignable(fnType->variadicElementType, argTypes[i])) {
                        error(stmt,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch in '" + name + "': expected '" +
                            fnType->variadicElementType->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            }
        } else {
            if (argCount != paramCount) {
                error(stmt, "function '" + name + "' expects " +
                                 std::to_string(paramCount) +
                                 " arguments " + formatParamTypes(fnType->paramTypes) +
                                 ", got " + std::to_string(argCount));
            } else {
                for (size_t i = 0; i < argCount; i++) {
                    if (argTypes[i] && fnType->paramTypes[i] &&
                        !isAssignable(fnType->paramTypes[i], argTypes[i])) {
                        error(stmt,
                            "argument " + std::to_string(i + 1) +
                            " type mismatch in '" + name + "': expected '" +
                            fnType->paramTypes[i]->name + "', got '" +
                            argTypes[i]->name + "'");
                    }
                }
            }
        }
        analyzeUnsafeCBufferCall(name, stmt, argExprs);
        applyCallOwnershipEffects(name, argExprs, stmt);
        return;
    }

    // Check builtin function signatures
    auto* sig = builtinRegistry_.lookup(name);
    if (!sig) return; // unknown builtin, already reported

    size_t argCount = argTypes.size();
    if (sig->isVariadic) {
        if (argCount < sig->paramTypes.size()) {
            error(stmt, "'" + name + "' expects at least " +
                             std::to_string(sig->paramTypes.size()) +
                             " argument(s) " + formatParamTypes(sig->paramTypes) +
                             ", got " + std::to_string(argCount));
            return;
        }
    } else if (argCount != sig->paramTypes.size()) {
        error(stmt, "'" + name + "' expects " +
                         std::to_string(sig->paramTypes.size()) +
                         " argument(s) " + formatParamTypes(sig->paramTypes) +
                         ", got " + std::to_string(argCount));
        return;
    }

    // Validate argument types for non-polymorphic builtins
    if (!sig->isPolymorphic) {
        for (size_t i = 0; i < argCount; i++) {
            if (!argTypes[i]) continue;
            auto& expected = sig->paramTypes[i];
            auto* argTI = argTypes[i];

            if (expected == "_any" || expected == "_numeric" ||
                expected == "_integer" || expected == "_float")
                continue;

            auto* expectedTI = resolveBuiltinReturnType(expected);
            if (expectedTI && !isAssignable(expectedTI, argTI)) {
                error(stmt, "'" + name + "' argument " +
                    std::to_string(i + 1) + ": expected '" +
                    expected + "', got '" + argTI->name + "'");
            }
        }
    }

    // Cross-parameter validation for polymorphic builtins
    if (sig->isPolymorphic && argCount >= 2) {
        auto isNumeric = [](const TypeInfo* t) {
            return t && (t->kind == TypeKind::Integer ||
                         t->kind == TypeKind::Float);
        };
        auto isScalar = [](const TypeInfo* t) {
            return t && (t->kind == TypeKind::Integer ||
                         t->kind == TypeKind::Float ||
                         t->kind == TypeKind::Bool ||
                         t->kind == TypeKind::Char);
        };
        bool allAny = true, allNumeric = true;
        for (auto& p : sig->paramTypes) {
            if (p != "_any") allAny = false;
            if (p != "_numeric") allNumeric = false;
        }
        if (allAny) {
            auto* firstType = argTypes[0];
            for (size_t i = 1; i < argCount; i++) {
                if (!argTypes[i] || !firstType) continue;
                bool stringVsScalar =
                    (firstType->kind == TypeKind::String && isScalar(argTypes[i])) ||
                    (argTypes[i]->kind == TypeKind::String && isScalar(firstType));
                if (stringVsScalar) {
                    error(stmt, "'" + name + "' argument " +
                        std::to_string(i + 1) + ": type mismatch, expected '" +
                        firstType->name + "', got '" + argTypes[i]->name + "'");
                }
            }
        }
        if (allNumeric) {
            for (size_t i = 0; i < argCount; i++) {
                if (!argTypes[i]) continue;
                if (!isNumeric(argTypes[i])) {
                    error(stmt, "'" + name + "' argument " +
                        std::to_string(i + 1) + ": expected numeric type, got '" +
                        argTypes[i]->name + "'");
                }
            }
        }
    }

    analyzeUnsafeCBufferCall(name, stmt, argExprs);
    applyCallOwnershipEffects(name, argExprs, stmt);
}

void Checker::checkAsmStmt(LucisParser::AsmStmtContext* stmt) {
    bool hasOutput = stmt->asmOutputList() != nullptr;
    bool hasInput = stmt->asmInputList() != nullptr;
    bool isVolatile = stmt->VOLATILE() != nullptr;

    // Count outputs for matching-constraint validation
    size_t numOutputs = 0;
    if (hasOutput)
        numOutputs = stmt->asmOutputList()->asmOutput().size();

    // Suggest volatile if no outputs (side-effect only asm)
    if (!hasOutput && !isVolatile) {
        warning(stmt, "asm statement with no outputs should be 'asm volatile'");
    }

    // Validate output constraints and variables
    if (auto* outList = stmt->asmOutputList()) {
        for (auto* out : outList->asmOutput()) {
            auto raw = out->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);

            // Output constraints must start with '=', '+', or be a digit (matching)
            if (constraint.empty()) {
                error(out, "asm output constraint cannot be empty");
            } else if (constraint[0] != '=' && constraint[0] != '+'
                       && !std::isdigit(constraint[0])) {
                error(out, "asm output constraint must start with '=', '+', or a digit "
                      "(got '" + constraint + "')");
            }

            if (!out->IDENTIFIER()) continue; // unnamed output — nothing to validate
            auto varName = out->IDENTIFIER()->getText();
            auto it = locals_.find(varName);
            if (it == locals_.end()) {
                error(out, "undefined variable '" + varName + "' in asm output");
                continue;
            }

            // Mark output variable as used and initialized
            it->second.used = true;
        }
    }

    // Validate input constraints and expressions
    if (auto* inList = stmt->asmInputList()) {
        for (auto* operand : inList->asmOperand()) {
            auto raw = operand->constraint->getText();
            auto constraint = raw.substr(1, raw.size() - 2);

            if (constraint.empty()) {
                error(operand, "asm input constraint cannot be empty");
            } else if (constraint[0] == '=') {
                error(operand, "asm input constraint cannot use '=' (reserved for outputs)");
            } else if (constraint[0] == '+') {
                error(operand, "asm input constraint cannot use '+' (use in output with '+r' instead)");
            } else if (std::isdigit(constraint[0])) {
                // Matching constraint — validate index
                int matchIdx = std::stoi(constraint);
                if (matchIdx < 0 || static_cast<size_t>(matchIdx) >= numOutputs) {
                    error(operand, "asm matching constraint '" + constraint
                          + "' refers to output " + constraint + " but there "
                          + (numOutputs == 1 ? "is only 1 output"
                                             : "are only " + std::to_string(numOutputs) + " outputs"));
                }
            }

            resolveExprType(operand->expression());
        }
    }

    // Collect goto label references (validated at function end for forward refs)
    if (auto* labelList = stmt->asmGotoLabelList()) {
        for (auto* ident : labelList->IDENTIFIER())
            asmGotoLabelRefs_.insert(ident->getText());
    }
}

void Checker::checkExprStmt(LucisParser::ExprStmtContext* stmt) {
    resolveExprType(stmt->expression());
}

void Checker::checkReturnStmt(LucisParser::ReturnStmtContext* stmt,
                               const TypeInfo* expectedType) {
    auto* expr = stmt->expression();

    if (!expr) {
        if (expectedType->kind != TypeKind::Void) {
            error(stmt,
                "function with return type '" + expectedType->name +
                "' must return a value");
        }
        return;
    }

    // void function should not return a value
    if (expectedType->kind == TypeKind::Void) {
        error(stmt, "void function should not return a value");
        return;
    }

    auto* exprType = resolveExprType(expr);
    if (exprType && expectedType && !isAssignable(expectedType, exprType)) {
        error(stmt, "return type mismatch: expected '" +
                         expectedType->name + "', got '" + exprType->name + "'");
    }
    markExprAsMoved(expr, stmt);
}

unsigned Checker::resolveArrayLitDims(LucisParser::ArrayLitExprContext* arrLit) {
    if (!arrLit || arrLit->expression().empty()) return 1;
    unsigned maxInner = 0;
    for (auto* elem : arrLit->expression()) {
        if (auto* inner = dynamic_cast<LucisParser::ArrayLitExprContext*>(elem)) {
            unsigned innerDims = resolveArrayLitDims(inner);
            if (innerDims > maxInner) maxInner = innerDims;
        }
    }
    if (maxInner > 0)
        return maxInner + 1;
    return 1;
}

std::vector<unsigned> Checker::extractArraySizesFromSpec(LucisParser::TypeSpecContext* spec) {
    std::vector<unsigned> sizes;
    while (spec && spec->LBRACKET()) {
        if (spec->INT_LIT()) {
            sizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
        } else if (spec->IDENTIFIER()) {
            auto identName = spec->IDENTIFIER()->getText();
            auto it = compileTimeValues_.find(identName);
            if (it != compileTimeValues_.end() && it->second.kind() == ComptimeValue::Kind::Int) {
                sizes.push_back(static_cast<unsigned>(it->second.asInt()));
            }
        }
        spec = spec->typeSpec().empty() ? nullptr : spec->typeSpec(0);
    }
    return sizes;
}

unsigned Checker::resolveExprArrayDims(LucisParser::ExpressionContext* expr) {
    if (!expr) return 0;
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto it = locals_.find(id->IDENTIFIER()->getText());
        if (it != locals_.end())
            return it->second.arrayDims;
    }
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        unsigned baseDims = resolveExprArrayDims(idx->expression(0));
        return baseDims > 0 ? baseDims - 1 : 0;
    }
    if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(expr)) {
        unsigned baseDims = resolveExprArrayDims(mc->expression());
        if (baseDims == 0) return 0;

        auto methodName = mc->IDENTIFIER()->getText();
        auto* md = methodRegistry_.lookupArrayMethod(methodName);
        if (!md) return 0;

        if (md->returnType == "_self") return baseDims;
        if (md->returnType == "_elem") return baseDims > 0 ? baseDims - 1 : 0;
        return 0;
    }
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return resolveExprArrayDims(paren->expression());
    if (auto* arrLit = dynamic_cast<LucisParser::ArrayLitExprContext*>(expr))
        return resolveArrayLitDims(arrLit);
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr))
        return resolveExprArrayDims(deref->expression());
    if (auto* cu = dynamic_cast<LucisParser::CatchUnwrapExprContext*>(expr)) {
        auto* sourceType = resolveExprType(cu->expression());
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason))
            return 0;
        if (!pattern.okVariant || pattern.okVariant->payloadFields.empty())
            return 0;
        return pattern.okVariant->payloadFields[0].arrayDims;
    }
    if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(expr)) {
        auto* sourceType = resolveExprType(pe->expression());
        UnwrapCatchPatternInfo pattern;
        std::string reason;
        if (!classifyUnwrapCatchEnum(sourceType, pattern, reason))
            return 0;
        if (!pattern.okVariant || pattern.okVariant->payloadFields.empty())
            return 0;
        return pattern.okVariant->payloadFields[0].arrayDims;
    }
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto* baseType = resolveExprType(fa->expression());
        if (baseType && (baseType->kind == TypeKind::Struct || baseType->kind == TypeKind::Union)) {
            auto fieldName = fa->IDENTIFIER()->getText();
            for (auto& field : baseType->fields) {
                if (field.name == fieldName)
                    return field.arrayDims;
            }
        }
        return 0;
    }
    if (dynamic_cast<LucisParser::ArrayLitExprContext*>(expr))
        return 1;
    return 0;
}

void Checker::warnUnusedLocals(LucisParser::FunctionDeclContext* func) {
    warnUnusedLocals(static_cast<antlr4::ParserRuleContext*>(func));
}

void Checker::warnUnusedLocals(antlr4::ParserRuleContext* ctx) {
    for (auto& [name, info] : locals_) {
        if (!info.used && name != "_") {
            // Skip warning for top-level constants (they may be used externally)
            if (info.isConst && info.scopeDepth == 0)
                continue;
            if (info.declToken) {
                warningToken(info.declToken, info.declToken,
                             "variable '" + name + "' is declared but never used");
            } else {
                warning(ctx, "variable '" + name + "' is declared but never used");
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  User-defined generics — monomorphization
// ═══════════════════════════════════════════════════════════════════════

std::string Checker::mangleGenericName(const std::string& baseName,
                                        const std::vector<const TypeInfo*>& typeArgs) {
    std::string name = baseName;
    for (auto* arg : typeArgs) {
        name += "__";
        // Replace characters that are not valid in identifiers
        for (char c : arg->name) {
            if (c == '<' || c == '>' || c == ' ' || c == ',')
                name += '_';
            else
                name += c;
        }
    }
    return name;
}

const TypeInfo* Checker::tryResolveQualifiedType(antlr4::ParserRuleContext* ctx,
                                                const std::string& first,
                                                const std::string& second) {
    // Try as module::type
    if (moduleRegistry_ && moduleRegistry_->hasModule(first)) {
        auto* sym = moduleRegistry_->findSymbol(first, second);
        if (sym) {
            if (auto* ti = typeRegistry_.lookup(second))
                return ti;
        }
    }

    // Try as enum_type::variant (e.g. Shape::Circle)
    if (auto* enumTI = typeRegistry_.lookup(first)) {
        if (enumTI->kind == TypeKind::Enum) {
            for (const auto& variant : enumTI->enumVariantInfos) {
                if (variant.name == second)
                    return enumTI;
            }
        }
    }

    error(ctx, "'" + first + "::" + second + "' is not a known type or enum variant");
    return nullptr;
}

const TypeInfo* Checker::resolveTypeSpecWithSubst(
    LucisParser::TypeSpecContext* typeSpec,
    const std::unordered_map<std::string, const TypeInfo*>& subst,
    unsigned& arrayDims) {
    // If the typeSpec is a bare IDENTIFIER that matches a type param, substitute it
    if (!typeSpec->LBRACKET() && !typeSpec->STAR() && !typeSpec->LT() &&
        !typeSpec->VEC() && !typeSpec->MAP() && !typeSpec->SET() &&
        !typeSpec->TUPLE() && !typeSpec->AUTO() && !typeSpec->fnTypeSpec()) {
        auto name = typeSpec->getText();
        auto it = subst.find(name);
        if (it != subst.end()) {
            arrayDims = 0;
            return it->second;
        }
    }
    // For pointer types, substitute into the inner type
    if (typeSpec->STAR() && typeSpec->typeSpec().size() == 1) {
        unsigned innerDims = 0;
        auto* inner = resolveTypeSpecWithSubst(typeSpec->typeSpec(0), subst, innerDims);
        if (!inner) return nullptr;
        // Preserve pointer-to-array shape under generic substitution as well.
        arrayDims = innerDims;
        return getPointerType(inner);
    }
    // For array types, substitute into the element type
    if (typeSpec->LBRACKET() && typeSpec->typeSpec().size() >= 1) {
        arrayDims = 0;
        auto* outerSpec = typeSpec;
        while (outerSpec && outerSpec->LBRACKET()) {
            arrayDims++;
            outerSpec = outerSpec->typeSpec().empty() ? nullptr : outerSpec->typeSpec(0);
        }
        unsigned elemDims = 0;
        auto* elemType = resolveTypeSpecWithSubst(outerSpec, subst, elemDims);
        return elemType;
    }
    // For generic instantiations like Node<T> where T is a type param
    if (typeSpec->LT()) {
        std::string innerBaseName;
        if (typeSpec->VEC()) innerBaseName = "Vec";
        else if (typeSpec->MAP()) innerBaseName = "Map";
        else if (typeSpec->SET()) innerBaseName = "Set";
        else if (typeSpec->IDENTIFIER()) innerBaseName = typeSpec->IDENTIFIER()->getText();
        if (innerBaseName.empty())
            return resolveTypeSpec(typeSpec, arrayDims);

        // Resolve all type args with substitution
        auto typeArgSpecs = typeSpec->typeSpec();
        std::vector<const TypeInfo*> resolvedArgs;
        for (auto* argSpec : typeArgSpecs) {
            unsigned argDims = 0;
            auto* argTI = resolveTypeSpecWithSubst(argSpec, subst, argDims);
            if (!argTI) return nullptr;
            resolvedArgs.push_back(argTI);
        }

        // Check if it's a user-defined generic struct
        auto structIt = genericStructTemplates_.find(innerBaseName);
        if (structIt != genericStructTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericStruct(innerBaseName, structIt->second,
                                             resolvedArgs, typeSpec);
        }
        auto unionIt = genericUnionTemplates_.find(innerBaseName);
        if (unionIt != genericUnionTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericUnion(innerBaseName, unionIt->second,
                                           resolvedArgs, typeSpec);
        }
        auto enumIt = genericEnumTemplates_.find(innerBaseName);
        if (enumIt != genericEnumTemplates_.end()) {
            arrayDims = 0;
            return instantiateGenericEnum(innerBaseName, enumIt->second,
                                          resolvedArgs, typeSpec);
        }

        // Built-in/generic extended types (Vec/Map/Set/Task/etc.) with substituted args
        if (auto* extDesc = extTypeRegistry_.lookup(innerBaseName)) {
            if (extDesc->genericArity == 1) {
                if (resolvedArgs.size() != 1) {
                    error(typeSpec, "'" + innerBaseName + "' expects 1 type parameter, got " +
                                        std::to_string(resolvedArgs.size()));
                    return nullptr;
                }
                auto* elemType = resolvedArgs[0];
                if (!elemType) return nullptr;
                if (elemType->kind == TypeKind::Extended &&
                    (innerBaseName == "Vec" || innerBaseName == "Map" || innerBaseName == "Set")) {
                    error(typeSpec, "nested collection types are not supported: '" +
                                        innerBaseName + "<" + elemType->name + ">'");
                    return nullptr;
                }

                auto fullName = innerBaseName + "<" + elemType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) {
                        arrayDims = 0;
                        return dt.get();
                    }
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = elemType->builtinSuffix;
                ti->elementType = elemType;
                ti->extendedKind = innerBaseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                arrayDims = 0;
                return raw;
            }

            if (extDesc->genericArity == 2) {
                if (resolvedArgs.size() != 2) {
                    error(typeSpec, "'" + innerBaseName + "' expects 2 type parameters, got " +
                                        std::to_string(resolvedArgs.size()));
                    return nullptr;
                }
                auto* keyType = resolvedArgs[0];
                auto* valType = resolvedArgs[1];
                if (!keyType || !valType) return nullptr;
                if (keyType->kind == TypeKind::Extended || valType->kind == TypeKind::Extended) {
                    error(typeSpec, "nested collection types are not supported for '" +
                                        innerBaseName + "'");
                    return nullptr;
                }

                auto fullName = innerBaseName + "<" + keyType->name + ", " + valType->name + ">";
                for (auto& dt : dynamicTypes_) {
                    if (dt->name == fullName) {
                        arrayDims = 0;
                        return dt.get();
                    }
                }
                auto ti = std::make_unique<TypeInfo>();
                ti->name = fullName;
                ti->kind = TypeKind::Extended;
                ti->bitWidth = 0;
                ti->isSigned = false;
                ti->builtinSuffix = keyType->builtinSuffix + "_" + valType->builtinSuffix;
                ti->keyType = keyType;
                ti->valueType = valType;
                ti->extendedKind = innerBaseName;
                const TypeInfo* raw = ti.get();
                dynamicTypes_.push_back(std::move(ti));
                arrayDims = 0;
                return raw;
            }
        }

        // Otherwise fall back to normal resolution (built-in generics, etc.)
    }
    // Function type: fn(params) -> ret — resolve child types with substitution
    if (typeSpec->fnTypeSpec()) {
        auto* fnSpec = typeSpec->fnTypeSpec();
        auto specs = fnSpec->typeSpec();
        if (specs.empty()) return nullptr;
        unsigned retDims = 0;
        auto* retType = resolveTypeSpecWithSubst(specs.back(), subst, retDims);
        if (!retType) return nullptr;
        std::vector<const TypeInfo*> paramTypes;
        for (size_t i = 0; i + 1 < specs.size(); i++) {
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(specs[i], subst, pDims);
            if (!pType) return nullptr;
            paramTypes.push_back(pType);
        }
        return makeFunctionType(retType, paramTypes);
    }
    // Default: normal resolution (no substitution needed for this node)
    return resolveTypeSpec(typeSpec, arrayDims);
}

const TypeInfo* Checker::resolveTypeParamConstraint(const std::string& constraintName,
                                                      antlr4::ParserRuleContext* ctx) {
    // Supported constraints: numeric, integer, float, bool, string, any
    // Return nullptr for "any".
    if (constraintName == "any" || constraintName == "Any") return nullptr;

    // Constraint aliases that are not concrete language types in TypeRegistry.
    static const TypeInfo numericConstraint = [] {
        TypeInfo ti;
        ti.name = "numeric";
        ti.kind = TypeKind::Integer;
        return ti;
    }();
    static const TypeInfo integerConstraint = [] {
        TypeInfo ti;
        ti.name = "integer";
        ti.kind = TypeKind::Integer;
        return ti;
    }();
    static const TypeInfo floatConstraint = [] {
        TypeInfo ti;
        ti.name = "float";
        ti.kind = TypeKind::Float;
        return ti;
    }();
    static const TypeInfo signedConstraint = [] {
        TypeInfo ti;
        ti.name = "signed";
        ti.kind = TypeKind::Integer;
        ti.isSigned = true;
        return ti;
    }();
    static const TypeInfo unsignedConstraint = [] {
        TypeInfo ti;
        ti.name = "unsigned";
        ti.kind = TypeKind::Integer;
        ti.isSigned = false;
        return ti;
    }();
    static const TypeInfo boolConstraint = [] {
        TypeInfo ti;
        ti.name = "bool";
        ti.kind = TypeKind::Bool;
        return ti;
    }();
    static const TypeInfo stringConstraint = [] {
        TypeInfo ti;
        ti.name = "string";
        ti.kind = TypeKind::String;
        return ti;
    }();
    if (constraintName == "numeric" || constraintName == "Numeric")
        return &numericConstraint;
    if (constraintName == "integer" || constraintName == "Integer")
        return &integerConstraint;
    if (constraintName == "float" || constraintName == "Float")
        return &floatConstraint;
    if (constraintName == "signed" || constraintName == "Signed")
        return &signedConstraint;
    if (constraintName == "unsigned" || constraintName == "Unsigned")
        return &unsignedConstraint;
    if (constraintName == "bool" || constraintName == "Bool")
        return &boolConstraint;
    if (constraintName == "string" || constraintName == "String")
        return &stringConstraint;

    auto* ti = typeRegistry_.lookup(constraintName);
    if (!ti) {
        error(ctx, "unknown type constraint '" + constraintName + "'");
        return nullptr;
    }
    return ti;
}

bool Checker::satisfiesConstraint(const TypeInfo* typeArg,
                                   const TypeInfo* constraint,
                                   const std::string& paramName,
                                   antlr4::ParserRuleContext* ctx) {
    if (!constraint) return true; // no constraint = any type is OK
    // Simple constraint check: typeArg must be the same kind or compatible
    if (constraint->name == "numeric") {
        if (typeArg->kind != TypeKind::Integer && typeArg->kind != TypeKind::Float) {
            error(ctx, "type argument for '" + paramName + "' must be numeric, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "integer") {
        if (typeArg->kind != TypeKind::Integer) {
            error(ctx, "type argument for '" + paramName + "' must be an integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "float") {
        if (typeArg->kind != TypeKind::Float) {
            error(ctx, "type argument for '" + paramName + "' must be a float, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "signed") {
        if (typeArg->kind != TypeKind::Integer || !typeArg->isSigned) {
            error(ctx, "type argument for '" + paramName + "' must be a signed integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "unsigned") {
        if (typeArg->kind != TypeKind::Integer || typeArg->isSigned) {
            error(ctx, "type argument for '" + paramName + "' must be an unsigned integer, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "bool") {
        if (typeArg->kind != TypeKind::Bool) {
            error(ctx, "type argument for '" + paramName + "' must be a bool, got '" +
                       typeArg->name + "'");
            return false;
        }
    } else if (constraint->name == "string") {
        if (typeArg->kind != TypeKind::String) {
            error(ctx, "type argument for '" + paramName + "' must be a string, got '" +
                       typeArg->name + "'");
            return false;
        }
    }
    return true;
}

const TypeInfo* Checker::instantiateGenericStruct(
    const std::string& baseName,
    const GenericStructTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic struct '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    // Check dynamic type cache first (already instantiated)
    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    // Also check type registry (registered during instantiation skeleton)
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    // Cycle detection
    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    // Build substitution map: T → int32, K → string, etc.
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        // Validate constraint if present
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            // Has constraint: T: numeric
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    // Register skeleton for self-referencing fields (e.g., *Node<T> inside Node<T>)
    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Struct;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericStructDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    // Instantiate fields with substitution
    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* field : tmpl.decl->structField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpecWithSubst(field->typeSpec(), subst, fieldDims);
        if (!fieldTI) {
            error(field, "cannot resolve field type in generic struct '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            return nullptr;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                         "' in generic struct '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    // Register concrete type (updates the skeleton registered above)
    typeRegistry_.registerType(std::move(ti));
    const TypeInfo* result = typeRegistry_.lookup(mangledName);

    // If there's a generic extend block for this struct, instantiate methods too
    auto extendIt = genericExtendTemplates_.find(baseName);
    if (extendIt != genericExtendTemplates_.end()) {
        auto& extTmpl = extendIt->second;
        for (auto* method : extTmpl.decl->extendMethod()) {
            auto methodName = method->IDENTIFIER(0)->getText();

            unsigned retDims = 0;
            auto* retType = resolveTypeSpecWithSubst(method->typeSpec(), subst, retDims);
            if (!retType) continue;

            StructMethodInfo info;
            info.name = methodName;
            info.returnType = retType;
            info.isStatic = (method->AMPERSAND() == nullptr);

            std::vector<LucisParser::ParamContext*> params;
            if (info.isStatic) {
                if (auto* pl = method->paramList())
                    params = pl->param();
            } else {
                params = method->param();
            }

            for (auto* param : params) {
                unsigned pDims = 0;
                auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
                if (!pType) continue;
                info.paramTypes.push_back(pType);
            }

            structMethods_[mangledName].push_back(std::move(info));
        }
    }

    instantiatingGenerics_.erase(mangledName);
    return result;
}

const TypeInfo* Checker::instantiateGenericUnion(
    const std::string& baseName,
    const GenericUnionTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic union '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Union;
    skeleton.bitWidth = 0;
    skeleton.isSigned = false;
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericStructDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* field : tmpl.decl->unionField()) {
        unsigned fieldDims = 0;
        auto* fieldTI = resolveTypeSpecWithSubst(field->typeSpec(), subst, fieldDims);
        if (!fieldTI) {
            error(field, "cannot resolve field type in generic union '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            return nullptr;
        }
        auto fieldName = field->IDENTIFIER()->getText();
        if (!seen.insert(fieldName).second) {
            error(field, "duplicate field '" + fieldName +
                         "' in generic union '" + baseName + "'");
            instantiatingGenerics_.erase(mangledName);
            continue;
        }
        ti.fields.push_back({ fieldName, fieldTI, fieldDims });
    }

    typeRegistry_.registerType(std::move(ti));
    instantiatingGenerics_.erase(mangledName);
    return typeRegistry_.lookup(mangledName);
}

const TypeInfo* Checker::instantiateGenericEnum(
    const std::string& baseName,
    const GenericEnumTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic enum '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    for (auto& dt : dynamicTypes_) {
        if (dt->name == mangledName)
            return dt.get();
    }
    if (auto* existing = typeRegistry_.lookup(mangledName))
        return existing;

    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    TypeInfo skeleton;
    skeleton.name = mangledName;
    skeleton.kind = TypeKind::Enum;
    skeleton.bitWidth = 32;
    skeleton.isSigned = false;
    skeleton.builtinSuffix = "i32";
    skeleton.isGenericInstance = true;
    skeleton.genericBaseName = baseName;
    skeleton.typeParamNames = tmpl.typeParams;
    skeleton.typeArgs = typeArgs;
    skeleton.genericEnumDecl = static_cast<antlr4::ParserRuleContext*>(tmpl.decl);
    typeRegistry_.registerType(skeleton);

    TypeInfo ti = skeleton;
    std::unordered_set<std::string> seen;
    for (auto* variantDecl : tmpl.decl->enumVariant()) {
        auto variantName = variantDecl->IDENTIFIER()->getText();
        if (!seen.insert(variantName).second) {
            error(variantDecl, "duplicate enum variant '" + variantName +
                               "' in generic enum '" + baseName + "'");
            continue;
        }

        ti.enumVariants.push_back(variantName);

        EnumVariantInfo info;
        info.name = variantName;
        info.discriminant = static_cast<unsigned>(ti.enumVariantInfos.size());

        if (variantDecl->LPAREN()) {
            info.payloadKind = EnumPayloadKind::Tuple;
            auto payloadTypes = variantDecl->typeSpec();
            for (size_t i = 0; i < payloadTypes.size(); i++) {
                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpecWithSubst(payloadTypes[i], subst, fieldDims);
                if (!fieldTI) {
                    error(payloadTypes[i], "cannot resolve payload type in generic enum '" +
                                           baseName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                std::vector<unsigned> fieldSizes;
                auto* spec = payloadTypes[i];
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadTypes[i],
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          baseName + "::" + variantName + "' or switch to 'vec<T>'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                info.payloadFields.push_back({"_" + std::to_string(i), fieldTI, fieldDims, fieldSizes});
            }
        } else if (variantDecl->LBRACE()) {
            info.payloadKind = EnumPayloadKind::Named;
            std::unordered_set<std::string> fieldSeen;
            for (auto* payloadField : variantDecl->enumPayloadField()) {
                auto fieldName = payloadField->IDENTIFIER()->getText();
                if (!fieldSeen.insert(fieldName).second) {
                    error(payloadField, "duplicate payload field '" + fieldName +
                                       "' in enum variant '" + baseName + "::" + variantName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                unsigned fieldDims = 0;
                auto* fieldTI = resolveTypeSpecWithSubst(payloadField->typeSpec(), subst, fieldDims);
                if (!fieldTI) {
                    error(payloadField, "cannot resolve payload type in generic enum '" +
                                        baseName + "'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                std::vector<unsigned> fieldSizes;
                auto* spec = payloadField->typeSpec();
                while (spec && spec->LBRACKET()) {
                    if (spec->INT_LIT())
                        fieldSizes.push_back(static_cast<unsigned>(std::stoul(spec->INT_LIT()->getText())));
                    spec = spec->typeSpec(0);
                }
                if (fieldDims > 0 && fieldSizes.empty()) {
                    error(payloadField,
                          "enum payload arrays must have fixed size; use '[N]T' in variant '" +
                          baseName + "::" + variantName + "' or switch to 'vec<T>'");
                    instantiatingGenerics_.erase(mangledName);
                    return nullptr;
                }

                info.payloadFields.push_back({fieldName, fieldTI, fieldDims, fieldSizes});
            }
        }

        ti.enumVariantInfos.push_back(std::move(info));
    }

    typeRegistry_.registerType(std::move(ti));
    instantiatingGenerics_.erase(mangledName);
    return typeRegistry_.lookup(mangledName);
}

const TypeInfo* Checker::instantiateGenericFunc(
    const std::string& baseName,
    const GenericFuncTemplate& tmpl,
    const std::vector<const TypeInfo*>& typeArgs,
    antlr4::ParserRuleContext* ctx) {

    if (typeArgs.size() != tmpl.typeParams.size()) {
        error(ctx, "generic function '" + baseName + "' expects " +
                   std::to_string(tmpl.typeParams.size()) + " type parameter(s), got " +
                   std::to_string(typeArgs.size()));
        return nullptr;
    }

    auto mangledName = mangleGenericName(baseName, typeArgs);

    // Already instantiated? Return the function's return type.
    if (functions_.count(mangledName)) {
        auto* ft = functions_[mangledName];
        return ft ? ft->returnType : nullptr;
    }

    // Cycle detection
    if (instantiatingGenerics_.count(mangledName)) {
        error(ctx, "recursive generic function instantiation detected for '" + mangledName + "'");
        return nullptr;
    }
    instantiatingGenerics_.insert(mangledName);

    // Build substitution map
    std::unordered_map<std::string, const TypeInfo*> subst;
    for (size_t i = 0; i < tmpl.typeParams.size(); i++) {
        auto* tpCtx = tmpl.decl->typeParamList()->typeParam(i);
        if (tpCtx->COLON()) {
            auto constraintName = tpCtx->IDENTIFIER(1)->getText();
            auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
            if (!satisfiesConstraint(typeArgs[i], constraint, tmpl.typeParams[i], ctx)) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
        }
        subst[tmpl.typeParams[i]] = typeArgs[i];
    }

    // Resolve return type with substitution
    unsigned retDims = 0;
    auto* retType = resolveTypeSpecWithSubst(tmpl.decl->typeSpec(), subst, retDims);
    if (!retType) {
        instantiatingGenerics_.erase(mangledName);
        return nullptr;
    }

    // Resolve parameter types with substitution
    std::vector<const TypeInfo*> paramTypes;
    bool isVariadic = false;
    bool isTypedVariadic = false;
    const TypeInfo* variadicElemType = nullptr;
    if (auto* paramList = tmpl.decl->paramList()) {
        for (auto* param : paramList->param()) {
            if (param->SPREAD() && !param->typeSpec()) {
                // Untyped variadic ... — skip, no type to add
                isVariadic = true;
                break;
            }
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
            if (!pType) {
                instantiatingGenerics_.erase(mangledName);
                return nullptr;
            }
            paramTypes.push_back(pType);
            if (param->SPREAD()) {
                // Typed variadic: T ...name
                isVariadic = true;
                isTypedVariadic = true;
                variadicElemType = pType;
            }
        }
    }

    auto* funcType = makeFunctionType(retType, paramTypes, isVariadic, variadicElemType);
    functions_[mangledName] = funcType;

    // Check the function body with the substituted locals
    // We save/restore locals_ since this is a nested instantiation
    auto savedLocals = locals_;
    auto savedTypeSubst = activeTypeSubst_;
    activeTypeSubst_ = subst;
    locals_.clear();
    if (auto* paramList = tmpl.decl->paramList()) {
        for (auto* param : paramList->param()) {
            if (param->SPREAD() && !param->IDENTIFIER())
                continue;
            auto paramName = param->IDENTIFIER()->getText();
            unsigned pDims = 0;
            auto* pType = resolveTypeSpecWithSubst(param->typeSpec(), subst, pDims);
            if (!pType) continue;
            if (param->SPREAD())
                pDims = 1;
            locals_[paramName] = { pType, pDims, {}, true, true, nullptr };
        }
    }

    // Temporarily add type params as dummy locals so references inside the body resolve
    // (they'll be substituted by resolveTypeSpec when body expressions are checked)
    // We don't need this — body checking happens via resolveTypeSpec normally since
    // subst is only used for type specs, not for body expression checking.
    // The body is checked with the concrete types already registered.
    checkBlock(tmpl.decl->block(), retType);

    locals_ = savedLocals;
    activeTypeSubst_ = savedTypeSubst;
    instantiatingGenerics_.erase(mangledName);

    // Return the function's return type (not the function type itself)
    return retType;
}

std::optional<std::vector<const TypeInfo*>> Checker::inferGenericTypeArgs(
    const std::string& displayName,
    const std::vector<std::string>& typeParams,
    LucisParser::TypeParamListContext* typeParamList,
    const std::vector<LucisParser::ParamContext*>& formalParams,
    const std::vector<const TypeInfo*>& argTypes,
    antlr4::ParserRuleContext* ctx) {

    if (formalParams.size() != argTypes.size())
        return std::nullopt;

    std::unordered_set<std::string> genericParamSet(typeParams.begin(), typeParams.end());
    std::unordered_map<std::string, const TypeInfo*> inferred;

    for (size_t i = 0; i < formalParams.size(); i++) {
        auto* formalParam = formalParams[i];
        auto* actualType = argTypes[i];
        if (!formalParam || !formalParam->typeSpec())
            continue;

        if (!typeSpecMentionsGenericParam(formalParam->typeSpec(), genericParamSet))
            continue;

        bool emittedSpecificError = false;
        if (!unifyGenericTypeArg(formalParam->typeSpec(), actualType, genericParamSet,
                                 inferred, ctx, displayName, emittedSpecificError)) {
            if (!emittedSpecificError) {
                error(ctx, "cannot infer generic type parameter(s) for '" +
                             displayName + "' from argument " +
                             std::to_string(i + 1) + " of type '" +
                             (actualType ? actualType->name : "<unknown>") + "'");
            }
            return std::nullopt;
        }
    }

    std::vector<const TypeInfo*> typeArgs;
    for (size_t i = 0; i < typeParams.size(); i++) {
        auto it = inferred.find(typeParams[i]);
        if (it == inferred.end()) {
            error(ctx, "cannot infer generic type parameter '" + typeParams[i] +
                         "' for '" + displayName + "'");
            return std::nullopt;
        }

        if (typeParamList && i < typeParamList->typeParam().size()) {
            auto* tpCtx = typeParamList->typeParam(i);
            if (tpCtx->COLON()) {
                auto constraintName = tpCtx->IDENTIFIER(1)->getText();
                auto* constraint = resolveTypeParamConstraint(constraintName, ctx);
                if (!satisfiesConstraint(it->second, constraint, typeParams[i], ctx))
                    return std::nullopt;
            }
        }

        typeArgs.push_back(it->second);
    }

    return typeArgs;
}

bool Checker::unifyGenericTypeArg(
    LucisParser::TypeSpecContext* formalType,
    const TypeInfo* actualType,
    const std::unordered_set<std::string>& genericParams,
    std::unordered_map<std::string, const TypeInfo*>& inferred,
    antlr4::ParserRuleContext* ctx,
    const std::string& displayName,
    bool& emittedSpecificError) {

    if (!formalType) return true;
    if (!typeSpecMentionsGenericParam(formalType, genericParams)) return true;
    if (!actualType) return false;

    if (!formalType->LBRACKET() && !formalType->STAR() && !formalType->LT() &&
        !formalType->VEC() && !formalType->MAP() && !formalType->SET() &&
        !formalType->TUPLE() && !formalType->AUTO() && !formalType->fnTypeSpec()) {
        auto name = formalType->getText();
        if (genericParams.count(name)) {
            auto it = inferred.find(name);
            if (it == inferred.end()) {
                inferred[name] = actualType;
                return true;
            }
            if (it->second != actualType && it->second->name != actualType->name) {
                emittedSpecificError = true;
                error(ctx, "ambiguous generic inference for '" + displayName +
                             "': '" + name + "' was inferred as both '" +
                             it->second->name + "' and '" + actualType->name + "'");
                return false;
            }
        }
        return true;
    }

    if (formalType->STAR() && formalType->typeSpec().size() == 1) {
        if (actualType->kind != TypeKind::Pointer || !actualType->pointeeType)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->pointeeType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->VEC()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Vec" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->SET()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Set" ||
            !actualType->elementType || formalType->typeSpec().size() != 1)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->elementType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->MAP()) {
        if (actualType->kind != TypeKind::Extended || actualType->extendedKind != "Map" ||
            !actualType->keyType || !actualType->valueType || formalType->typeSpec().size() != 2)
            return false;
        return unifyGenericTypeArg(formalType->typeSpec(0), actualType->keyType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError)
            && unifyGenericTypeArg(formalType->typeSpec(1), actualType->valueType,
                                   genericParams, inferred, ctx, displayName,
                                   emittedSpecificError);
    }

    if (formalType->TUPLE()) {
        if (actualType->kind != TypeKind::Tuple ||
            actualType->tupleElements.size() != formalType->typeSpec().size())
            return false;
        for (size_t i = 0; i < formalType->typeSpec().size(); i++) {
            if (!unifyGenericTypeArg(formalType->typeSpec(i), actualType->tupleElements[i],
                                     genericParams, inferred, ctx, displayName,
                                     emittedSpecificError))
                return false;
        }
        return true;
    }

    if (formalType->LT()) {
        auto baseName = formalType->IDENTIFIER()->getText();
        std::vector<const TypeInfo*> actualArgs;

        if (actualType->isGenericInstance && actualType->genericBaseName == baseName) {
            actualArgs = actualType->typeArgs;
        } else if (actualType->kind == TypeKind::Extended && actualType->extendedKind == baseName) {
            if (formalType->typeSpec().size() == 1 && actualType->elementType) {
                actualArgs.push_back(actualType->elementType);
            } else if (formalType->typeSpec().size() == 2 && actualType->keyType && actualType->valueType) {
                actualArgs.push_back(actualType->keyType);
                actualArgs.push_back(actualType->valueType);
            } else {
                return false;
            }
        } else {
            return false;
        }

        if (actualArgs.size() != formalType->typeSpec().size())
            return false;

        for (size_t i = 0; i < formalType->typeSpec().size(); i++) {
            if (!unifyGenericTypeArg(formalType->typeSpec(i), actualArgs[i], genericParams,
                                     inferred, ctx, displayName, emittedSpecificError))
                return false;
        }
        return true;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 1: SemanticDB parallel population
// ═══════════════════════════════════════════════════════════════════════════════

void Checker::initSemanticDB() {
    if (!semanticDB_) return;

    auto addPrim = [&](const char* name, semantic::DeclKind kind,
                       unsigned bw, bool signd, const char* suffix) {
        auto d = std::make_unique<semantic::PrimitiveDecl>(kind);
        d->name          = name;
        d->bitWidth      = bw;
        d->isSigned      = signd;
        d->builtinSuffix = suffix;
        semanticDB_->registerBuiltin(std::move(d));
    };

    addPrim("void",    semantic::DeclKind::Void,    0, true,  "");
    addPrim("bool",    semantic::DeclKind::Bool,    1, true,  "bool");
    addPrim("char",    semantic::DeclKind::Char,    8, true,  "char");
    addPrim("string",  semantic::DeclKind::String,  0, true,  "str");
    addPrim("int1",   semantic::DeclKind::Integer, 1,   true,  "i1");
    addPrim("int8",   semantic::DeclKind::Integer, 8,   true,  "i8");
    addPrim("int16",  semantic::DeclKind::Integer, 16,  true,  "i16");
    addPrim("int32",  semantic::DeclKind::Integer, 32,  true,  "i32");
    addPrim("int64",  semantic::DeclKind::Integer, 64,  true,  "i64");
    addPrim("int128", semantic::DeclKind::Integer, 128, true,  "i128");
    addPrim("intinf", semantic::DeclKind::Integer, 256, true,  "i128");
    addPrim("isize",  semantic::DeclKind::Integer, 0,   true,  "i64");
    addPrim("uint1",   semantic::DeclKind::Integer, 1,   false, "u1");
    addPrim("uint8",   semantic::DeclKind::Integer, 8,   false, "u8");
    addPrim("uint16",  semantic::DeclKind::Integer, 16,  false, "u16");
    addPrim("uint32",  semantic::DeclKind::Integer, 32,  false, "u32");
    addPrim("uint64",  semantic::DeclKind::Integer, 64,  false, "u64");
    addPrim("uint128", semantic::DeclKind::Integer, 128, false, "u128");
    addPrim("usize",   semantic::DeclKind::Integer, 0,   false, "u64");
    addPrim("float32",  semantic::DeclKind::Float, 32,  true, "f32");
    addPrim("float64",  semantic::DeclKind::Float, 64,  true, "f64");
    addPrim("float80",  semantic::DeclKind::Float, 80,  true, "f80");
    addPrim("float128", semantic::DeclKind::Float, 128, true, "f128");
    addPrim("double",   semantic::DeclKind::Float, 64,  true, "f64");

    {
        auto d = std::make_unique<semantic::PrimitiveDecl>(semantic::DeclKind::VAList);
        d->name = "va_list";
        d->builtinSuffix = "valist";
        semanticDB_->registerBuiltin(std::move(d));
    }

    auto addExtended = [&](const char* name, const char* kind,
                           const char* elem, const char* key,
                           const char* val, const char* cpfx) {
        auto d = std::make_unique<semantic::ExtendedDecl>();
        d->name         = name;
        d->extendedKind = kind;
        d->cPrefix      = cpfx;
        if (elem) d->elementType = const_cast<semantic::Decl*>(semanticDB_->lookupAny(elem));
        if (key)  d->keyType     = const_cast<semantic::Decl*>(semanticDB_->lookupAny(key));
        if (val)  d->valueType   = const_cast<semantic::Decl*>(semanticDB_->lookupAny(val));
        semanticDB_->registerBuiltin(std::move(d));
    };

    addExtended("Vec",   "Vec",   "void", nullptr, nullptr, "lucis_vec");
    addExtended("Map",   "Map",   nullptr, "void", "void", "lucis_map");
    addExtended("Set",   "Set",   "void", nullptr, nullptr, "lucis_set");
    addExtended("Task",  "Task",  "void", nullptr, nullptr, "lucis_task");

    {
        auto d = std::make_unique<semantic::ExtendedDecl>();
        d->name         = "Mutex";
        d->extendedKind = "Mutex";
        semanticDB_->registerBuiltin(std::move(d));
    }

    {
        auto ed = std::make_unique<semantic::StructDecl>();
        ed->name = "Error";
        auto* strTI = semanticDB_->lookupAny("string");
        auto* i32TI = semanticDB_->lookupAny("int32");
        semantic::FieldInfo f1; f1.name = "message"; f1.type = const_cast<semantic::Decl*>(strTI);
        semantic::FieldInfo f2; f2.name = "file";    f2.type = const_cast<semantic::Decl*>(strTI); f2.autoFill = true;
        semantic::FieldInfo f3; f3.name = "line";    f3.type = const_cast<semantic::Decl*>(i32TI); f3.autoFill = true;
        semantic::FieldInfo f4; f4.name = "column";  f4.type = const_cast<semantic::Decl*>(i32TI); f4.autoFill = true;
        ed->fields.push_back(std::move(f1));
        ed->fields.push_back(std::move(f2));
        ed->fields.push_back(std::move(f3));
        ed->fields.push_back(std::move(f4));
        semanticDB_->registerBuiltin(std::move(ed));
    }
}

semantic::SourceLocation Checker::toSemanticLoc(
    antlr4::ParserRuleContext* ctx) const {
    semantic::SourceLocation loc;
    if (ctx) {
        loc.file   = currentFile_;
        loc.line   = static_cast<unsigned>(ctx->getStart()->getLine());
        loc.column = static_cast<unsigned>(ctx->getStart()->getCharPositionInLine());
    }
    return loc;
}

semantic::DeclKind Checker::toSemanticKind(TypeKind tk) const {
    switch (tk) {
    case TypeKind::Integer:  return semantic::DeclKind::Integer;
    case TypeKind::Float:    return semantic::DeclKind::Float;
    case TypeKind::Bool:     return semantic::DeclKind::Bool;
    case TypeKind::Char:     return semantic::DeclKind::Char;
    case TypeKind::Void:     return semantic::DeclKind::Void;
    case TypeKind::String:   return semantic::DeclKind::String;
    case TypeKind::Struct:   return semantic::DeclKind::Struct;
    case TypeKind::Union:    return semantic::DeclKind::Union;
    case TypeKind::Enum:     return semantic::DeclKind::Enum;
    case TypeKind::Pointer:  return semantic::DeclKind::Pointer;
    case TypeKind::Function: return semantic::DeclKind::Function;
    case TypeKind::Extended: return semantic::DeclKind::Extended;
    case TypeKind::Tuple:    return semantic::DeclKind::Tuple;
    case TypeKind::VAList:   return semantic::DeclKind::VAList;
    }
    return semantic::DeclKind::Void;
}

semantic::FieldInfo Checker::toSemanticField(const ::FieldInfo& f) const {
    semantic::FieldInfo sf;
    sf.name       = f.name;
    sf.arrayDims  = f.arrayDims;
    sf.arraySizes = f.arraySizes;
    sf.autoFill   = f.autoFill;
    if (f.typeInfo && semanticDB_)
        sf.type = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(f.typeInfo->name));
    return sf;
}

semantic::VariantInfo Checker::toSemanticVariant(const EnumVariantInfo& v) const {
    semantic::VariantInfo sv;
    sv.name         = v.name;
    sv.discriminant = v.discriminant;
    switch (v.payloadKind) {
    case EnumPayloadKind::Unit:  sv.payloadKind = semantic::VariantPayloadKind::Unit;  break;
    case EnumPayloadKind::Tuple: sv.payloadKind = semantic::VariantPayloadKind::Tuple; break;
    case EnumPayloadKind::Named: sv.payloadKind = semantic::VariantPayloadKind::Named; break;
    }
    for (const auto& pf : v.payloadFields)
        sv.payloadFields.push_back(toSemanticField(pf));
    return sv;
}

semantic::MethodInfo Checker::toSemanticMethod(const StructMethodInfo& m) const {
    semantic::MethodInfo sm;
    sm.name     = m.name;
    sm.isStatic = m.isStatic;
    if (m.returnType && semanticDB_)
        sm.returnType = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(m.returnType->name));
    for (auto* pt : m.paramTypes) {
        semantic::ParamInfo sp;
        if (pt && semanticDB_)
            sp.type = const_cast<semantic::Decl*>(
                semanticDB_->lookupAny(pt->name));
        sm.params.push_back(sp);
    }
    return sm;
}

std::unique_ptr<semantic::Decl> Checker::typeInfoToDecl(const TypeInfo& ti) {
    if (!semanticDB_) return nullptr;

    switch (ti.kind) {
    case TypeKind::Struct: {
        auto sd = std::make_unique<semantic::StructDecl>();
        sd->name       = ti.name;
        sd->modulePath = currentModulePath_;
        sd->dropTracked = ti.dropTracked;
        sd->moveOnly    = ti.moveOnly;
        if (ti.parentType)
            sd->parentName = ti.parentType->name;
        for (const auto& f : ti.fields)
            sd->fields.push_back(toSemanticField(f));
        return sd;
    }
    case TypeKind::Union: {
        auto ud = std::make_unique<semantic::UnionDecl>();
        ud->name       = ti.name;
        ud->modulePath = currentModulePath_;
        ud->dropTracked = ti.dropTracked;
        ud->moveOnly    = ti.moveOnly;
        for (const auto& f : ti.fields)
            ud->fields.push_back(toSemanticField(f));
        return ud;
    }
    case TypeKind::Enum: {
        auto ed = std::make_unique<semantic::EnumDecl>();
        ed->name       = ti.name;
        ed->modulePath = currentModulePath_;
        ed->dropTracked = ti.dropTracked;
        ed->moveOnly    = ti.moveOnly;
        for (const auto& v : ti.enumVariantInfos)
            ed->variants.push_back(toSemanticVariant(v));
        return ed;
    }
    default:
        return nullptr;
    }
}

void Checker::syncToSemanticDB_Struct(const TypeInfo& ti,
                                       const std::string& modulePath,
                                       antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Struct,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Union(const TypeInfo& ti,
                                      const std::string& modulePath,
                                      antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Union,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Enum(const TypeInfo& ti,
                                     const std::string& modulePath,
                                     antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Enum,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_TypeAlias(const TypeInfo& ti,
                                          const std::string& modulePath,
                                          antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Function(const std::string& name,
                                         const TypeInfo& funcType,
                                         const std::string& modulePath,
                                         antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto fd = std::make_unique<semantic::FunctionDecl>();
    fd->name       = name;
    fd->modulePath = modulePath;
    fd->loc        = toSemanticLoc(ctx);
    fd->isVariadic = funcType.isVariadic;
    if (funcType.returnType && semanticDB_)
        fd->returnType = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(funcType.returnType->name));
    for (auto* pt : funcType.paramTypes) {
        semantic::ParamInfo sp;
        if (pt && semanticDB_)
            sp.type = const_cast<semantic::Decl*>(
                semanticDB_->lookupAny(pt->name));
        fd->params.push_back(sp);
    }
    semanticDB_->registerFunction(std::move(fd));
}

void Checker::syncToSemanticDB_Extend(const std::string& structName,
                                       const std::vector<StructMethodInfo>& methods) {
    if (!semanticDB_) return;
    std::vector<semantic::MethodInfo> smethods;
    for (const auto& m : methods)
        smethods.push_back(toSemanticMethod(m));
    semanticDB_->mergeExtendMethods(structName, std::move(smethods));
}

void Checker::syncToSemanticDB_GenericStruct(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::StructDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::StructDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericUnion(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::UnionDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::UnionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericEnum(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::EnumDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::EnumDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericFunc(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::FunctionDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::FunctionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericExtend(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::ExtendDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::FunctionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericInstantiation(
    const std::string& mangledName,
    const TypeInfo& concreteTI) {
    if (!semanticDB_) return;
    auto decl = typeInfoToDecl(concreteTI);
    if (decl) {
        decl->name = mangledName;
        semanticDB_->registerType(std::move(decl));
    }
}
