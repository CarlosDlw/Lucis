#include "checkers/Checker.h"
#include "checkers/Checker_helpers.h"

#include "generated/LucisLexer.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"
#include "attributes/CfgPredicate.h"
#include "attributes/TargetInfo.h"

#include <algorithm>
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

    // If this variable is a parameter of the function currently being checked,
    // the callee took ownership of it. Record the parameter index so callers
    // consume the argument at the call site instead of freeing it themselves.
    if (!currentFnConsumeKey_.empty()) {
        auto pit = currentParamIndices_.find(name);
        if (pit != currentParamIndices_.end()) {
            auto& consumed = userFunctionConsumingArgs_[currentFnConsumeKey_];
            if (std::find(consumed.begin(), consumed.end(), pit->second) == consumed.end())
                consumed.push_back(pit->second);
        }
    }

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
    // Built-in functions: use builtin registry for consumption info
    auto* sig = calleeName.empty() ? nullptr : builtinRegistry_.lookup(calleeName);
    if (sig) {
        for (size_t i = 0; i < args.size(); ++i) {
            if (builtinRegistry_.argBorrows(calleeName, i)) continue;
            if (builtinRegistry_.argConsumes(calleeName, i))
                markExprAsMoved(args[i], whereCtx);
        }
        return;
    }

    // User-defined functions: check if any parameters are consumed (moved into return)
    auto it = userFunctionConsumingArgs_.find(calleeName);
    if (it != userFunctionConsumingArgs_.end()) {
        for (size_t paramIdx : it->second) {
            if (paramIdx < args.size())
                markExprAsMoved(args[paramIdx], whereCtx);
        }
    }

    // Record dependency edges for every argument that is a parameter of the
    // function currently being checked. This happens even when the callee's
    // consumption set is unknown yet (or may still grow), so the post-check
    // fixpoint can propagate ownership transfer through the call chain.
    for (size_t i = 0; i < args.size(); ++i)
        recordConsumeEdgeIfParam(calleeName, i, args[i]);
}



void Checker::recordConsumeEdgeIfParam(
    const std::string& calleeKey,
    size_t calleeArgIdx,
    LucisParser::ExpressionContext* argExpr) {
    if (currentFnConsumeKey_.empty() || calleeKey.empty()) return;
    auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(argExpr);
    if (!ident) return;
    auto pit = currentParamIndices_.find(ident->IDENTIFIER()->getText());
    if (pit == currentParamIndices_.end()) return;

    auto& edges = consumeEdges_[currentFnConsumeKey_];
    for (const auto& e : edges) {
        if (e.callerParamIdx == pit->second && e.calleeKey == calleeKey &&
            e.calleeArgIdx == calleeArgIdx)
            return;
    }
    edges.push_back({pit->second, calleeKey, calleeArgIdx});
}



void Checker::resolveConsumptionFixpoint() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& [callerKey, edges] : consumeEdges_) {
            for (const auto& e : edges) {
                auto cit = userFunctionConsumingArgs_.find(e.calleeKey);
                if (cit == userFunctionConsumingArgs_.end()) continue;
                const auto& calleeConsumed = cit->second;
                if (std::find(calleeConsumed.begin(), calleeConsumed.end(),
                              e.calleeArgIdx) == calleeConsumed.end())
                    continue;

                auto& consumed = userFunctionConsumingArgs_[callerKey];
                if (std::find(consumed.begin(), consumed.end(),
                              e.callerParamIdx) == consumed.end()) {
                    consumed.push_back(e.callerParamIdx);
                    changed = true;
                }
            }
        }
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


