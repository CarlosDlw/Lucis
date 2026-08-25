#include "IRBuilder/IRGen.h"
#include "IRBuilder/IRGen_helpers.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <vector>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <fstream>

#ifdef LLVM_VERSION_15_OR_NEWER
#  include <llvm/TargetParser/Host.h>
#else
#  include <llvm/Support/Host.h>
#endif

#include "generated/LucisLexer.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"

#include <cctype>
#include <iostream>



static std::string opTextToInternal(const std::string& text) {
    if (text == "+")   return "opAdd";
    if (text == "-")   return "opSub";
    if (text == "*")   return "opMul";
    if (text == "/")   return "opDiv";
    if (text == "%")   return "opMod";
    if (text == "==")  return "opEq";
    if (text == "!=")  return "opNe";
    if (text == "<")   return "opLt";
    if (text == ">")   return "opGt";
    if (text == "<=")  return "opLe";
    if (text == ">=")  return "opGe";
    if (text == "&")   return "opBitAnd";
    if (text == "|")   return "opBitOr";
    if (text == "^")   return "opBitXor";
    if (text == "<<")  return "opShl";
    if (text == ">>")  return "opShr";
    if (text == "&&")  return "opAnd";
    if (text == "||")  return "opOr";
    return "";
}

// ═══════════════════════════════════════════════════════════════════════
//  extend StructName { methods... }
// ═══════════════════════════════════════════════════════════════════════



static int suffixIntWidth(const std::string& suf) {
    if (suf == "i8" || suf == "u8") return 8;
    if (suf == "i16" || suf == "u16") return 16;
    if (suf == "i32" || suf == "u32") return 32;
    if (suf == "i64" || suf == "u64" || suf == "isize" || suf == "usize") return 64;
    if (suf == "i128" || suf == "u128") return 128;
    if (suf == "iinf") return 256;
    return 32; // fallback
}

// Map suffix → LLVM float type width in bits


static int suffixFloatWidth(const std::string& suf) {
    if (suf == "f32") return 32;
    if (suf == "f64") return 64;
    if (suf == "f80") return 80;
    if (suf == "f128") return 128;
    return 64; // fallback
}

/// Choose the smallest standard integer type (32, 64, 128, or 256 bits)
/// that can hold the given literal value.


static llvm::Type* literalIntType(llvm::LLVMContext& ctx, const llvm::APInt& ap) {
    unsigned bits = 32;
    unsigned sz = ap.getActiveBits() + 1;
    if (ap.isNegative()) {
        sz = (-ap).getActiveBits() + 1;
    }
    if (sz > 32) {
        if (sz <= 64) bits = 64;
        else if (sz <= 128) bits = 128;
        else bits = 256;
    }
    return llvm::Type::getIntNTy(ctx, bits);
}



std::any IRGen::visitSuffixedIntLitExpr(LucisParser::SuffixedIntLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_INT()->getText(), suffix);
    llvm::APInt ap(256, base, 10);
    int bits = suffixIntWidth(suffix);
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(bits)));
}

std::any IRGen::visitSuffixedHexLitExpr(LucisParser::SuffixedHexLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_HEX()->getText(), suffix);
    auto hex = base.substr(2); // strip 0x/0X
    llvm::APInt ap(256, hex, 16);
    int bits = suffixIntWidth(suffix);
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(bits)));
}

std::any IRGen::visitSuffixedOctLitExpr(LucisParser::SuffixedOctLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_OCT()->getText(), suffix);
    auto oct = base.substr(2); // strip 0o/0O
    llvm::APInt ap(256, oct, 8);
    int bits = suffixIntWidth(suffix);
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(bits)));
}

std::any IRGen::visitSuffixedBinLitExpr(LucisParser::SuffixedBinLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_BIN()->getText(), suffix);
    auto bin = base.substr(2); // strip 0b/0B
    llvm::APInt ap(256, bin, 2);
    int bits = suffixIntWidth(suffix);
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(bits)));
}

std::any IRGen::visitSuffixedFloatLitExpr(LucisParser::SuffixedFloatLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_FLOAT()->getText(), suffix);
    double v = std::stod(base);
    int bits = suffixFloatWidth(suffix);
    if (bits == 32) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFloatTy(*context_), static_cast<float>(v)));
    }
    if (bits == 80) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getX86_FP80Ty(*context_), v));
    }
    if (bits == 128) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFP128Ty(*context_), v));
    }
    return static_cast<llvm::Value*>(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), v));
}

std::any IRGen::visitSuffixedLeadingDotFloatExpr(
        LucisParser::SuffixedLeadingDotFloatExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_DOT_FLOAT()->getText(), suffix);
    double v = std::stod("0" + base); // base is like ".5" → "0.5"
    int bits = suffixFloatWidth(suffix);
    if (bits == 32) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFloatTy(*context_), static_cast<float>(v)));
    }
    if (bits == 80) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getX86_FP80Ty(*context_), v));
    }
    if (bits == 128) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFP128Ty(*context_), v));
    }
    return static_cast<llvm::Value*>(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), v));
}

std::any IRGen::visitSuffixedIntFloatExpr(LucisParser::SuffixedIntFloatExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_INT_FLOAT()->getText(), suffix);
    double v = std::stod(base);
    int bits = suffixFloatWidth(suffix);
    if (bits == 32) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFloatTy(*context_), static_cast<float>(v)));
    }
    if (bits == 80) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getX86_FP80Ty(*context_), v));
    }
    if (bits == 128) {
        return static_cast<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getFP128Ty(*context_), v));
    }
    return static_cast<llvm::Value*>(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), v));
}

std::any IRGen::visitSuffixedFloatIntExpr(LucisParser::SuffixedFloatIntExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string suffix;
    auto base = splitSuffix(ctx->SUFFIXED_FLOAT_INT()->getText(), suffix);
    double v = std::stod(base);
    int bits = suffixIntWidth(suffix);
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, static_cast<int64_t>(v)));
}

// ── Unsuffixed literal visitors ─────────────────────────────────────────



std::any IRGen::visitIntLitExpr(LucisParser::IntLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = stripUnderscores(ctx->INT_LIT()->getText());
    llvm::APInt ap(256, text, 10);
    unsigned bits = 32;
    unsigned sz = ap.getActiveBits() + 1;
    if (ap.isNegative()) {
        sz = (-ap).getActiveBits() + 1;
    }
    if (sz > 32) {
        if (sz <= 64) bits = 64;
        else if (sz <= 128) bits = 128;
        else bits = 256;
    }
    auto* ty = llvm::Type::getIntNTy(*context_, bits);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(bits)));
}

std::any IRGen::visitHexLitExpr(LucisParser::HexLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = stripUnderscores(ctx->HEX_LIT()->getText().substr(2)); // strip "0x"/"0X"
    llvm::APInt ap(256, text, 16);
    auto* ty = literalIntType(*context_, ap);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(ty->getIntegerBitWidth())));
}

std::any IRGen::visitOctLitExpr(LucisParser::OctLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = stripUnderscores(ctx->OCT_LIT()->getText().substr(2)); // strip "0o"/"0O"
    llvm::APInt ap(256, text, 8);
    auto* ty = literalIntType(*context_, ap);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(ty->getIntegerBitWidth())));
}

std::any IRGen::visitBinLitExpr(LucisParser::BinLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = stripUnderscores(ctx->BIN_LIT()->getText().substr(2)); // strip "0b"/"0B"
    llvm::APInt ap(256, text, 2);
    auto* ty = literalIntType(*context_, ap);
    return static_cast<llvm::Value*>(llvm::ConstantInt::get(ty, ap.sextOrTrunc(ty->getIntegerBitWidth())));
}

std::any IRGen::visitFloatLitExpr(LucisParser::FloatLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    double v = std::stod(stripUnderscores(ctx->FLOAT_LIT()->getText()));
    return static_cast<llvm::Value*>(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), v));
}

std::any IRGen::visitLeadingDotFloatLitExpr(
        LucisParser::LeadingDotFloatLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    double v = std::stod("0." + stripUnderscores(ctx->INT_LIT()->getText()));
    return static_cast<llvm::Value*>(
        llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), v));
}



std::any IRGen::visitBoolLitExpr(LucisParser::BoolLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    bool v = ctx->BOOL_LIT()->getText() == "true";
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), v ? 1 : 0));
}



static bool allHex(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}


static bool allOct(const std::string& s) {
    return !s.empty() && s.find_first_not_of("01234567") == std::string::npos;
}



static std::string utf8Encode(uint32_t cp) {
    std::string s;
    if (cp <= 0x7F) {
        s += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return s;
}



std::any IRGen::visitCharLitExpr(LucisParser::CharLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->CHAR_LIT()->getText();
    auto inner = raw.substr(1, raw.size() - 2);
    uint8_t ch;
    if (inner.size() >= 2 && inner[0] == '\\') {
        switch (inner[1]) {
        case 'n':  ch = '\n'; break;
        case 'r':  ch = '\r'; break;
        case 't':  ch = '\t'; break;
        case '\\': ch = '\\'; break;
        case '\'': ch = '\''; break;
        case '"':  ch = '"';  break;
        case 'a':  ch = '\a'; break;
        case 'b':  ch = '\b'; break;
        case 'f':  ch = '\f'; break;
        case 'v':  ch = '\v'; break;
        case 'e':  case 'E': ch = 0x1B; break;
        case '?':  ch = '?'; break;
        case 'x': {
            auto hex = inner.substr(2);
            ch = static_cast<uint8_t>(std::stoul(hex, nullptr, 16));
            break;
        }
        case 'u': {
            auto hex = inner.substr(2);
            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
            if (cp > 0xFF) {
                std::cerr << "lucis: unicode escape \\u" << hex << " does not fit in char (max 0xFF)\n";
                ch = 0;
            } else {
                ch = static_cast<uint8_t>(cp);
            }
            break;
        }
        case 'U': {
            auto hex = inner.substr(2);
            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
            if (cp > 0xFF) {
                std::cerr << "lucis: unicode escape \\U" << hex << " does not fit in char (max 0xFF)\n";
                ch = 0;
            } else {
                ch = static_cast<uint8_t>(cp);
            }
            break;
        }
        default:
            if (inner[1] >= '0' && inner[1] <= '7') {
                auto oct = inner.substr(1);
                auto val = std::stoul(oct, nullptr, 8);
                if (val > 255) {
                    std::cerr << "lucis: octal escape \\" << oct << " exceeds byte range\n";
                    ch = 0;
                } else {
                    ch = static_cast<uint8_t>(val);
                }
            } else {
                std::cerr << "lucis: unknown escape sequence '\\" << inner[1] << "'\n";
                ch = inner[1];
            }
            break;
        }
    } else {
        ch = static_cast<uint8_t>(inner[0]);
    }
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), ch));
}



std::any IRGen::visitStrLitExpr(LucisParser::StrLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->STR_LIT()->getText();
    auto escaped = raw.substr(1, raw.size() - 2);

    // Process escape sequences
    std::string str;
    str.reserve(escaped.size());
    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.size()) {
            char next = escaped[++i];
            switch (next) {
                case 'n':  str += '\n'; break;
                case 'r':  str += '\r'; break;
                case 't':  str += '\t'; break;
                case '\\': str += '\\'; break;
                case '"':  str += '"';  break;
                case '\'': str += '\''; break;
                case 'a':  str += '\a'; break;
                case 'b':  str += '\b'; break;
                case 'f':  str += '\f'; break;
                case 'v':  str += '\v'; break;
                case 'e':  case 'E': str += static_cast<char>(0x1B); break;
                case '?':  str += '?'; break;
                case 'x': {
                    if (i + 2 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 2);
                        if (allHex(hex))
                            str += static_cast<char>(std::stoi(hex, nullptr, 16));
                        else
                            std::cerr << "lucis: invalid hex escape \\x" << hex << "\n";
                        i += 2;
                    }
                    break;
                }
                case 'u': {
                    if (i + 4 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 4);
                        if (allHex(hex)) {
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            str += utf8Encode(cp);
                        } else {
                            std::cerr << "lucis: invalid unicode escape \\u" << hex << "\n";
                        }
                        i += 4;
                    }
                    break;
                }
                case 'U': {
                    if (i + 8 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 8);
                        if (allHex(hex)) {
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            str += utf8Encode(cp);
                        } else {
                            std::cerr << "lucis: invalid unicode escape \\U" << hex << "\n";
                        }
                        i += 8;
                    }
                    break;
                }
                default:
                    if (next >= '0' && next <= '7') {
                        size_t octLen = 1;
                        while (octLen < 3 && i + 1 < escaped.size() && escaped[i + 1] >= '0' && escaped[i + 1] <= '7') {
                            ++i;
                            ++octLen;
                        }
                        auto oct = escaped.substr(i - octLen + 1, octLen);
                        if (allOct(oct))
                            str += static_cast<char>(std::stoi(oct, nullptr, 8));
                        else
                            std::cerr << "lucis: invalid octal escape \\" << oct << "\n";
                    } else {
                        str += '\\';
                        str += next;
                    }
                    break;
            }
        } else {
            str += escaped[i];
        }
    }

    auto* strGlobal = builder_->CreateGlobalString(str, ".str", 0, module_);

    auto* strTy   = typeRegistry_.lookup("string")
                        ->toLLVMType(*context_, module_->getDataLayout());
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* len     = llvm::ConstantInt::get(usizeTy, str.size());

    llvm::Value* strStruct = llvm::UndefValue::get(strTy);
    strStruct = builder_->CreateInsertValue(strStruct, strGlobal, 0, "str_ptr");
    strStruct = builder_->CreateInsertValue(strStruct, len,       1, "str_len");
    return static_cast<llvm::Value*>(strStruct);
}

std::any IRGen::visitCStrLitExpr(LucisParser::CStrLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->C_STR_LIT()->getText();
    // Strip the c"..." wrapper: skip 'c"' at start and '"' at end
    auto escaped = raw.substr(2, raw.size() - 3);

    // Process escape sequences (same as string literals)
    std::string str;
    str.reserve(escaped.size());
    for (size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.size()) {
            char next = escaped[++i];
            switch (next) {
                case 'n':  str += '\n'; break;
                case 'r':  str += '\r'; break;
                case 't':  str += '\t'; break;
                case '\\': str += '\\'; break;
                case '"':  str += '"';  break;
                case '\'': str += '\''; break;
                case 'a':  str += '\a'; break;
                case 'b':  str += '\b'; break;
                case 'f':  str += '\f'; break;
                case 'v':  str += '\v'; break;
                case 'e':  case 'E': str += static_cast<char>(0x1B); break;
                case '?':  str += '?'; break;
                case 'x':
                    if (i + 2 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 2);
                        if (allHex(hex))
                            str += static_cast<char>(std::stoi(hex, nullptr, 16));
                        else
                            std::cerr << "lucis: invalid hex escape \\x" << hex << "\n";
                        i += 2;
                    }
                    break;
                case 'u': {
                    if (i + 4 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 4);
                        if (allHex(hex)) {
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            str += utf8Encode(cp);
                        } else {
                            std::cerr << "lucis: invalid unicode escape \\u" << hex << "\n";
                        }
                        i += 4;
                    }
                    break;
                }
                case 'U': {
                    if (i + 8 < escaped.size()) {
                        auto hex = escaped.substr(i + 1, 8);
                        if (allHex(hex)) {
                            auto cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                            str += utf8Encode(cp);
                        } else {
                            std::cerr << "lucis: invalid unicode escape \\U" << hex << "\n";
                        }
                        i += 8;
                    }
                    break;
                }
                default:
                    if (next >= '0' && next <= '7') {
                        size_t octLen = 1;
                        while (octLen < 3 && i + 1 < escaped.size() && escaped[i + 1] >= '0' && escaped[i + 1] <= '7') {
                            ++i;
                            ++octLen;
                        }
                        auto oct = escaped.substr(i - octLen + 1, octLen);
                        if (allOct(oct))
                            str += static_cast<char>(std::stoi(oct, nullptr, 8));
                        else
                            std::cerr << "lucis: invalid octal escape \\" << oct << "\n";
                    } else {
                        str += '\\';
                        str += next;
                    }
                    break;
            }
        } else {
            str += escaped[i];
        }
    }

    // Emit as a null-terminated global constant, return pointer (ptr)
    auto* cstrGlobal = builder_->CreateGlobalString(str, ".cstr", 0, module_);
    return static_cast<llvm::Value*>(cstrGlobal);
}

// ── Raw backtick: `...` or r`...` ──────────────────────────────────────────


static std::string decodeRawBacktick(const std::string& raw) {
    auto first = raw.find('`');
    auto last  = raw.rfind('`');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return {};
    auto content = raw.substr(first + 1, last - first - 1);
    std::string out;
    out.reserve(content.size());
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '`' && i + 1 < content.size() && content[i + 1] == '`') {
            out += '`';
            i += 1;
        } else {
            out += content[i];
        }
    }
    return out;
}



static llvm::Value* emitBtickString(llvm::IRBuilder<>& builder, llvm::Module* mod,
                                      llvm::LLVMContext& ctx,
                                      const TypeRegistry& typeRegistry,
                                      const std::string& str,
                                      const std::string& globalName) {
    auto* strGlobal = builder.CreateGlobalString(str, globalName, 0, mod);
    auto* strTy = typeRegistry.lookup("string")
        ->toLLVMType(ctx, mod->getDataLayout());
    auto* usizeTy = mod->getDataLayout().getIntPtrType(ctx);
    auto* len = llvm::ConstantInt::get(usizeTy, str.size());
    llvm::Value* strStruct = llvm::UndefValue::get(strTy);
    strStruct = builder.CreateInsertValue(strStruct, strGlobal, 0, "str_ptr");
    strStruct = builder.CreateInsertValue(strStruct, len,       1, "str_len");
    return strStruct;
}



std::any IRGen::visitBtickExpr(LucisParser::BtickExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->BTICK()->getText();
    auto str = decodeRawBacktick(raw);
    return static_cast<llvm::Value*>(
        emitBtickString(*builder_, module_, *context_, typeRegistry_, str, ".btick"));
}



std::any IRGen::visitRawBtickExpr(LucisParser::RawBtickExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->RAW_BTICK()->getText();
    auto str = decodeRawBacktick(raw);
    return static_cast<llvm::Value*>(
        emitBtickString(*builder_, module_, *context_, typeRegistry_, str, ".rbtick"));
}

// ── Interpolated backtick: i`text{expr}more` ────────────────────────────────
// Lowers to sprintf("text {} more", expr...)
// Each {expr} is parsed as a sub-expression and evaluated at runtime.


static bool isSimpleIdent(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isalnum(c) && c != '_') return false;
    return true;
}



std::any IRGen::visitIntBtickExpr(LucisParser::IntBtickExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->INT_BTICK()->getText();
    auto first = raw.find('`');
    auto last  = raw.rfind('`');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return static_cast<llvm::Value*>(llvm::UndefValue::get(
            typeRegistry_.lookup("string")->toLLVMType(*context_, module_->getDataLayout())));
    auto content = raw.substr(first + 1, last - first - 1);

    // Parse segments: literal text and {expr} interpolations
    struct Segment { bool isExpr; std::string text; };
    std::vector<Segment> segs;
    size_t braceDepth = 0;
    std::string cur;
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '{' && braceDepth == 0) {
            segs.push_back({false, cur});
            cur.clear();
            braceDepth = 1;
        } else if (content[i] == '}' && braceDepth == 1) {
            segs.push_back({true, cur});
            cur.clear();
            braceDepth = 0;
        } else if (content[i] == '`' && i + 1 < content.size() && content[i + 1] == '`') {
            cur += '`';
            i += 1;
        } else {
            cur += content[i];
        }
    }
    segs.push_back({false, cur});

    // Build format string: literal text + "{}" per expression
    std::string fmt;
    size_t numExprs = 0;
    for (auto& seg : segs) {
        if (seg.isExpr) {
            fmt += "{}";
            numExprs++;
        } else {
            fmt += seg.text;
        }
    }

    // Evaluate sub-expressions
    auto* strTy   = typeRegistry_.lookup("string")
                        ->toLLVMType(*context_, module_->getDataLayout());
    auto* sliceTy = strTy; // {ptr, i64}
    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* i64Ty   = llvm::Type::getInt64Ty(*context_);

    std::vector<llvm::Value*> exprVals;
    exprVals.reserve(numExprs);

    // Keep ParseResults alive for the duration of visiting
    std::vector<ParseResult> held;

    for (auto& seg : segs) {
        if (!seg.isExpr) continue;

        llvm::Value* val = nullptr;

        // Fast path: simple variable lookup
        if (isSimpleIdent(seg.text)) {
            auto it = locals_.find(seg.text);
            if (it != locals_.end()) {
                auto* ty = it->second.typeInfo->toLLVMType(*context_, module_->getDataLayout());
                val = builder_->CreateLoad(ty, it->second.alloca, seg.text);
                exprVals.push_back(val);
                continue;
            }
        }

        // Full path: wrap expression in a dummy function and parse properly
        std::string wrap = "fn __ibtick_" + std::to_string(held.size()) +
                           "() int32 { return (" + seg.text + "); }";
        auto pr = Parser::parseString(wrap);
        if (pr.tree && !pr.hasErrors) {
            // Navigate: program → topLevelDecl → functionDecl → functionDefinition → block → stmt → returnStmt → expression
            auto tds = pr.tree->topLevelDecl();
            if (!tds.empty() && tds[0]->functionDecl()) {
                auto* fd = tds[0]->functionDecl();
                if (auto* block = fd->block()) {
                    auto stmts = block->statement();
                    if (!stmts.empty() && stmts[0]->returnStmt()) {
                        held.push_back(std::move(pr));
                        val = castValue(visit(stmts[0]->returnStmt()->expression()));
                    }
                }
            }
        }

        if (val) {
            exprVals.push_back(val);
        } else {
            exprVals.push_back(llvm::Constant::getNullValue(sliceTy));
        }
    }

    // Build lucis_sprintf call
    // Arguments: lucis_sprintf(fmtPtr, fmtLen, argsPtr, argsCount)
    auto* fmtGlobal = builder_->CreateGlobalString(fmt, ".ibtick", 0, module_);
    auto* fmtLen    = llvm::ConstantInt::get(usizeTy, fmt.size());
    auto* zero      = llvm::ConstantInt::get(i64Ty, 0);

    llvm::Value* argsPtr   = llvm::ConstantPointerNull::get(ptrTy);
    llvm::Value* argsCount = llvm::ConstantInt::get(usizeTy, 0);

    if (!exprVals.empty()) {
        size_t n = exprVals.size();
        auto* arrTy = llvm::ArrayType::get(sliceTy, n);
        auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "ibtick.args");

        for (size_t i = 0; i < n; i++) {
            auto* arg = exprVals[i];
            llvm::Value* converted = nullptr;

            if (arg->getType() == sliceTy) {
                converted = arg;
            } else if (arg->getType()->isIntegerTy(1)) {
                auto* trueStr  = builder_->CreateGlobalString("true", ".str.true", 0, module_);
                auto* falseStr = builder_->CreateGlobalString("false", ".str.false", 0, module_);
                auto* trueLen  = llvm::ConstantInt::get(usizeTy, 4);
                auto* falseLen = llvm::ConstantInt::get(usizeTy, 5);
                auto* ptr = builder_->CreateSelect(arg, trueStr, falseStr);
                auto* len = builder_->CreateSelect(arg, trueLen, falseLen);
                converted = llvm::UndefValue::get(sliceTy);
                converted = builder_->CreateInsertValue(converted, ptr, 0);
                converted = builder_->CreateInsertValue(converted, len, 1);
            } else if (arg->getType()->isIntegerTy()) {
                auto* ext = builder_->CreateIntCast(arg, i64Ty, true, "icast");
                auto fn = declareBuiltin("lucis_itoa", sliceTy, {i64Ty});
                converted = builder_->CreateCall(fn, {ext}, "itoa");
            } else if (arg->getType()->isFloatingPointTy()) {
                auto* dblTy = llvm::Type::getDoubleTy(*context_);
                auto* ext = arg->getType()->isFloatTy()
                    ? builder_->CreateFPExt(arg, dblTy, "fext")
                    : arg;
                auto fn = declareBuiltin("lucis_ftoa", sliceTy, {dblTy});
                converted = builder_->CreateCall(fn, {ext}, "ftoa");
            } else {
                converted = arg;
            }

            auto* idx = llvm::ConstantInt::get(i64Ty, i);
            auto* gep = builder_->CreateGEP(arrTy, arrAlloca, {zero, idx});
            builder_->CreateStore(converted, gep);
        }

        argsPtr   = builder_->CreateGEP(arrTy, arrAlloca, {zero, zero}, "ibtick.args.ptr");
        argsCount = llvm::ConstantInt::get(usizeTy, n);
    }

    auto callee = declareBuiltin("lucis_sprintf", strTy,
        {ptrTy, usizeTy, ptrTy, usizeTy});
    auto* ret = builder_->CreateCall(callee,
        {fmtGlobal, fmtLen, argsPtr, argsCount}, "ibtick.sprintf");

    auto* retPtr = builder_->CreateExtractValue(ret, 0, "ibtick.ptr");
    auto* retLen = builder_->CreateExtractValue(ret, 1, "ibtick.len");
    llvm::Value* strStruct = llvm::UndefValue::get(strTy);
    strStruct = builder_->CreateInsertValue(strStruct, retPtr, 0);
    strStruct = builder_->CreateInsertValue(strStruct, retLen, 1);
    return static_cast<llvm::Value*>(strStruct);
}

// ── Shell backtick: s`command` ──────────────────────────────────────────────


std::any IRGen::visitShellBtickExpr(LucisParser::ShellBtickExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->SHELL_BTICK()->getText();
    auto cmd = decodeRawBacktick(raw);

    // Call lucis_system(cmd) at runtime
    auto* strGlobal = builder_->CreateGlobalString(cmd, ".scmd", 0, module_);
    auto* strTy   = typeRegistry_.lookup("string")
                        ->toLLVMType(*context_, module_->getDataLayout());
    auto* voidTy  = llvm::Type::getVoidTy(*context_);
    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);

    // Build {ptr, len} argument for the command string
    auto* cmdLen = llvm::ConstantInt::get(usizeTy, cmd.size());
    llvm::Value* cmdStruct = llvm::UndefValue::get(strTy);
    cmdStruct = builder_->CreateInsertValue(cmdStruct, strGlobal, 0, "cmd_ptr");
    cmdStruct = builder_->CreateInsertValue(cmdStruct, cmdLen,    1, "cmd_len");

    // Call lucis_system(cmdStruct) → returns lucis_str_result
    auto callee = declareBuiltin("lucis_system", strTy, {strTy});
    return static_cast<llvm::Value*>(
        builder_->CreateCall(callee, {cmdStruct}, "shell_result"));
}

// ── Comptime backtick: c`command` ───────────────────────────────────────────


std::any IRGen::visitCmptBtickExpr(LucisParser::CmptBtickExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto raw = ctx->CMPT_BTICK()->getText();
    auto cmd = decodeRawBacktick(raw);

    // Execute command at compile time
    auto* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "lucis: comptime shell command failed: " << cmd << "\n";
        return static_cast<llvm::Value*>(
            emitBtickString(*builder_, module_, *context_, typeRegistry_, "", ".cbtick"));
    }
    std::string result;
    char buf[4096];
    while (auto n = fread(buf, 1, sizeof(buf), pipe))
        result.append(buf, n);
    pclose(pipe);

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return static_cast<llvm::Value*>(
        emitBtickString(*builder_, module_, *context_, typeRegistry_, result, ".cbtick"));
}



std::any IRGen::visitLambdaExpr(LucisParser::LambdaExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Check cache first (avoid double-generation during auto type inference)
    {
        auto it = lambdaCache_.find(ctx);
        if (it != lambdaCache_.end())
            return static_cast<llvm::Value*>(it->second);
    }

    // 1. Determine parameter types and names from AST
    std::vector<llvm::Type*> paramLLTypes;
    std::vector<std::string> paramNames;
    std::vector<const TypeInfo*> paramTIs;

    if (auto* params = ctx->paramList()) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue;
            paramNames.push_back(p->IDENTIFIER()->getText());
            auto* pti = resolveTypeInfo(p->typeSpec());
            paramTIs.push_back(pti);
            auto* llty = pti ? pti->toLLVMType(*context_, module_->getDataLayout())
                             : llvm::Type::getInt32Ty(*context_);
            paramLLTypes.push_back(llty);
        }
    }

    // 2. Determine return type from body expression
    auto* bodyExpr = ctx->expression();
    auto* bodyTI = resolveExprTypeInfo(bodyExpr);
    auto* retLLTy = bodyTI ? bodyTI->toLLVMType(*context_, module_->getDataLayout())
                           : llvm::Type::getInt32Ty(*context_);

    // 3. Create the synthetic function
    auto funcName = "__lambda_" + std::to_string(lambdaCounter_++);
    auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
    auto* fn = llvm::Function::Create(fnType, llvm::Function::InternalLinkage, funcName, module_);

    // 4. Set up function body
    auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", fn);
    auto* prevFn = currentFunction_;
    auto* prevBuilder = builder_;
    auto* prevDbgScope = currentDbgScope_;
    currentFunction_ = fn;
    auto entryBuilder = std::make_unique<llvm::IRBuilder<>>(entryBB);
    builder_ = entryBuilder.get();

    // Save outer locals, create new scope
    auto savedDbgScope = currentDbgScope_;
    currentDbgScope_ = nullptr;
    auto savedLocals = std::move(locals_);
    locals_.clear();

    // Store params as allocas
    for (size_t i = 0; i < paramLLTypes.size(); i++) {
        auto* alloca = builder_->CreateAlloca(paramLLTypes[i], nullptr, paramNames[i]);
        auto* paramVal = fn->getArg(static_cast<unsigned>(i));
        paramVal->setName(paramNames[i]);
        builder_->CreateStore(paramVal, alloca);
        VarInfo vi;
        vi.alloca = alloca;
        vi.typeInfo = (i < paramTIs.size()) ? paramTIs[i] : nullptr;
        vi.isParam = true;
        locals_[paramNames[i]] = vi;
    }

    // Visit the body expression
    auto* bodyVal = castValue(visit(bodyExpr));
    if (bodyVal) {
        if (retLLTy->isVoidTy()) {
            builder_->CreateRetVoid();
        } else {
            if (bodyVal->getType() != retLLTy) {
                if (bodyVal->getType()->isIntegerTy() && retLLTy->isIntegerTy())
                    bodyVal = builder_->CreateIntCast(bodyVal, retLLTy, true);
                else if (bodyVal->getType()->isFloatingPointTy() && retLLTy->isFloatingPointTy())
                    bodyVal = builder_->CreateFPCast(bodyVal, retLLTy);
            }
            builder_->CreateRet(bodyVal);
        }
    } else {
        builder_->CreateRet(llvm::UndefValue::get(retLLTy));
    }

    // Restore
    locals_ = std::move(savedLocals);
    builder_ = prevBuilder;
    currentFunction_ = prevFn;
    currentDbgScope_ = prevDbgScope;
    currentDbgScope_ = savedDbgScope;

    // 5. Cache and return function pointer
    lambdaCache_[ctx] = fn;
    return static_cast<llvm::Value*>(fn);
}



std::any IRGen::visitLambdaBlockExpr(LucisParser::LambdaBlockExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Check cache first
    {
        auto it = lambdaCache_.find(ctx);
        if (it != lambdaCache_.end())
            return static_cast<llvm::Value*>(it->second);
    }

    // For block lambdas, we generate the function and visit the block.
    std::vector<llvm::Type*> paramLLTypes;
    std::vector<std::string> paramNames;
    std::vector<const TypeInfo*> paramTIs;

    if (auto* params = ctx->paramList()) {
        for (auto* p : params->param()) {
            if (p->SPREAD()) continue;
            paramNames.push_back(p->IDENTIFIER()->getText());
            auto* pti = resolveTypeInfo(p->typeSpec());
            paramTIs.push_back(pti);
            auto* llty = pti ? pti->toLLVMType(*context_, module_->getDataLayout())
                             : llvm::Type::getInt32Ty(*context_);
            paramLLTypes.push_back(llty);
        }
    }

    // Determine return type from block (look for ret stmt)
    auto* block = ctx->block();

    // Temporarily register params so return expression can resolve them
    for (size_t i = 0; i < paramNames.size(); i++) {
        auto* pTy = paramLLTypes[i];
        auto* alloca = builder_->CreateAlloca(pTy, nullptr, paramNames[i] + ".tmp");
        locals_[paramNames[i]] = { alloca, paramTIs[i], 0 };
    }

    const TypeInfo* retTI = nullptr;
    for (auto* stmt : block->statement()) {
        if (auto* retStmt = stmt->returnStmt()) {
            if (auto* retExpr = retStmt->expression()) {
                if (auto* rti = resolveExprTypeInfo(retExpr)) {
                    retTI = rti;
                }
            }
            if (!retTI) retTI = typeRegistry_.lookup("int32");
            break;
        }
    }
    if (!retTI) retTI = typeRegistry_.lookup("void");

    // Clean up temporary param bindings before setting up function context
    for (auto& pn : paramNames)
        locals_.erase(pn);
    auto* retLLTy = retTI->toLLVMType(*context_, module_->getDataLayout());

    auto funcName = "__lambda_" + std::to_string(lambdaCounter_++);
    auto* fnType = llvm::FunctionType::get(retLLTy, paramLLTypes, false);
    auto* fn = llvm::Function::Create(fnType, llvm::Function::InternalLinkage, funcName, module_);

    auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", fn);
    auto* prevFn = currentFunction_;
    auto* prevBuilder = builder_;
    currentFunction_ = fn;
    auto entryBuilder = std::make_unique<llvm::IRBuilder<>>(entryBB);
    builder_ = entryBuilder.get();

    auto savedLocals = std::move(locals_);
    locals_.clear();

    for (size_t i = 0; i < paramLLTypes.size(); i++) {
        auto* alloca = builder_->CreateAlloca(paramLLTypes[i], nullptr, paramNames[i]);
        auto* paramVal = fn->getArg(static_cast<unsigned>(i));
        paramVal->setName(paramNames[i]);
        builder_->CreateStore(paramVal, alloca);
        VarInfo vi;
        vi.alloca = alloca;
        vi.typeInfo = (i < paramTIs.size()) ? paramTIs[i] : nullptr;
        vi.isParam = true;
        locals_[paramNames[i]] = vi;
    }

    // Visit statements directly (avoid visitBlock which creates a new BB)
    for (auto* stmt : block->statement()) {
        visit(stmt);
    }

    // Ensure terminator
    llvm::BasicBlock* curBB = builder_->GetInsertBlock();
    if (!curBB->getTerminator()) {
        if (retLLTy->isVoidTy())
            builder_->CreateRetVoid();
        else
            builder_->CreateRet(llvm::UndefValue::get(retLLTy));
    }

    locals_ = std::move(savedLocals);
    builder_ = prevBuilder;
    currentFunction_ = prevFn;

    lambdaCache_[ctx] = fn;
    return static_cast<llvm::Value*>(fn);
}



std::any IRGen::visitCfgExpr(LucisParser::CfgExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Evaluate @cfg(…) at compile time using TargetInfo
    auto* inner = ctx->expression();
    bool value = evalCfgPredicate(inner);
    return static_cast<llvm::Value*>(builder_->getInt1(value));
}



std::any IRGen::visitAtPtrExpr(LucisParser::AtPtrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // @ptr(T, addr) — convert integer address to typed pointer
    auto* pointeeTI = resolveTypeInfo(ctx->typeSpec());
    if (!pointeeTI) {
        std::cerr << "lucis: @ptr: unknown pointee type\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::PointerType::getUnqual(*context_)));
    }
    auto* pointeeTy = pointeeTI->toLLVMType(*context_, module_->getDataLayout());
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    auto* addrVal = castValue(visit(ctx->expression()));
    if (!addrVal) {
        std::cerr << "lucis: @ptr: address expression must be an integer\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(ptrTy));
    }
    // Cast the integer address to a pointer
    return static_cast<llvm::Value*>(
        builder_->CreateIntToPtr(addrVal, ptrTy, "ptr"));
}



std::any IRGen::visitAtTypeExpr(LucisParser::AtTypeExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto builtinName = ctx->IDENTIFIER()->getText();

    auto* typeTI = resolveTypeInfo(ctx->typeSpec());
    if (!typeTI) {
        std::cerr << "lucis: @" << builtinName << ": unknown type\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    // Look up the builtin in the comptime engine
    if (comptimeEngine_) {
        auto* builtin = comptimeEngine_->builtinRegistry().lookup(builtinName);
        if (builtin) {
            std::vector<ComptimeValue> args;
            args.push_back(ComptimeValue::typeVal(const_cast<TypeInfo*>(typeTI)));
            auto result = builtin->fn(args);

            if (result.kind() == ComptimeValue::Kind::Bool) {
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_),
                        result.asBool() ? 1 : 0));
            }
            if (result.kind() == ComptimeValue::Kind::String) {
                // Build a proper {i8*, i64} string struct (like visitStrLitExpr does)
                std::string s = result.asString();
                auto* strGlobal = builder_->CreateGlobalString(s,
                    ".builtin_str", 0, module_);
                auto* strTy = typeRegistry_.lookup("string")
                    ->toLLVMType(*context_, module_->getDataLayout());
                auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
                auto* len = llvm::ConstantInt::get(usizeTy, s.size());
                llvm::Value* strStruct = llvm::UndefValue::get(strTy);
                strStruct = builder_->CreateInsertValue(strStruct, strGlobal, 0, "builtin_ptr");
                strStruct = builder_->CreateInsertValue(strStruct, len,       1, "builtin_len");
                return static_cast<llvm::Value*>(strStruct);
            }
            if (result.kind() == ComptimeValue::Kind::Int) {
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_),
                        result.asInt(), true));
            }
            if (result.kind() == ComptimeValue::Kind::Float) {
                return static_cast<llvm::Value*>(
                    llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_),
                        result.asFloat()));
            }
            if (result.kind() == ComptimeValue::Kind::Void) {
                return static_cast<llvm::Value*>(
                    llvm::Constant::getNullValue(
                        llvm::Type::getInt32Ty(*context_)));
            }
            // Kind::List, Kind::Struct, Kind::Function, Kind::Type are not
            // supported as comptime builtin return values yet.
            std::cerr << "lucis: @" << builtinName
                      << ": unsupported comptime builtin return kind\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
    }

    std::cerr << "lucis: unknown comptime builtin '@" << builtinName << "'\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}

// ── Helper: evaluate @cfg predicate expression ─────────────────────


bool IRGen::evalCfgPredicate(LucisParser::ExpressionContext* expr) {
    // Bool literals
    if (auto* b = dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return b->BOOL_LIT()->getText() == "true";

    // Parenthesized
    if (auto* paren = dynamic_cast<LucisParser::ParenExprContext*>(expr))
        return evalCfgPredicate(paren->expression());

    // Logical NOT
    if (auto* lnot = dynamic_cast<LucisParser::LogicalNotExprContext*>(expr))
        return !evalCfgPredicate(lnot->expression());

    // Logical AND (short-circuit)
    if (auto* land = dynamic_cast<LucisParser::LogicalAndExprContext*>(expr)) {
        auto sub = land->expression();
        return evalCfgPredicate(sub[0]) && evalCfgPredicate(sub[1]);
    }

    // Logical OR (short-circuit)
    if (auto* lor = dynamic_cast<LucisParser::LogicalOrExprContext*>(expr)) {
        auto sub = lor->expression();
        return evalCfgPredicate(sub[0]) || evalCfgPredicate(sub[1]);
    }

    // Equality / inequality
    if (auto* eq = dynamic_cast<LucisParser::EqExprContext*>(expr)) {
        auto sub = eq->expression();
        auto lhs = evalCfgIdentOrString(sub[0]);
        auto rhs = evalCfgIdentOrString(sub[1]);
        if (!lhs || !rhs) return true; // fallback
        auto raw = eq->getText();
        bool isEq = raw.find("==") != std::string::npos;
        return isEq ? (*lhs == *rhs) : (*lhs != *rhs);
    }

    // Relational (<, >, <=, >=)
    if (auto* rel = dynamic_cast<LucisParser::RelExprContext*>(expr)) {
        auto sub = rel->expression();
        auto lhs = evalCfgIntValue(sub[0]);
        auto rhs = evalCfgIntValue(sub[1]);
        if (!lhs || !rhs) return true;
        auto raw = rel->getText();
        if (raw.find("<=") != std::string::npos) return *lhs <= *rhs;
        if (raw.find(">=") != std::string::npos) return *lhs >= *rhs;
        if (raw.find('<') != std::string::npos) return *lhs < *rhs;
        if (raw.find('>') != std::string::npos) return *lhs > *rhs;
        return true;
    }

    // Function calls: all(), any(), not() — built-in cfg combinators
    if (auto* fn = dynamic_cast<LucisParser::FnCallExprContext*>(expr)) {
        if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(fn->expression())) {
            auto name = ident->IDENTIFIER()->getText();
            auto* args = fn->argList();
            auto argExprs = args ? args->expression() : std::vector<LucisParser::ExpressionContext*>();

            if (name == "all") {
                for (auto* a : argExprs) {
                    if (!evalCfgPredicate(a)) return false;
                }
                return true;
            }
            if (name == "any") {
                for (auto* a : argExprs) {
                    if (evalCfgPredicate(a)) return true;
                }
                return false;
            }
            if (name == "not") {
                if (argExprs.size() == 1)
                    return !evalCfgPredicate(argExprs[0]);
                return true;
            }
        }
    }

    // Bare identifier: treat as boolean (debug_assertions → true)
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        // Check target info
        auto val = getCfgTargetValue(name);
        if (val) return *val;
        // Unknown identifier → fallback true
        return true;
    }

    return true; // fallback
}

// Helper: resolve an identifier or string literal to an optional string


std::optional<std::string> IRGen::evalCfgIdentOrString(LucisParser::ExpressionContext* expr) {
    if (auto* s = dynamic_cast<LucisParser::StrLitExprContext*>(expr)) {
        auto raw = s->STR_LIT()->getText();
        if (raw.size() >= 2)
            return raw.substr(1, raw.size() - 2);
        return "";
    }
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        return getCfgTargetString(name);
    }
    if (auto* b = dynamic_cast<LucisParser::BoolLitExprContext*>(expr))
        return b->BOOL_LIT()->getText() == "true" ? std::string("true") : std::string("false");
    return std::nullopt;
}

// Helper: resolve identifier or literal to an optional integer


std::optional<int64_t> IRGen::evalCfgIntValue(LucisParser::ExpressionContext* expr) {
    if (auto* n = dynamic_cast<LucisParser::IntLitExprContext*>(expr)) {
        std::string t = n->INT_LIT()->getText();
        return static_cast<int64_t>(std::stoll(t));
    }
    if (auto* id = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = id->IDENTIFIER()->getText();
        auto val = getCfgTargetValue(name);
        if (val) return static_cast<int64_t>(*val ? 1 : 0);
        return getCfgTargetInt(name);
    }
    return std::nullopt;
}

// ── Target info helpers ──────────────────────────────────────────


std::optional<bool> IRGen::getCfgTargetValue(const std::string& name) {
    if (name == "debug_assertions")
        return debugOutput_;
    return std::nullopt;
}



std::optional<std::string> IRGen::getCfgTargetString(const std::string& name) {
    // Use target triple from the current module
    auto tripleStr = module_->getTargetTriple().str();
    if (tripleStr.empty())
        tripleStr = llvm::sys::getDefaultTargetTriple();
    TargetInfo target(tripleStr, debugOutput_);
    if (name == "target_os") return target.os;
    if (name == "target_arch") return target.arch;
    if (name == "target_endian") return target.endian;
    if (name == "target_os_family") return target.osFamily;
    if (name == "target_triple") return target.triple;
    // Shorthand expansions
    if (name == "unix") return target.osFamily;
    if (name == "windows") return std::string("windows");
    if (name == "x86_64") return std::string("x86_64");
    if (name == "aarch64") return std::string("aarch64");
    if (name == "debug") return debugOutput_ ? std::string("true") : std::string("false");
    return std::nullopt;
}



std::optional<int64_t> IRGen::getCfgTargetInt(const std::string& name) {
    if (name == "target_pointer_width") {
        auto tripleStr = module_->getTargetTriple().str();
        if (tripleStr.empty())
            tripleStr = llvm::sys::getDefaultTargetTriple();
        TargetInfo target(tripleStr, debugOutput_);
        return static_cast<int64_t>(target.pointerWidth);
    }
    return std::nullopt;
}



std::any IRGen::visitIdentExpr(LucisParser::IdentExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto name = ctx->IDENTIFIER()->getText();
    auto it   = locals_.find(name);
    if (it != locals_.end()) {
        auto* alloca = it->second.alloca;
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(alloca->getAllocatedType(), alloca, name));
    }

    // ── Top-level const: load from global ─────────────────────────────────
    auto cit = topLevelConsts_.find(name);
    if (cit != topLevelConsts_.end()) {
        auto* global = cit->second.global;
        auto* llvmType = global->getValueType();
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(llvmType, global, name));
    }

    // ── Variadic parameter: return data pointer ─────────────────────────
    auto vit = variadicParams_.find(name);
    if (vit != variadicParams_.end()) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(ptrTy, vit->second.dataPtr, name));
    }

    // ── std::math constants ─────────────────────────────────────────────
    if (imports_.isImported(name)) {
        auto* f64Ty = llvm::Type::getDoubleTy(*context_);
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);

        if (name == "PI")
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::get(f64Ty, 3.14159265358979323846));
        if (name == "E")
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::get(f64Ty, 2.71828182845904523536));
        if (name == "TAU")
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::get(f64Ty, 6.28318530717958647692));
        if (name == "INF")
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::getInfinity(f64Ty));
        if (name == "NAN")
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::getNaN(f64Ty));
        if (name == "INT32_MAX")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(i32Ty, 2147483647));
        if (name == "INT32_MIN")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(i32Ty, (uint64_t)(int32_t)-2147483648, true));
        if (name == "INT64_MAX")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(i64Ty, 9223372036854775807ULL));
        if (name == "INT64_MIN")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(i64Ty, (uint64_t)(-9223372036854775807LL - 1), true));
        if (name == "UINT32_MAX")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 4294967295U));
        if (name == "UINT64_MAX")
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(i64Ty, 18446744073709551615ULL));
        if (name == "ISIZE_MIN" || name == "ISIZE_MAX") {
            auto ptrWidth = module_->getDataLayout().getPointerSizeInBits();
            auto* isizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto val = (ptrWidth == 64)
                ? (name == "ISIZE_MIN" ? 0x8000000000000000ULL : 0x7FFFFFFFFFFFFFFFULL)
                : (name == "ISIZE_MIN" ? 0x80000000ULL : 0x7FFFFFFFULL);
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(isizeTy, val, true));
        }
        if (name == "USIZE_MAX") {
            auto ptrWidth = module_->getDataLayout().getPointerSizeInBits();
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto val = (ptrWidth == 64) ? 0xFFFFFFFFFFFFFFFFULL : 0xFFFFFFFFULL;
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(usizeTy, val, false));
        }
    }

    // Check if it's a module-level function (function reference)
    if (auto* fn = module_->getFunction(name)) {
        return static_cast<llvm::Value*>(fn);
    }
    // Try mangled name via callTargetMap_
    {
        auto cit = callTargetMap_.find(name);
        if (cit != callTargetMap_.end()) {
            if (auto* fn = module_->getFunction(cit->second))
                return static_cast<llvm::Value*>(fn);
        }
    }

    // ── C enum constants ────────────────────────────────────────────────
    {
        auto eit = cEnumConstants_.find(name);
        if (eit != cEnumConstants_.end()) {
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_),
                                       static_cast<uint64_t>(eit->second), true));
        }
    }

    // ── C float constants ───────────────────────────────────────────────
    {
        auto fit = cFloatConstants_.find(name);
        if (fit != cFloatConstants_.end()) {
            return static_cast<llvm::Value*>(
                llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_),
                                      fit->second));
        }
    }

    // ── C string constants ──────────────────────────────────────────────
    {
        auto sit = cStringConstants_.find(name);
        if (sit != cStringConstants_.end()) {
            return static_cast<llvm::Value*>(
                builder_->CreateGlobalStringPtr(sit->second, name));
        }
    }

    // ── C struct literal macros (e.g. RAYWHITE → Color{245,245,245,255}) ─
    {
        auto sit = cStructMacros_.find(name);
        if (sit != cStructMacros_.end()) {
            const auto* sm = sit->second;
            auto* structTy = llvm::StructType::getTypeByName(*context_,
                                                              sm->structType);
            if (structTy) {
                std::vector<llvm::Constant*> fields;
                unsigned numElements = structTy->getNumElements();
                for (unsigned i = 0; i < numElements && i < sm->fieldValues.size(); ++i) {
                    auto* fieldTy = structTy->getElementType(i);
                    fields.push_back(llvm::ConstantInt::get(
                        fieldTy, static_cast<uint64_t>(sm->fieldValues[i]),
                        /* isSigned */ true));
                }
                // Pad remaining fields with zero if macro has fewer values
                for (unsigned i = static_cast<unsigned>(sm->fieldValues.size());
                     i < numElements; ++i) {
                    fields.push_back(llvm::Constant::getNullValue(
                        structTy->getElementType(i)));
                }
                return static_cast<llvm::Value*>(
                    llvm::ConstantStruct::get(structTy, fields));
            }
        }
    }

    // ── C global variables ──────────────────────────────────────────────
    {
        auto git = cGlobals_.find(name);
        if (git != cGlobals_.end()) {
            auto& entry = git->second;
            auto* loadedTy = entry.global->getValueType();
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(loadedTy, entry.global, name));
        }
    }

    // ── User-imported constant from another module ─────────────────────
    if (userImports_.count(name)) {
        std::string globalName = "const_" + name;
        auto* gv = module_->getGlobalVariable(globalName);
        if (!gv) {
            // Determine LLVM type from the defining module's declaration
            llvm::Type* llvmType = llvm::Type::getInt32Ty(*context_);
            auto modPath = userImports_[name];
            if (moduleRegistry_) {
                auto* sym = moduleRegistry_->findSymbol(modPath, name);
                if (sym && sym->kind == ExportedSymbol::Constant) {
                    if (auto* cd = dynamic_cast<LucisParser::ConstDeclaratorContext*>(sym->decl)) {
                        const TypeInfo* typeInfo = nullptr;
                        if (cd->typeSpec()) {
                            typeInfo = resolveTypeInfo(cd->typeSpec());
                        } else if (cd->expression()) {
                            typeInfo = resolveExprTypeInfo(cd->expression());
                        }
                        if (typeInfo)
                            llvmType = typeInfo->toLLVMType(*context_, module_->getDataLayout());
                    }
                }
            }
            gv = module_->getOrInsertGlobal(globalName, llvmType);
        }
        if (gv) {
            auto* loadedTy = gv->getValueType();
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(loadedTy, gv, name));
        }
    }

    std::cerr << "lucis: undefined variable '" << name << "'\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitArrayLitExpr(LucisParser::ArrayLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto elems = ctx->expression();
    if (elems.empty()) {
        auto* vecTy = getOrCreateVecStructType();
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        
        llvm::Constant* nullPtr = llvm::ConstantPointerNull::get(ptrTy);
        llvm::Constant* zero = llvm::ConstantInt::get(i64Ty, 0);
        
        return static_cast<llvm::Value*>(
            llvm::ConstantStruct::get(vecTy, {nullPtr, zero, zero}));
    }

    std::vector<llvm::Value*> vals;
    for (auto* e : elems) {
        vals.push_back(castValue(visit(e)));
        // Ownership move into literal aggregate: invalidate moved locals so
        // autodrop won't free both source local and destination container.
        consumeExprIfOwnedLocal(e);
    }

    auto* elemTy = vals[0]->getType();
    auto* arrTy  = llvm::ArrayType::get(elemTy, vals.size());
    llvm::Value* agg = llvm::UndefValue::get(arrTy);
    for (size_t i = 0; i < vals.size(); i++)
        agg = builder_->CreateInsertValue(agg, vals[i], {static_cast<unsigned>(i)});

    return static_cast<llvm::Value*>(agg);
}



std::any IRGen::visitBangInitExpr(LucisParser::BangInitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    std::string baseName;
    if (ctx->VEC())       baseName = "Vec";
    else if (ctx->MAP())  baseName = "Map";
    else if (ctx->SET())  baseName = "Set";
    else if (ctx->IDENTIFIER()) baseName = ctx->IDENTIFIER()->getText();
    else {
        std::cerr << "lucis: bang-initializer requires a type name before '!'\n";
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* voidTy  = llvm::Type::getVoidTy(*context_);
    auto entries  = ctx->bangEntry();

    // ── Vec<T> / Set<T> ──────────────────────────────────────────────
    if (baseName == "Vec" || baseName == "Set") {
        auto* collTy = (baseName == "Vec")
            ? getOrCreateVecStructType() : getOrCreateSetStructType();

        // Empty initializer: return zero-initialized collection (null ptr, 0 len/cap)
        if (entries.empty()) {
            return static_cast<llvm::Value*>(llvm::ConstantAggregateZero::get(collTy));
        }

        auto* alloca = builder_->CreateAlloca(collTy, nullptr, baseName + "_bang");
        auto* elemTI = resolveExprTypeInfo(entries[0]->expression(0));
        if (!elemTI) {
            std::cerr << "lucis: cannot determine element type\n";
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        auto suffix  = getVecSuffix(elemTI);
        auto* elemLLTy = elemTI->toLLVMType(*context_, module_->getDataLayout());
        auto& dl = module_->getDataLayout();
        auto elemSz = dl.getTypeAllocSize(elemLLTy);
        bool isRaw = (suffix == "raw");

        // Init (Vec supports init_cap for efficiency; Set only has plain init)
        if (isRaw) {
            auto initFn = declareBuiltin("lucis_vec_init_cap_raw", voidTy, {ptrTy, usizeTy, usizeTy});
            builder_->CreateCall(initFn, {alloca,
                llvm::ConstantInt::get(usizeTy, entries.size()),
                llvm::ConstantInt::get(usizeTy, elemSz)});
        } else if (baseName == "Vec") {
            auto initFn = declareBuiltin("lucis_vec_init_cap_" + suffix, voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(initFn, {alloca,
                llvm::ConstantInt::get(usizeTy, entries.size())});
        } else {
            // Set has no init_cap variant — use plain init
            auto initFn = declareBuiltin("lucis_set_init_" + suffix, voidTy, {ptrTy});
            builder_->CreateCall(initFn, {alloca});
        }

        // Push/add each element
        for (auto* entry : entries) {
            auto* val = castValue(visit(entry->expression(0)));
            if (isRaw) {
                auto pushName = (baseName == "Vec" ? "lucis_vec_push_raw" : "lucis_set_add_raw");
                auto* tmp = builder_->CreateAlloca(elemLLTy);
                builder_->CreateStore(val, tmp);
                if (baseName == "Vec") {
                    auto fn = declareBuiltin(pushName, voidTy, {ptrTy, ptrTy, usizeTy});
                    builder_->CreateCall(fn, {alloca, tmp, llvm::ConstantInt::get(usizeTy, elemSz)});
                } else {
                    auto fn = declareBuiltin(pushName, llvm::Type::getInt32Ty(*context_), {ptrTy, ptrTy});
                    builder_->CreateCall(fn, {alloca, tmp});
                }
            } else {
                auto pushName = (baseName == "Vec" ? "lucis_vec_push_" : "lucis_set_add_") + suffix;
                auto fn = declareBuiltin(pushName, voidTy, {ptrTy, elemLLTy});
                if (val->getType() != elemLLTy) {
                    if (val->getType()->isIntegerTy() && elemLLTy->isIntegerTy())
                        val = builder_->CreateIntCast(val, elemLLTy, elemTI->isSigned);
                    else if (val->getType()->isFloatingPointTy() && elemLLTy->isFloatingPointTy())
                        val = builder_->CreateFPCast(val, elemLLTy);
                }
                builder_->CreateCall(fn, {alloca, val});
            }
            consumeExprIfOwnedLocal(entry->expression(0));
        }
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(collTy, alloca, baseName + "_val"));
    }

    // ── Map<K,V> ─────────────────────────────────────────────────────
    if (baseName == "Map") {
        if (entries.empty()) {
            auto* mapTy = getOrCreateMapStructType();
            return static_cast<llvm::Value*>(llvm::ConstantAggregateZero::get(mapTy));
        }
        // Resolve key/value types from first entry
        auto* keyTI = resolveExprTypeInfo(entries[0]->expression(0));
        auto* valTI = resolveExprTypeInfo(entries[0]->expression(1));
        if (!keyTI || !valTI) {
            std::cerr << "lucis: cannot determine Map key/value types\n";
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        auto valSuffix = valTI->builtinSuffix.empty() ? "raw" : valTI->builtinSuffix;
        auto suffix = keyTI->builtinSuffix + "_" + valSuffix;

        auto* mapTy  = getOrCreateMapStructType();
        auto* alloca = builder_->CreateAlloca(mapTy, nullptr, "Map_bang");
        bool isRaw = (valSuffix == "raw");
        auto& dlMap = module_->getDataLayout();

        if (isRaw) {
            auto* valLLTy = valTI->toLLVMType(*context_, dlMap);
            auto valSz = dlMap.getTypeAllocSize(valLLTy);
            auto initFn = declareBuiltin("lucis_map_init_" + suffix, voidTy, {ptrTy, usizeTy});
            builder_->CreateCall(initFn, {alloca, llvm::ConstantInt::get(usizeTy, valSz)});
        } else {
            auto initFn = declareBuiltin("lucis_map_init_" + suffix, voidTy, {ptrTy});
            builder_->CreateCall(initFn, {alloca});
        }

        auto* keyLLTy = keyTI->toLLVMType(*context_, module_->getDataLayout());
        auto* valLLTy = valTI->toLLVMType(*context_, module_->getDataLayout());
        auto insertName = "lucis_map_set_" + suffix;

        for (auto* entry : entries) {
            auto* key = castValue(visit(entry->expression(0)));
            auto* val = castValue(visit(entry->expression(1)));
            if (isRaw) {
                auto* kTmp = builder_->CreateAlloca(key->getType());
                builder_->CreateStore(key, kTmp);
                auto* vTmp = builder_->CreateAlloca(val->getType());
                builder_->CreateStore(val, vTmp);
                auto fn = declareBuiltin(insertName, voidTy, {ptrTy, ptrTy, ptrTy});
                builder_->CreateCall(fn, {alloca, kTmp, vTmp});
            } else {
                auto fn = declareBuiltin(insertName, voidTy, {ptrTy, keyLLTy, valLLTy});
                builder_->CreateCall(fn, {alloca, key, val});
            }
            consumeExprIfOwnedLocal(entry->expression(0));
            consumeExprIfOwnedLocal(entry->expression(1));
        }
        return static_cast<llvm::Value*>(
            builder_->CreateLoad(mapTy, alloca, "Map_val"));
    }

    std::cerr << "lucis: unsupported bang-initializer type '" << baseName << "'\n";
    return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitListCompExpr(LucisParser::ListCompExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* typeInfo = resolveTypeInfo(ctx->typeSpec());
    if (!typeInfo) return {};
    auto* varType  = typeInfo->toLLVMType(*context_, module_->getDataLayout());
    auto  varName  = ctx->IDENTIFIER()->getText();
    auto* iterExpr = ctx->expression(1);

    // Determine if iterating over a range
    bool isRange = dynamic_cast<LucisParser::RangeExprContext*>(iterExpr) ||
                   dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr);
    bool isInclusive = dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr) != nullptr;

    bool hasFilter = ctx->IF() != nullptr;

    if (isRange) {
        // Get start and end values
        llvm::Value* startVal;
        llvm::Value* endVal;

        if (isInclusive) {
            auto* rng = dynamic_cast<LucisParser::RangeInclExprContext*>(iterExpr);
            startVal = castValue(visit(rng->expression(0)));
            endVal   = castValue(visit(rng->expression(1)));
        } else {
            auto* rng = dynamic_cast<LucisParser::RangeExprContext*>(iterExpr);
            startVal = castValue(visit(rng->expression(0)));
            endVal   = castValue(visit(rng->expression(1)));
        }

        if (startVal->getType() != varType)
            startVal = builder_->CreateIntCast(startVal, varType, typeInfo->isSigned, "lc.cast");
        if (endVal->getType() != varType)
            endVal = builder_->CreateIntCast(endVal, varType, typeInfo->isSigned, "lc.cast");

        // Compute array size at compile time from constants
        auto* startConst = llvm::dyn_cast<llvm::ConstantInt>(startVal);
        auto* endConst   = llvm::dyn_cast<llvm::ConstantInt>(endVal);

        if (!startConst || !endConst) {
            std::cerr << "lucis: list comprehension range bounds must be constants\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }

        int64_t s = startConst->getSExtValue();
        int64_t e = endConst->getSExtValue();
        if ((!isInclusive && s >= e) || (isInclusive && s > e)) {
            std::cerr << "lucis: invalid range: start (" << s
                      << ") >= end (" << e << ")\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        uint64_t rangeLen = isInclusive ? (e - s + 1) : (e - s);

        // Allocate the loop variable
        auto* loopVar = builder_->CreateAlloca(varType, nullptr, varName);
        builder_->CreateStore(startVal, loopVar);
        locals_[varName] = { loopVar, typeInfo, 0 };

        if (!hasFilter) {
            // No filter: output array is same size as range
            // Evaluate the value expression for each iteration
            auto* valExpr = ctx->expression(0);

            // First evaluate to get the element type
            auto* firstVal = castValue(visit(valExpr));
            auto* elemTy = firstVal->getType();
            auto* arrTy = llvm::ArrayType::get(elemTy, rangeLen);

            // Allocate array on the stack and fill via loop
            auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "lc.arr");
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* idxAlloca = builder_->CreateAlloca(i64Ty, nullptr, "lc.idx");
            builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);

            // Store the first element (already computed)
            auto* gep0 = builder_->CreateInBoundsGEP(
                arrTy, arrAlloca,
                { llvm::ConstantInt::get(i64Ty, 0),
                  llvm::ConstantInt::get(i64Ty, 0) }, "lc.gep");
            builder_->CreateStore(firstVal, gep0);

            // Store idx=1, loopVar = start+1
            builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 1), idxAlloca);
            auto* nextStart = builder_->CreateAdd(startVal,
                llvm::ConstantInt::get(varType, 1), "lc.next");
            builder_->CreateStore(nextStart, loopVar);

            auto* condBB   = llvm::BasicBlock::Create(*context_, "lc.cond", currentFunction_);
            auto* bodyBB   = llvm::BasicBlock::Create(*context_, "lc.body", currentFunction_);
            auto* endBB    = llvm::BasicBlock::Create(*context_, "lc.end", currentFunction_);

            builder_->CreateBr(condBB);

            // Condition
            builder_->SetInsertPoint(condBB);
            auto* cur = builder_->CreateLoad(varType, loopVar, "lc.cur");
            llvm::Value* cond;
            if (isInclusive)
                cond = typeInfo->isSigned
                     ? builder_->CreateICmpSLE(cur, endVal, "lc.cmp")
                     : builder_->CreateICmpULE(cur, endVal, "lc.cmp");
            else
                cond = typeInfo->isSigned
                     ? builder_->CreateICmpSLT(cur, endVal, "lc.cmp")
                     : builder_->CreateICmpULT(cur, endVal, "lc.cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body
            builder_->SetInsertPoint(bodyBB);
            auto* val = castValue(visit(valExpr));
            auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "lc.idx");
            auto* gep = builder_->CreateInBoundsGEP(
                arrTy, arrAlloca,
                { llvm::ConstantInt::get(i64Ty, 0), idx }, "lc.gep");
            builder_->CreateStore(val, gep);

            // Update: idx++, loopVar++
            auto* newIdx = builder_->CreateAdd(idx,
                llvm::ConstantInt::get(i64Ty, 1), "lc.idx.inc");
            builder_->CreateStore(newIdx, idxAlloca);
            auto* curVar = builder_->CreateLoad(varType, loopVar, "lc.var");
            auto* nextVar = builder_->CreateAdd(curVar,
                llvm::ConstantInt::get(varType, 1), "lc.var.inc");
            builder_->CreateStore(nextVar, loopVar);
            builder_->CreateBr(condBB);

            builder_->SetInsertPoint(endBB);
            locals_.erase(varName);

            // Load the completed array from the alloca
            auto* result = builder_->CreateLoad(arrTy, arrAlloca, "lc.result");
            return static_cast<llvm::Value*>(result);
        } else {
            // With filter: use max-size array and write index
            auto* valExpr = ctx->expression(0);
            auto* filterExpr = ctx->expression(2);

            // Pre-evaluate to get element type (use start value)
            auto* firstVal = castValue(visit(valExpr));
            auto* elemTy = firstVal->getType();
            auto* arrTy = llvm::ArrayType::get(elemTy, rangeLen);

            auto* arrAlloca = builder_->CreateAlloca(arrTy, nullptr, "lc.arr");
            // Zero-init
            builder_->CreateStore(llvm::Constant::getNullValue(arrTy), arrAlloca);

            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* writeIdx = builder_->CreateAlloca(i64Ty, nullptr, "lc.widx");
            builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), writeIdx);

            // Check first element against filter
            auto* filterVal0 = castValue(visit(filterExpr));
            auto* filterCond0BB = llvm::BasicBlock::Create(*context_, "lc.filt0.then", currentFunction_);
            auto* filterJoin0BB = llvm::BasicBlock::Create(*context_, "lc.filt0.join", currentFunction_);
            builder_->CreateCondBr(filterVal0, filterCond0BB, filterJoin0BB);

            builder_->SetInsertPoint(filterCond0BB);
            auto* wi0 = builder_->CreateLoad(i64Ty, writeIdx, "lc.widx");
            auto* gep0 = builder_->CreateInBoundsGEP(
                arrTy, arrAlloca,
                { llvm::ConstantInt::get(i64Ty, 0), wi0 }, "lc.gep");
            builder_->CreateStore(firstVal, gep0);
            auto* wi0inc = builder_->CreateAdd(wi0,
                llvm::ConstantInt::get(i64Ty, 1), "lc.widx.inc");
            builder_->CreateStore(wi0inc, writeIdx);
            builder_->CreateBr(filterJoin0BB);

            builder_->SetInsertPoint(filterJoin0BB);

            // Continue loop from start+1
            auto* nextStart = builder_->CreateAdd(startVal,
                llvm::ConstantInt::get(varType, 1), "lc.next");
            builder_->CreateStore(nextStart, loopVar);

            auto* condBB = llvm::BasicBlock::Create(*context_, "lc.cond", currentFunction_);
            auto* bodyBB = llvm::BasicBlock::Create(*context_, "lc.body", currentFunction_);
            auto* storeBB = llvm::BasicBlock::Create(*context_, "lc.store", currentFunction_);
            auto* skipBB  = llvm::BasicBlock::Create(*context_, "lc.skip", currentFunction_);
            auto* endBB   = llvm::BasicBlock::Create(*context_, "lc.end", currentFunction_);

            builder_->CreateBr(condBB);

            // Condition
            builder_->SetInsertPoint(condBB);
            auto* cur = builder_->CreateLoad(varType, loopVar, "lc.cur");
            llvm::Value* cond;
            if (isInclusive)
                cond = typeInfo->isSigned
                     ? builder_->CreateICmpSLE(cur, endVal, "lc.cmp")
                     : builder_->CreateICmpULE(cur, endVal, "lc.cmp");
            else
                cond = typeInfo->isSigned
                     ? builder_->CreateICmpSLT(cur, endVal, "lc.cmp")
                     : builder_->CreateICmpULT(cur, endVal, "lc.cmp");
            builder_->CreateCondBr(cond, bodyBB, endBB);

            // Body — evaluate filter
            builder_->SetInsertPoint(bodyBB);
            auto* val = castValue(visit(valExpr));
            auto* filterVal = castValue(visit(filterExpr));
            builder_->CreateCondBr(filterVal, storeBB, skipBB);

            // Store — filter passed
            builder_->SetInsertPoint(storeBB);
            auto* wi = builder_->CreateLoad(i64Ty, writeIdx, "lc.widx");
            auto* gep = builder_->CreateInBoundsGEP(
                arrTy, arrAlloca,
                { llvm::ConstantInt::get(i64Ty, 0), wi }, "lc.gep");
            builder_->CreateStore(val, gep);
            auto* wiInc = builder_->CreateAdd(wi,
                llvm::ConstantInt::get(i64Ty, 1), "lc.widx.inc");
            builder_->CreateStore(wiInc, writeIdx);
            builder_->CreateBr(skipBB);

            // Skip/Update
            builder_->SetInsertPoint(skipBB);
            auto* curVar = builder_->CreateLoad(varType, loopVar, "lc.var");
            auto* nextVar = builder_->CreateAdd(curVar,
                llvm::ConstantInt::get(varType, 1), "lc.var.inc");
            builder_->CreateStore(nextVar, loopVar);
            builder_->CreateBr(condBB);

            builder_->SetInsertPoint(endBB);
            locals_.erase(varName);

            auto* result = builder_->CreateLoad(arrTy, arrAlloca, "lc.result");
            return static_cast<llvm::Value*>(result);
        }
    }

    std::cerr << "lucis: list comprehension only supports range iterables\n";
    return static_cast<llvm::Value*>(
        llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
}



std::any IRGen::visitNegExpr(LucisParser::NegExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* val = castValue(visit(ctx->expression()));
    if (val->getType()->isFloatingPointTy())
        return static_cast<llvm::Value*>(builder_->CreateFNeg(val, "neg"));
    return static_cast<llvm::Value*>(builder_->CreateNeg(val, "neg"));
}



std::any IRGen::visitMulExpr(LucisParser::MulExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));

    // ── User-defined operator overload ──
    if (lhsTI && lhsTI->kind == TypeKind::Struct) {
        auto opName = opTextToInternal(ctx->op->getText());
        if (!opName.empty()) {
            if (auto* entry = findOperatorEntryInChain(lhsTI, opName)) {
                auto* fn = entry->fn;
                auto* call = fn->getReturnType()->isVoidTy()
                    ? builder_->CreateCall(fn, {lhs, rhs})
                    : builder_->CreateCall(fn, {lhs, rhs}, opName);
                return static_cast<llvm::Value*>(call);
            }
        }
    }

    auto [l, r] = promoteArithmetic(lhs, rhs);
    bool isFloat = l->getType()->isFloatingPointTy();

    // Division by zero check for integer / and %
    if (!isFloat && (ctx->op->getType() == LucisLexer::SLASH ||
                     ctx->op->getType() == LucisLexer::PERCENT)) {
        emitDivByZeroGuard(r, ctx->op);
    }

    switch (ctx->op->getType()) {
    case LucisLexer::STAR:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFMul(l, r, "mul")
                    : builder_->CreateMul(l, r, "mul"));
    case LucisLexer::SLASH:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFDiv(l, r, "div")
                    : (lhsTI->isSigned ? builder_->CreateSDiv(l, r, "div")
                                       : builder_->CreateUDiv(l, r, "div")));
    case LucisLexer::PERCENT:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFRem(l, r, "mod")
                    : (lhsTI->isSigned ? builder_->CreateSRem(l, r, "mod")
                                       : builder_->CreateURem(l, r, "mod")));
    default:
        return static_cast<llvm::Value*>(llvm::UndefValue::get(l->getType()));
    }
}



std::any IRGen::visitAddSubExpr(LucisParser::AddSubExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));

    bool lhsPtr = lhs->getType()->isPointerTy();
    bool rhsPtr = rhs->getType()->isPointerTy();

    // ── Pointer arithmetic ──────────────────────────────────────────
    if (lhsPtr || rhsPtr) {
        auto* i8Ty = llvm::Type::getInt8Ty(*context_);

        // Resolve the pointee LLVM type for proper stride calculation
        auto getPointeeTy = [&](LucisParser::ExpressionContext* e) -> llvm::Type* {
            auto* ti = resolveExprTypeInfo(e);
            if (ti && ti->kind == TypeKind::Pointer && ti->pointeeType) {
                return ti->pointeeType->toLLVMType(*context_, module_->getDataLayout());
            }
            return i8Ty; // fallback: byte-level arithmetic
        };

        if (lhsPtr && !rhsPtr) {
            // ptr +/- int
            auto* elemTy = getPointeeTy(ctx->expression(0));
            if (!elemTy->isSized()) elemTy = i8Ty;
            auto* idx = rhs;
            if (ctx->op->getType() == LucisLexer::MINUS) {
                idx = builder_->CreateNeg(rhs, "neg");
            }
            return static_cast<llvm::Value*>(
                builder_->CreateGEP(elemTy, lhs, idx, "ptradd"));
        }
        if (!lhsPtr && rhsPtr) {
            // int + ptr
            auto* elemTy = getPointeeTy(ctx->expression(1));
            if (!elemTy->isSized()) elemTy = i8Ty;
            return static_cast<llvm::Value*>(
                builder_->CreateGEP(elemTy, rhs, lhs, "ptradd"));
        }
        // ptr - ptr → ptrdiff (i64) — only for subtraction
        if (ctx->op->getType() != LucisLexer::MINUS) {
            std::cerr << "lucis: cannot add two pointers\n";
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
        }
        auto* elemTy = getPointeeTy(ctx->expression(0));
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* lhsInt = builder_->CreatePtrToInt(lhs, i64Ty, "ptr2int");
        auto* rhsInt = builder_->CreatePtrToInt(rhs, i64Ty, "ptr2int");
        auto* diff = builder_->CreateSub(lhsInt, rhsInt, "ptrdiff");
        auto  elemSize = module_->getDataLayout().getTypeAllocSize(elemTy);
        if (elemSize > 1) {
            diff = builder_->CreateExactSDiv(diff,
                llvm::ConstantInt::get(i64Ty, elemSize), "ptrdiff.elems");
        }
        return static_cast<llvm::Value*>(diff);
    }

    // ── String concatenation ────────────────────────────────────────
    {
        auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
        auto* rhsTI = resolveExprTypeInfo(ctx->expression(1));
        if (lhsTI && rhsTI &&
            lhsTI->kind == TypeKind::String && rhsTI->kind == TypeKind::String) {
            auto* ptrTy   = llvm::PointerType::getUnqual(*context_);
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            auto* strTy   = lhs->getType();

            auto* lhsPtr = builder_->CreateExtractValue(lhs, {0}, "lhs.ptr");
            auto* lhsLen = builder_->CreateExtractValue(lhs, {1}, "lhs.len");
            auto* rhsPtr = builder_->CreateExtractValue(rhs, {0}, "rhs.ptr");
            auto* rhsLen = builder_->CreateExtractValue(rhs, {1}, "rhs.len");

            auto callee = declareBuiltin("lucis_concat", strTy,
                                        {ptrTy, usizeTy, ptrTy, usizeTy});
            return static_cast<llvm::Value*>(
                builder_->CreateCall(callee, {lhsPtr, lhsLen, rhsPtr, rhsLen}, "strcat"));
        }
    }

    // ── User-defined operator overload ──
    {
        auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
        if (lhsTI && lhsTI->kind == TypeKind::Struct) {
            auto opName = opTextToInternal(ctx->op->getText());
            if (!opName.empty()) {
                if (auto* entry = findOperatorEntryInChain(lhsTI, opName)) {
                    auto* call = builder_->CreateCall(entry->fn, {lhs, rhs});
                    if (!call->getType()->isVoidTy())
                        call->setName(opName);
                    auto* result = call;
                    return static_cast<llvm::Value*>(result);
                }
            }
        }
    }

    // ── Normal arithmetic ───────────────────────────────────────────
    auto [l, r] = promoteArithmetic(lhs, rhs);
    bool isFloat = l->getType()->isFloatingPointTy();

    switch (ctx->op->getType()) {
    case LucisLexer::PLUS:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFAdd(l, r, "add")
                    : builder_->CreateAdd(l, r, "add"));
    case LucisLexer::MINUS:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFSub(l, r, "sub")
                    : builder_->CreateSub(l, r, "sub"));
    default:
        return static_cast<llvm::Value*>(llvm::UndefValue::get(l->getType()));
    }
}



std::any IRGen::visitLogicalNotExpr(LucisParser::LogicalNotExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* val = castValue(visit(ctx->expression()));
    val = toBool(val, resolveExprTypeInfo(ctx->expression()), "tobool");
    return static_cast<llvm::Value*>(builder_->CreateNot(val, "not"));
}

std::any IRGen::visitLogicalAndExpr(LucisParser::LogicalAndExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn = currentFunction_;
    auto* lhs = castValue(visit(ctx->expression(0)));
    lhs = toBool(lhs, resolveExprTypeInfo(ctx->expression(0)), "tobool");

    // Short-circuit: if lhs is false, skip rhs
    auto* rhsBB   = llvm::BasicBlock::Create(*context_, "and.rhs",  fn);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "and.merge", fn);
    auto* lhsBB   = builder_->GetInsertBlock();
    builder_->CreateCondBr(lhs, rhsBB, mergeBB);

    builder_->SetInsertPoint(rhsBB);
    auto* rhs = castValue(visit(ctx->expression(1)));
    rhs = toBool(rhs, resolveExprTypeInfo(ctx->expression(1)), "tobool");
    auto* rhsEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(llvm::Type::getInt1Ty(*context_), 2, "and");
    phi->addIncoming(llvm::ConstantInt::getFalse(*context_), lhsBB);
    phi->addIncoming(rhs, rhsEndBB);
    return static_cast<llvm::Value*>(phi);
}

std::any IRGen::visitLogicalOrExpr(LucisParser::LogicalOrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* fn = currentFunction_;
    auto* lhs = castValue(visit(ctx->expression(0)));
    lhs = toBool(lhs, resolveExprTypeInfo(ctx->expression(0)), "tobool");

    // Short-circuit: if lhs is true, skip rhs
    auto* rhsBB   = llvm::BasicBlock::Create(*context_, "or.rhs",  fn);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "or.merge", fn);
    auto* lhsBB   = builder_->GetInsertBlock();
    builder_->CreateCondBr(lhs, mergeBB, rhsBB);

    builder_->SetInsertPoint(rhsBB);
    auto* rhs = castValue(visit(ctx->expression(1)));
    rhs = toBool(rhs, resolveExprTypeInfo(ctx->expression(1)), "tobool");
    auto* rhsEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(llvm::Type::getInt1Ty(*context_), 2, "or");
    phi->addIncoming(llvm::ConstantInt::getTrue(*context_), lhsBB);
    phi->addIncoming(rhs, rhsEndBB);
    return static_cast<llvm::Value*>(phi);
}



std::any IRGen::visitBitNotExpr(LucisParser::BitNotExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* val = castValue(visit(ctx->expression()));
    val = ptrToIntIfNeeded(val);
    return static_cast<llvm::Value*>(builder_->CreateNot(val, "bitnot"));
}

std::any IRGen::visitLshiftExpr(LucisParser::LshiftExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    {
        auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
        if (lhsTI && lhsTI->kind == TypeKind::Struct) {
            if (auto* entry = findOperatorEntryInChain(lhsTI, "opShl")) {
                auto* call = builder_->CreateCall(entry->fn, {lhs, rhs});
                    if (!call->getType()->isVoidTy())
                        call->setName("opShl");
                    auto* result = call;
                return static_cast<llvm::Value*>(result);
            }
        }
    }
    lhs = ptrToIntIfNeeded(lhs);
    rhs = ptrToIntIfNeeded(rhs);
    auto [l, r] = promoteArithmetic(lhs, rhs);
    return static_cast<llvm::Value*>(builder_->CreateShl(l, r, "shl"));
}

std::any IRGen::visitRshiftExpr(LucisParser::RshiftExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    {
        auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
        if (lhsTI && lhsTI->kind == TypeKind::Struct) {
            if (auto* entry = findOperatorEntryInChain(lhsTI, "opShr")) {
                auto* call = builder_->CreateCall(entry->fn, {lhs, rhs});
                    if (!call->getType()->isVoidTy())
                        call->setName("opShr");
                    auto* result = call;
                return static_cast<llvm::Value*>(result);
            }
        }
    }
    lhs = ptrToIntIfNeeded(lhs);
    rhs = ptrToIntIfNeeded(rhs);
    auto [l, r] = promoteArithmetic(lhs, rhs);
    auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
    bool signedShift = !lhsTI || lhsTI->isSigned;
    return static_cast<llvm::Value*>(
        signedShift ? builder_->CreateAShr(l, r, "shr")
                    : builder_->CreateLShr(l, r, "shr"));
}

std::any IRGen::visitBitAndExpr(LucisParser::BitAndExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    lhs = ptrToIntIfNeeded(lhs);
    rhs = ptrToIntIfNeeded(rhs);
    auto [l, r] = promoteArithmetic(lhs, rhs);
    return static_cast<llvm::Value*>(builder_->CreateAnd(l, r, "bitand"));
}

std::any IRGen::visitBitXorExpr(LucisParser::BitXorExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    lhs = ptrToIntIfNeeded(lhs);
    rhs = ptrToIntIfNeeded(rhs);
    auto [l, r] = promoteArithmetic(lhs, rhs);
    return static_cast<llvm::Value*>(builder_->CreateXor(l, r, "bitxor"));
}

std::any IRGen::visitBitOrExpr(LucisParser::BitOrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    lhs = ptrToIntIfNeeded(lhs);
    rhs = ptrToIntIfNeeded(rhs);
    auto [l, r] = promoteArithmetic(lhs, rhs);
    return static_cast<llvm::Value*>(builder_->CreateOr(l, r, "bitor"));
}

// Helper: resolve an lvalue expression to its pointer and LLVM type.
// Supports: bare variable (x), struct field (p.x), pointer dereference (*p).


std::pair<llvm::Value*, llvm::Type*>
IRGen::resolveIncrDecrTarget(LucisParser::ExpressionContext* expr) {
    std::pair<llvm::Value*, llvm::Type*> undef = { nullptr, nullptr };

    // Vec element raw pointer helper (vecElemPtr[index]), shared by the
    // indexed cases below.
    auto vecElemPtr = [&](LucisParser::ExpressionContext* base,
                          LucisParser::ExpressionContext* idxExpr,
                          const TypeInfo** outElemTI,
                          llvm::Type** outElemLLTy) -> llvm::Value* {
        auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(base);
        if (!ident) return nullptr;
        auto varName = ident->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it == locals_.end()) return nullptr;
        auto* baseTI = it->second.typeInfo;
        if (!baseTI || baseTI->kind != TypeKind::Extended ||
            baseTI->extendedKind != "Vec") {
            std::cerr << "lucis: ++/-- index requires a vec<T>\n";
            return nullptr;
        }
        const TypeInfo* elemTI = baseTI->elementType;
        auto* elemLLTy = elemTI->toLLVMType(*context_, module_->getDataLayout());
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto& dl = module_->getDataLayout();
        auto* elemSz = llvm::ConstantInt::get(
            usizeTy, dl.getTypeAllocSize(elemLLTy));
        auto ptrFn = declareBuiltin("lucis_vec_ptr_raw", ptrTy,
                                    {ptrTy, usizeTy, usizeTy});
        auto* idxVal = castValue(visit(idxExpr));
        if (idxVal->getType() != usizeTy)
            idxVal = builder_->CreateIntCast(idxVal, usizeTy, false);
        *outElemTI = elemTI;
        *outElemLLTy = elemLLTy;
        return builder_->CreateCall(ptrFn, {it->second.alloca, idxVal, elemSz},
                                    varName + "_incr_elem");
    };

    // Case 1: bare variable (x++ / ++x)
    if (auto* ident = dynamic_cast<LucisParser::IdentExprContext*>(expr)) {
        auto name = ident->IDENTIFIER()->getText();
        auto it = locals_.find(name);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << name << "'\n";
            return undef;
        }
        auto* alloca = it->second.alloca;
        return { alloca, alloca->getAllocatedType() };
    }

    // Case 2: struct field access (p.x++ / ++p.x)
    if (auto* fa = dynamic_cast<LucisParser::FieldAccessExprContext*>(expr)) {
        auto fieldName = fa->IDENTIFIER()->getText();
        auto* baseIdent = dynamic_cast<LucisParser::IdentExprContext*>(fa->expression());
        if (!baseIdent) {
            // v[i].field++ — increment a field of a vec element.
            if (auto* idxBase =
                    dynamic_cast<LucisParser::IndexExprContext*>(fa->expression())) {
                const TypeInfo* elemTI = nullptr;
                llvm::Type* elemLLTy = nullptr;
                auto* elemPtr = vecElemPtr(idxBase->expression(0),
                                           idxBase->expression(1),
                                           &elemTI, &elemLLTy);
                if (!elemPtr) return undef;
                if (!elemTI || (elemTI->kind != TypeKind::Struct &&
                                elemTI->kind != TypeKind::Union)) {
                    std::cerr << "lucis: ++/-- field access requires a "
                              << "struct element\n";
                    return undef;
                }
                auto f2 = fa->IDENTIFIER()->getText();
                int fIdx = -1;
                const TypeInfo* fTI = nullptr;
                for (size_t f3 = 0; f3 < elemTI->fields.size(); f3++) {
                    if (elemTI->fields[f3].name == f2) {
                        fIdx = static_cast<int>(f3);
                        fTI = elemTI->fields[f3].typeInfo;
                        break;
                    }
                }
                if (fIdx < 0) {
                    std::cerr << "lucis: '" << elemTI->name << "' has no "
                              << "field '" << f2 << "'\n";
                    return undef;
                }
                auto* fLLTy = fTI->toLLVMType(*context_, module_->getDataLayout());
                auto* gep = builder_->CreateStructGEP(
                    elemLLTy, elemPtr, fIdx, f2 + "_incr_ptr");
                return { gep, fLLTy };
            }
            std::cerr << "lucis: ++/-- on nested field access not supported\n";
            return undef;
        }
        auto varName = baseIdent->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it == locals_.end()) {
            std::cerr << "lucis: undefined variable '" << varName << "'\n";
            return undef;
        }
        auto* alloca   = it->second.alloca;
        auto* structTI = it->second.typeInfo;

        // Auto-dereference pointer-to-struct/union for &self in extend methods
        if (structTI && structTI->kind == TypeKind::Pointer && structTI->pointeeType) {
            auto* ptrVal = builder_->CreateLoad(
                llvm::PointerType::getUnqual(*context_), alloca, "selfptr");
            auto* pointeeTI = structTI->pointeeType;
            int pFieldIdx = -1;
            const TypeInfo* pFieldTI = nullptr;
            for (size_t f = 0; f < pointeeTI->fields.size(); f++) {
                if (pointeeTI->fields[f].name == fieldName) {
                    pFieldIdx = static_cast<int>(f);
                    pFieldTI = pointeeTI->fields[f].typeInfo;
                    break;
                }
            }
            if (pFieldIdx < 0) {
                std::cerr << "lucis: '" << pointeeTI->name
                          << "' has no field '" << fieldName << "'\n";
                return undef;
            }
            auto* pFieldLLTy = pFieldTI->toLLVMType(*context_, module_->getDataLayout());
            auto* structLLTy = llvm::StructType::getTypeByName(*context_, pointeeTI->name);
            if (!structLLTy) {
                structLLTy = static_cast<llvm::StructType*>(
                    pointeeTI->toLLVMType(*context_, module_->getDataLayout()));
            }
            auto* gep = builder_->CreateStructGEP(
                structLLTy, ptrVal, pFieldIdx, fieldName + "_ptr");
            return { gep, pFieldLLTy };
        }

        if (!structTI || (structTI->kind != TypeKind::Struct &&
                          structTI->kind != TypeKind::Union)) {
            std::cerr << "lucis: '" << varName << "' is not a struct or union\n";
            return undef;
        }
        int fieldIdx = -1;
        const TypeInfo* fieldTI = nullptr;
        for (size_t f = 0; f < structTI->fields.size(); f++) {
            if (structTI->fields[f].name == fieldName) {
                fieldIdx = static_cast<int>(f);
                fieldTI = structTI->fields[f].typeInfo;
                break;
            }
        }
        if (fieldIdx < 0) {
            std::cerr << "lucis: '" << structTI->name
                      << "' has no field '" << fieldName << "'\n";
            return undef;
        }
        auto* fieldLLTy = fieldTI->toLLVMType(*context_, module_->getDataLayout());
        if (structTI->kind == TypeKind::Union) {
            return { alloca, fieldLLTy };
        }
        auto* structTy = alloca->getAllocatedType();
        auto* gep = builder_->CreateStructGEP(structTy, alloca,
                                               fieldIdx, fieldName + "_ptr");
        return { gep, fieldLLTy };
    }

    // Case 3: pointer dereference (*p++ / ++*p)
    if (auto* deref = dynamic_cast<LucisParser::DerefExprContext*>(expr)) {
        auto* ptrVal = castValue(visit(deref->expression()));
        auto* recvTI = resolveExprTypeInfo(deref->expression());
        if (!recvTI || recvTI->kind != TypeKind::Pointer || !recvTI->elementType) {
            std::cerr << "lucis: ++/-- dereference requires a pointer\n";
            return undef;
        }
        auto* elemTy = recvTI->elementType->toLLVMType(*context_,
                                                         module_->getDataLayout());
        return { ptrVal, elemTy };
    }

    // Case 4: indexed collection element (tokens[i]++ / v[i].field++).
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(expr)) {
        const TypeInfo* elemTI = nullptr;
        llvm::Type* elemLLTy = nullptr;
        auto* elemPtr = vecElemPtr(idx->expression(0), idx->expression(1),
                                   &elemTI, &elemLLTy);
        if (!elemPtr) return undef;
        if (elemTI && (elemTI->kind == TypeKind::Struct ||
                       elemTI->kind == TypeKind::Union)) {
            std::cerr << "lucis: ++/-- on a struct/union element is not "
                      << "supported (use a field)\n";
            return undef;
        }
        return { elemPtr, elemLLTy };
    }

    std::cerr << "lucis: ++/-- requires a variable (lvalue)\n";
    return undef;
}



std::any IRGen::visitPreIncrExpr(LucisParser::PreIncrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto [ptr, ty] = resolveIncrDecrTarget(ctx->expression());
    if (!ptr)
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));

    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    if (exprTI && exprTI->kind == TypeKind::Pointer && exprTI->pointeeType) {
        // Pointer ++: GEP with stride 1
        llvm::Value* ptrVal = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), ptr, "preinc_ptr");
        auto* pointeeLLTy = exprTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        auto* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 1);
        auto* newPtr = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, ptrVal, one, "ptrinc"));
        builder_->CreateStore(newPtr, ptr);
        return newPtr;
    }

    auto* cur = builder_->CreateLoad(ty, ptr, "preinc_load");
    auto* one = llvm::ConstantInt::get(ty, 1);
    auto* inc = builder_->CreateAdd(cur, one, "preinc");
    builder_->CreateStore(inc, ptr);
    return static_cast<llvm::Value*>(inc);
}

std::any IRGen::visitPreDecrExpr(LucisParser::PreDecrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto [ptr, ty] = resolveIncrDecrTarget(ctx->expression());
    if (!ptr)
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));

    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    if (exprTI && exprTI->kind == TypeKind::Pointer && exprTI->pointeeType) {
        // Pointer --: GEP with stride -1
        llvm::Value* ptrVal = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), ptr, "predec_ptr");
        auto* pointeeLLTy = exprTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        auto* negOne = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), -1, true);
        auto* newPtr = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, ptrVal, negOne, "ptrdec"));
        builder_->CreateStore(newPtr, ptr);
        return newPtr;
    }

    auto* cur = builder_->CreateLoad(ty, ptr, "predec_load");
    auto* one = llvm::ConstantInt::get(ty, 1);
    auto* dec = builder_->CreateSub(cur, one, "predec");
    builder_->CreateStore(dec, ptr);
    return static_cast<llvm::Value*>(dec);
}

std::any IRGen::visitPostIncrExpr(LucisParser::PostIncrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto [ptr, ty] = resolveIncrDecrTarget(ctx->expression());
    if (!ptr)
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));

    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    if (exprTI && exprTI->kind == TypeKind::Pointer && exprTI->pointeeType) {
        // Pointer ++: GEP with stride 1, return old pointer
        llvm::Value* old = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), ptr, "postinc_old");
        auto* pointeeLLTy = exprTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        auto* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 1);
        auto* newPtr = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, old, one, "ptrinc"));
        builder_->CreateStore(newPtr, ptr);
        return old;
    }

    auto* old = builder_->CreateLoad(ty, ptr, "postinc_load");
    auto* one = llvm::ConstantInt::get(ty, 1);
    auto* inc = builder_->CreateAdd(old, one, "postinc");
    builder_->CreateStore(inc, ptr);
    return static_cast<llvm::Value*>(old);
}

std::any IRGen::visitPostDecrExpr(LucisParser::PostDecrExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto [ptr, ty] = resolveIncrDecrTarget(ctx->expression());
    if (!ptr)
        return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));

    auto* exprTI = resolveExprTypeInfo(ctx->expression());
    if (exprTI && exprTI->kind == TypeKind::Pointer && exprTI->pointeeType) {
        // Pointer --: GEP with stride -1, return old pointer
        llvm::Value* old = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), ptr, "postdec_old");
        auto* pointeeLLTy = exprTI->pointeeType->toLLVMType(*context_, module_->getDataLayout());
        auto* negOne = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), -1, true);
        auto* newPtr = static_cast<llvm::Value*>(
            builder_->CreateGEP(pointeeLLTy, old, negOne, "ptrdec"));
        builder_->CreateStore(newPtr, ptr);
        return old;
    }

    auto* old = builder_->CreateLoad(ty, ptr, "postdec_load");
    auto* one = llvm::ConstantInt::get(ty, 1);
    auto* dec = builder_->CreateSub(old, one, "postdec");
    builder_->CreateStore(dec, ptr);
    return static_cast<llvm::Value*>(old);
}



std::any IRGen::visitCastExpr(LucisParser::CastExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* val = castValue(visit(ctx->expression()));
    auto* ti = resolveTypeInfo(ctx->typeSpec());
    if (!ti) return static_cast<llvm::Value*>(val);
    auto* targetTy = ti->toLLVMType(*context_, module_->getDataLayout());
    // A slice type spec ([]T) resolves to the element type here — rebuild
    // the slice ABI so `bytes as []uint8` casts to {ptr, len}.
    if (countArrayDims(ctx->typeSpec()) > 0) {
        auto* ptrTy = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        targetTy = llvm::StructType::get(*context_, {ptrTy, usizeTy});
    }
    auto* srcTy = val->getType();

    if (srcTy == targetTy)
        return static_cast<llvm::Value*>(val);

    auto* srcTI = resolveExprTypeInfo(ctx->expression());
    bool srcSigned = srcTI ? srcTI->isSigned : false;

    // int -> int
    if (srcTy->isIntegerTy() && targetTy->isIntegerTy()) {
        return static_cast<llvm::Value*>(
            builder_->CreateIntCast(val, targetTy, srcSigned, "cast"));
    }
    // float -> float
    if (srcTy->isFloatingPointTy() && targetTy->isFloatingPointTy()) {
        if (srcTy->getPrimitiveSizeInBits() > targetTy->getPrimitiveSizeInBits())
            return static_cast<llvm::Value*>(
                builder_->CreateFPTrunc(val, targetTy, "cast"));
        else
            return static_cast<llvm::Value*>(
                builder_->CreateFPExt(val, targetTy, "cast"));
    }
    // int -> float
    if (srcTy->isIntegerTy() && targetTy->isFloatingPointTy()) {
        return static_cast<llvm::Value*>(
            srcSigned ? builder_->CreateSIToFP(val, targetTy, "cast")
                      : builder_->CreateUIToFP(val, targetTy, "cast"));
    }
    // float -> int
    if (srcTy->isFloatingPointTy() && targetTy->isIntegerTy()) {
        return static_cast<llvm::Value*>(
            ti->isSigned ? builder_->CreateFPToSI(val, targetTy, "cast")
                         : builder_->CreateFPToUI(val, targetTy, "cast"));
    }
    // pointer -> pointer (bitcast is a no-op with opaque pointers)
    if (srcTy->isPointerTy() && targetTy->isPointerTy())
        return static_cast<llvm::Value*>(val);
    // int -> pointer
    if (srcTy->isIntegerTy() && targetTy->isPointerTy())
        return static_cast<llvm::Value*>(
            builder_->CreateIntToPtr(val, targetTy, "cast"));
    // pointer -> int
    if (srcTy->isPointerTy() && targetTy->isIntegerTy())
        return static_cast<llvm::Value*>(
            builder_->CreatePtrToInt(val, targetTy, "cast"));
    // array -> pointer (explicit cast, e.g. `arr as *T`)
    // Reuse the source alloca from the load to avoid copying the whole array.
    if (srcTy->isArrayTy() && targetTy->isPointerTy()) {
        llvm::Value* srcAlloca = nullptr;
        llvm::LoadInst* deadLoad = nullptr;
        if (auto* li = llvm::dyn_cast<llvm::LoadInst>(val)) {
            srcAlloca = li->getPointerOperand();
            deadLoad = li;
        }
        if (!srcAlloca) {
            srcAlloca = builder_->CreateAlloca(srcTy, nullptr, "arr.cast");
            builder_->CreateStore(val, srcAlloca);
        }
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
        auto* gep = builder_->CreateGEP(srcTy, srcAlloca, {zero, zero}, "arr.ptr");
        // Remove the now-dead load — large aggregate loads stall SelectionDAG.
        if (deadLoad && deadLoad->use_empty())
            deadLoad->eraseFromParent();
        return static_cast<llvm::Value*>(gep);
    }
    // [N]uint8 → string / []uint8: build the {ptr, len} fat pointer from the
    // fixed array (reuses the source alloca, like array→pointer above).
    if (srcTy->isArrayTy() && targetTy->isStructTy() &&
        targetTy->getNumContainedTypes() == 2 &&
        targetTy->getContainedType(0)->isPointerTy() &&
        srcTy->getArrayElementType()->isIntegerTy(8)) {
        llvm::Value* srcAlloca = nullptr;
        if (auto* li = llvm::dyn_cast<llvm::LoadInst>(val)) {
            srcAlloca = li->getPointerOperand();
        }
        if (!srcAlloca) {
            srcAlloca = builder_->CreateAlloca(srcTy, nullptr, "bytes.cast");
            builder_->CreateStore(val, srcAlloca);
        }
        auto* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0);
        auto* ptr = builder_->CreateGEP(srcTy, srcAlloca, {zero, zero}, "bytes.ptr");
        auto* lenTy = targetTy->getContainedType(1);
        auto* len = llvm::ConstantInt::get(
            lenTy, srcTy->getArrayNumElements());
        llvm::Value* agg = llvm::UndefValue::get(targetTy);
        agg = builder_->CreateInsertValue(agg, ptr, 0);
        agg = builder_->CreateInsertValue(agg, len, 1);
        return static_cast<llvm::Value*>(agg);
    }
    // []uint8 <-> string: ABI-identical {ptr, len} fat pointers — coerce
    // field-by-field (same layout, different struct identity).
    if (srcTy->isStructTy() && targetTy->isStructTy() &&
        srcTy->getNumContainedTypes() == targetTy->getNumContainedTypes()) {
        auto* coerced = coerceValueToType(val, targetTy, srcSigned);
        if (coerced)
            return static_cast<llvm::Value*>(coerced);
    }

    std::cerr << "lucis: unsupported cast\n";
    return static_cast<llvm::Value*>(val);
}



std::any IRGen::visitSizeofExpr(LucisParser::SizeofExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* spec = ctx->typeSpec();
    std::vector<uint64_t> arraySizes;
    while (spec && spec->LBRACKET()) {
        if (!spec->INT_LIT()) {
            std::cerr << "lucis: sizeof requires fixed-size array type\n";
            auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(usizeTy, 0));
        }
        arraySizes.push_back(std::stoull(spec->INT_LIT()->getText()));
        if (spec->typeSpec().empty()) break;
        spec = spec->typeSpec(0);
    }

    auto* ti = resolveTypeInfo(spec);
    if (!ti) return {};
    auto* llvmTy = ti->toLLVMType(*context_, module_->getDataLayout());
    for (auto it = arraySizes.rbegin(); it != arraySizes.rend(); ++it)
        llvmTy = llvm::ArrayType::get(llvmTy, *it);

    auto& dl = module_->getDataLayout();
    uint64_t sizeBytes = dl.getTypeAllocSize(llvmTy);
    auto* usizeTy = dl.getIntPtrType(*context_);
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(usizeTy, sizeBytes));
}



std::any IRGen::visitTypeofExpr(LucisParser::TypeofExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Resolve expression type at compile-time
    auto* exprVal = castValue(visit(ctx->expression()));

    // Try to find TypeInfo for the expression
    std::string typeName = "unknown";
    if (auto* exprTI = resolveExprTypeInfo(ctx->expression())) {
        if (!exprTI->name.empty())
            typeName = exprTI->name;
    }

    if (typeName == "unknown") {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(exprVal)) {
        auto* src = load->getPointerOperand();
        for (auto& [vname, info] : locals_) {
            if (info.alloca == src) {
                typeName = info.typeInfo->name;
                break;
            }
        }
        // Struct field lookup via GEP
        if (typeName == "unknown") {
            if (auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(src)) {
                auto* base = gep->getPointerOperand();
                for (auto& [vname, info] : locals_) {
                    if (info.alloca == base &&
                        info.typeInfo && info.typeInfo->kind == TypeKind::Struct) {
                        if (gep->getNumIndices() == 2) {
                            if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(
                                    gep->getOperand(2))) {
                                unsigned idx = ci->getZExtValue();
                                if (idx < info.typeInfo->fields.size())
                                    typeName = info.typeInfo->fields[idx].typeInfo->name;
                            }
                        }
                        break;
                    }
                }
            }
        }
        } else {
            // Infer from LLVM type for literals/constants
            auto* ty = exprVal->getType();
            if (ty->isIntegerTy(1))       typeName = "bool";
            else if (ty->isIntegerTy(8))  typeName = "int8";
            else if (ty->isIntegerTy(16)) typeName = "int16";
            else if (ty->isIntegerTy(32)) typeName = "int32";
            else if (ty->isIntegerTy(64)) typeName = "int64";
            else if (ty->isIntegerTy(128)) typeName = "int128";
            else if (ty->isIntegerTy(256)) typeName = "int32";  // untyped literal
            else if (ty->isFloatTy())     typeName = "float32";
            else if (ty->isDoubleTy())    typeName = "float64";
            else if (ty->isStructTy())    typeName = "string";
            else if (ty->isPointerTy())   typeName = "pointer";
        }
    }

// ── visitAlignofExpr ──────────────────────────────────────────────────────
    // AST-based fallback for expression kinds not covered by Load/GEP heuristics
    if (typeName == "unknown") {
        if (auto* exprTI = resolveExprTypeInfo(ctx->expression())) {
            if (!exprTI->name.empty())
                typeName = exprTI->name;
        }
    }

    // Additional AST-based fallback for method calls and function calls
    // that may still need specialized return-type resolution strings.
    if (typeName == "unknown") {
        auto* exprCtx = ctx->expression();

        if (auto* mc = dynamic_cast<LucisParser::MethodCallExprContext*>(exprCtx)) {
            auto* receiverTI = resolveExprTypeInfo(mc->expression());
            if (receiverTI) {
                std::string mname = mc->IDENTIFIER()->getText();

                if (receiverTI->kind == TypeKind::Extended) {
                    // Map.keys() → Vec<K>, Map.values() → Vec<V>
                    if (receiverTI->extendedKind == "Map") {
                        if (mname == "keys" && receiverTI->keyType)
                            typeName = "Vec<" + receiverTI->keyType->name + ">";
                        else if (mname == "values" && receiverTI->valueType)
                            typeName = "Vec<" + receiverTI->valueType->name + ">";
                    }
                    // Set.values() → Vec<T>
                    else if (receiverTI->extendedKind == "Set") {
                        if (mname == "values" && receiverTI->elementType)
                            typeName = "Vec<" + receiverTI->elementType->name + ">";
                    }

                    // Generic Extended type method lookup
                    if (typeName == "unknown") {
                        auto* desc = extTypeRegistry_.lookup(receiverTI->extendedKind);
                        if (desc) {
                            for (auto& m : desc->methods) {
                                if (m.name == mname) {
                                    if (m.returnType == "_self")
                                        typeName = receiverTI->name;
                                    else
                                        typeName = m.returnType;
                                    break;
                                }
                            }
                        }
                    }
                }
                // String / Integer / Float method lookup
                else if (receiverTI->kind == TypeKind::String ||
                         receiverTI->kind == TypeKind::Integer ||
                         receiverTI->kind == TypeKind::Float) {
                    auto* md = methodRegistry_.lookup(receiverTI->kind,
                                                      mc->IDENTIFIER()->getText());
                    if (md) {
                        typeName = (md->returnType == "_self")
                                       ? receiverTI->name
                                       : md->returnType;
                    }
                }
            }
        }
    }

    auto typeDims = resolveExprArrayDims(ctx->expression());
    if (typeDims > 0) {
        std::vector<unsigned> sizes;
        if (inferArraySizesFromLLVMType(exprVal->getType(), typeDims, sizes) &&
            sizes.size() == typeDims) {
            std::string withSizes;
            for (auto sz : sizes)
                withSizes += "[" + std::to_string(sz) + "]";
            typeName = withSizes + typeName;
        } else {
            std::string withDims;
            for (unsigned i = 0; i < typeDims; i++)
                withDims += "[]";
            typeName = withDims + typeName;
        }
    }

    // Emit as string constant (fat pointer: {ptr, len})
    auto* strGlobal = builder_->CreateGlobalString(typeName, ".typeof", 0, module_);
    auto* strTy   = typeRegistry_.lookup("string")
                        ->toLLVMType(*context_, module_->getDataLayout());
    auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
    auto* len     = llvm::ConstantInt::get(usizeTy, typeName.size());

    llvm::Value* strStruct = llvm::UndefValue::get(strTy);
    strStruct = builder_->CreateInsertValue(strStruct, strGlobal, 0, "str_ptr");
    strStruct = builder_->CreateInsertValue(strStruct, len,       1, "str_len");
    return static_cast<llvm::Value*>(strStruct);
}


std::any IRGen::visitAlignofExpr(LucisParser::AlignofExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* spec = ctx->typeSpec();
    auto* ti = resolveTypeInfo(spec);
    if (!ti) return {};
    auto* llvmTy = ti->toLLVMType(*context_, module_->getDataLayout());
    auto& dl = module_->getDataLayout();
    uint64_t alignBytes = dl.getABITypeAlign(llvmTy).value();
    auto* usizeTy = dl.getIntPtrType(*context_);
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(usizeTy, alignBytes));
}

// ── visitOffsetofExpr ────────────────────────────────────────────────────
std::any IRGen::visitOffsetofExpr(LucisParser::OffsetofExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* spec = ctx->typeSpec();
    auto* ti = resolveTypeInfo(spec);
    if (!ti) return {};

    auto& dl = module_->getDataLayout();
    auto* structTy = llvm::dyn_cast<llvm::StructType>(
        ti->toLLVMType(*context_, dl));
    if (!structTy) {
                std::cerr << "lucis: offsetof requires a struct/union type\n";

        auto* usizeTy = dl.getIntPtrType(*context_);
        return static_cast<llvm::Value*>(
            llvm::ConstantInt::get(usizeTy, 0));
    }

    std::string fieldName = ctx->IDENTIFIER()->getText();
    int fieldIdx = -1;
    for (size_t i = 0; i < ti->fields.size(); i++) {
        if (ti->fields[i].name == fieldName) {
            fieldIdx = static_cast<int>(i);
            break;
        }
    }
    if (fieldIdx < 0) {
                std::cerr << "lucis: offsetof: field '" << fieldName << "' not found in type\n";

        auto* usizeTy = dl.getIntPtrType(*context_);
        return static_cast<llvm::Value*>(
            llvm::ConstantInt::get(usizeTy, 0));
    }

    // Get struct layout via the DataLayout
    auto* layout = dl.getStructLayout(structTy);
    uint64_t offset = layout->getElementOffset(static_cast<unsigned>(fieldIdx));
    auto* usizeTy = dl.getIntPtrType(*context_);
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(usizeTy, offset));
}




std::any IRGen::visitTernaryExpr(LucisParser::TernaryExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* cond = castValue(visit(ctx->expression(0)));
    cond = toBool(cond, resolveExprTypeInfo(ctx->expression(0)), "tobool");

    auto* currentFn = builder_->GetInsertBlock()->getParent();
    auto* trueBB  = llvm::BasicBlock::Create(*context_, "ternary.true");
    auto* falseBB = llvm::BasicBlock::Create(*context_, "ternary.false");
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "ternary.merge");

    builder_->CreateCondBr(cond, trueBB, falseBB);

    // True branch — evaluate only if condition is true
    currentFn->insert(currentFn->end(), trueBB);
    builder_->SetInsertPoint(trueBB);
    auto* trueVal = castValue(visit(ctx->expression(1)));
    builder_->CreateBr(mergeBB);
    // Refresh trueBB in case the visit created new blocks after it
    auto* trueBBFinal = builder_->GetInsertBlock();

    // False branch — evaluate only if condition is false
    currentFn->insert(currentFn->end(), falseBB);
    builder_->SetInsertPoint(falseBB);
    auto* falseVal = castValue(visit(ctx->expression(2)));
    builder_->CreateBr(mergeBB);
    auto* falseBBFinal = builder_->GetInsertBlock();

    // Merge block — phi selects the right value
    currentFn->insert(currentFn->end(), mergeBB);
    builder_->SetInsertPoint(mergeBB);

    auto [t, f] = promoteArithmetic(trueVal, falseVal);
    auto* phi = builder_->CreatePHI(t->getType(), 2, "ternary");
    phi->addIncoming(t, trueBBFinal);
    phi->addIncoming(f, falseBBFinal);
    return static_cast<llvm::Value*>(phi);
}



std::any IRGen::visitIsExpr(LucisParser::IsExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* val = castValue(visit(ctx->expression()));
    auto* targetTI = resolveTypeInfo(ctx->typeSpec());
    if (!targetTI) {
        return static_cast<llvm::Value*>(
            llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
    }

    // Variant identity check: value is EnumType::Variant
    if (ctx->SCOPE()) {
        if (!targetTI || targetTI->kind != TypeKind::Enum) {
            std::cerr << "lucis: right side of variant 'is' check must be an enum type\n";
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
        }

        auto* variantNode = ctx->IDENTIFIER(0);
        if (!variantNode) {
            std::cerr << "lucis: invalid enum variant in 'is' check\n";
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
        }
        auto variantName = variantNode->getText();

        const EnumVariantInfo* variantInfo = nullptr;
        for (const auto& info : targetTI->enumVariantInfos) {
            if (info.name == variantName) {
                variantInfo = &info;
                break;
            }
        }
        if (!variantInfo) {
            std::cerr << "lucis: enum '" << targetTI->name
                      << "' has no variant '" << variantName << "'\n";
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
        }

        auto* i32Ty = llvm::Type::getInt32Ty(*context_);
        llvm::Value* lhsTag = nullptr;

        if (auto* st = llvm::dyn_cast<llvm::StructType>(val->getType())) {
            if (st->getNumElements() > 0) {
                lhsTag = builder_->CreateExtractValue(val, {0}, "enum.tag");
            }
        } else if (val->getType()->isIntegerTy()) {
            lhsTag = val;
        }

        if (!lhsTag) {
            return static_cast<llvm::Value*>(
                llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
        }

        if (lhsTag->getType() != i32Ty) {
            if (lhsTag->getType()->isIntegerTy(1))
                lhsTag = builder_->CreateZExt(lhsTag, i32Ty, "enum.tag.zext");
            else if (lhsTag->getType()->isIntegerTy())
                lhsTag = builder_->CreateIntCast(lhsTag, i32Ty, false, "enum.tag.cast");
            else
                return static_cast<llvm::Value*>(
                    llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0));
        }

        auto* expectedTag = llvm::ConstantInt::get(i32Ty, variantInfo->discriminant);
        auto* cmpResult = builder_->CreateICmpEQ(lhsTag, expectedTag, "is.variant");

        // Binding: if result is EnumType::Variant(bindingName) { ... }
        // Set up pendingIsBinding_ so visitIfStmt can inject the payload
        // into locals_ before emitting the true-branch body.
        if (ctx->LPAREN() && ctx->IDENTIFIER(1) && !variantInfo->payloadFields.empty()) {
            auto bindingName = ctx->IDENTIFIER(1)->getText();
            auto& dl = module_->getDataLayout();
            auto* enumLLTy = targetTI->toLLVMType(*context_, dl);
            if (auto* st = llvm::dyn_cast<llvm::StructType>(enumLLTy)) {
                if (st->getNumElements() > 1) {
                    auto* tempAlloca = builder_->CreateAlloca(enumLLTy, nullptr, "is.enum.tmp");
                    builder_->CreateStore(val, tempAlloca);
                    auto* rawPayloadPtr = builder_->CreateStructGEP(enumLLTy, tempAlloca, 1, "is.payload.ptr");
                    auto* payloadLLTy = getEnumVariantPayloadType(*variantInfo);
                    if (payloadLLTy) {
                        auto* typedPtr = builder_->CreateBitCast(
                            rawPayloadPtr,
                            llvm::PointerType::getUnqual(*context_),
                            "is.payload.cast");
                        const TypeInfo* payloadTI = nullptr;
                        if (variantInfo->payloadKind == EnumPayloadKind::Tuple &&
                            variantInfo->payloadFields.size() == 1) {
                            payloadTI = variantInfo->payloadFields[0].typeInfo;
                        }
                        pendingIsBinding_ = PendingIsBinding{
                            bindingName, typedPtr, payloadTI, payloadLLTy
                        };
                    }
                }
            }
        }

        return static_cast<llvm::Value*>(cmpResult);
    }

    auto* targetTy = targetTI->toLLVMType(*context_, module_->getDataLayout());
    bool match = (val->getType() == targetTy);
    return static_cast<llvm::Value*>(
        llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), match ? 1 : 0));
}



std::any IRGen::visitNullCoalExpr(LucisParser::NullCoalExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* ptrVal = castValue(visit(ctx->expression(0)));

    // Check if pointer is null
    auto* isNull = builder_->CreateICmpEQ(
        ptrVal,
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)),
        "isnull");

    auto* thenBB  = llvm::BasicBlock::Create(*context_, "coal.nonnull", currentFunction_);
    auto* elseBB  = llvm::BasicBlock::Create(*context_, "coal.null", currentFunction_);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "coal.end", currentFunction_);

    builder_->CreateCondBr(isNull, elseBB, thenBB);

    // Non-null path: dereference the pointer to get the value
    builder_->SetInsertPoint(thenBB);
    auto* defaultVal = castValue(visit(ctx->expression(1)));
    auto* loadedVal = builder_->CreateLoad(defaultVal->getType(), ptrVal, "coal.load");
    builder_->CreateBr(mergeBB);
    auto* thenEnd = builder_->GetInsertBlock();

    // Null path: use the default value
    builder_->SetInsertPoint(elseBB);
    auto* fallback = castValue(visit(ctx->expression(1)));
    builder_->CreateBr(mergeBB);
    auto* elseEnd = builder_->GetInsertBlock();

    // Merge
    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(defaultVal->getType(), 2, "coal.result");
    phi->addIncoming(loadedVal, thenEnd);
    phi->addIncoming(fallback, elseEnd);

    return static_cast<llvm::Value*>(phi);
}



std::any IRGen::visitRangeExpr(LucisParser::RangeExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Range creates a {start, end} struct
    auto* startVal = castValue(visit(ctx->expression(0)));
    auto* endVal   = castValue(visit(ctx->expression(1)));

    auto* elemTy = startVal->getType();
    auto* rangeTy = llvm::StructType::get(*context_, {elemTy, elemTy});

    llvm::Value* rangeVal = llvm::UndefValue::get(rangeTy);
    rangeVal = builder_->CreateInsertValue(rangeVal, startVal, 0, "range.start");
    rangeVal = builder_->CreateInsertValue(rangeVal, endVal,   1, "range.end");
    return static_cast<llvm::Value*>(rangeVal);
}

std::any IRGen::visitRangeInclExpr(LucisParser::RangeInclExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Inclusive range: same struct as range, semantics differ at use site (for loop)
    auto* startVal = castValue(visit(ctx->expression(0)));
    auto* endVal   = castValue(visit(ctx->expression(1)));

    auto* elemTy = startVal->getType();
    auto* rangeTy = llvm::StructType::get(*context_, {elemTy, elemTy});

    llvm::Value* rangeVal = llvm::UndefValue::get(rangeTy);
    rangeVal = builder_->CreateInsertValue(rangeVal, startVal, 0, "range.start");
    rangeVal = builder_->CreateInsertValue(rangeVal, endVal,   1, "range.end");
    return static_cast<llvm::Value*>(rangeVal);
}

std::any IRGen::visitSpreadExpr(LucisParser::SpreadExprContext* ctx) {
        SET_DBG_LOC(ctx);
    // Spread just evaluates the inner expression — expansion handled at call/array site
    return visit(ctx->expression());
}

std::any IRGen::visitParenExpr(LucisParser::ParenExprContext* ctx) {
        SET_DBG_LOC(ctx);
    return visit(ctx->expression());
}

// ═══════════════════════════════════════════════════════════════════════
//  Tuple expressions
// ═══════════════════════════════════════════════════════════════════════



std::any IRGen::visitTupleLitExpr(LucisParser::TupleLitExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto exprs = ctx->expression();

    // Evaluate each element and collect LLVM values + types
    std::vector<llvm::Value*> vals;
    std::vector<llvm::Type*>  elemTypes;
    for (auto* e : exprs) {
        auto* v = castValue(visit(e));
        vals.push_back(v);
        elemTypes.push_back(v->getType());
    }

    // Build anonymous struct: { T0, T1, ... }
    auto* tupleTy = llvm::StructType::get(*context_, elemTypes);
    llvm::Value* agg = llvm::ConstantAggregateZero::get(tupleTy);
    for (unsigned i = 0; i < vals.size(); i++)
        agg = builder_->CreateInsertValue(agg, vals[i], {i});

    return static_cast<llvm::Value*>(agg);
}

std::any IRGen::visitTupleIndexExpr(LucisParser::TupleIndexExprContext* ctx) {
        SET_DBG_LOC(ctx);
    unsigned idx = std::stoul(ctx->INT_LIT()->getText());

    // Optimized path: base is local variable — use GEP+Load
    if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(
            ctx->expression())) {
        auto varName = identBase->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it != locals_.end() && it->second.typeInfo &&
            it->second.typeInfo->kind == TypeKind::Tuple) {
            auto* alloca  = it->second.alloca;
            auto* tupleTI = it->second.typeInfo;
            auto* elemTI  = tupleTI->tupleElements[idx];
            auto* elemTy  = elemTI->toLLVMType(*context_,
                                                module_->getDataLayout());
            auto* tupleTy = alloca->getAllocatedType();
            auto* gep = builder_->CreateStructGEP(tupleTy, alloca, idx,
                                                   "tup_ptr");
            return static_cast<llvm::Value*>(
                builder_->CreateLoad(elemTy, gep, "tup_elem"));
        }
    }

    // Generic path: evaluate base, extract from value
    auto* tupleVal = castValue(visit(ctx->expression()));
    return static_cast<llvm::Value*>(
        builder_->CreateExtractValue(tupleVal, {idx}, "tup_elem"));
}

std::any IRGen::visitTupleArrowIndexExpr(
        LucisParser::TupleArrowIndexExprContext* ctx) {
        SET_DBG_LOC(ctx);
    unsigned idx = std::stoul(ctx->INT_LIT()->getText());

    auto* ptrVal = castValue(visit(ctx->expression()));
    auto* exprTI = resolveExprTypeInfo(ctx->expression());

    const TypeInfo* tupleTI = nullptr;
    if (exprTI && exprTI->kind == TypeKind::Pointer)
        tupleTI = exprTI->pointeeType;

    if (!tupleTI || tupleTI->kind != TypeKind::Tuple) {
        std::cerr << "lucis: '->N' requires pointer to tuple\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* elemTI  = tupleTI->tupleElements[idx];
    auto* elemTy  = elemTI->toLLVMType(*context_, module_->getDataLayout());
    auto* tupleTy = tupleTI->toLLVMType(*context_, module_->getDataLayout());
    auto* gep = builder_->CreateStructGEP(tupleTy, ptrVal, idx, "tup_arr_ptr");
    return static_cast<llvm::Value*>(
        builder_->CreateLoad(elemTy, gep, "tup_arr_elem"));
}

// Chained tuple dot access: e.g. nested.1.0  (FLOAT_LIT "1.0" → indices 1,0)
std::any IRGen::visitChainedTupleIndexExpr(
        LucisParser::ChainedTupleIndexExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = ctx->FLOAT_LIT()->getText();
    auto dotPos = text.find('.');
    unsigned idx1 = std::stoul(text.substr(0, dotPos));
    unsigned idx2 = std::stoul(text.substr(dotPos + 1));

    // Optimized path: base is local variable — use GEP+Load+ExtractValue
    if (auto* identBase = dynamic_cast<LucisParser::IdentExprContext*>(
            ctx->expression())) {
        auto varName = identBase->IDENTIFIER()->getText();
        auto it = locals_.find(varName);
        if (it != locals_.end() && it->second.typeInfo &&
            it->second.typeInfo->kind == TypeKind::Tuple &&
            idx1 < it->second.typeInfo->tupleElements.size()) {
            auto* alloca  = it->second.alloca;
            auto* tupleTI = it->second.typeInfo;
            auto* innerTI = tupleTI->tupleElements[idx1];
            auto* innerTy = innerTI->toLLVMType(*context_,
                                                 module_->getDataLayout());
            auto* tupleTy = alloca->getAllocatedType();
            auto* gep = builder_->CreateStructGEP(tupleTy, alloca, idx1,
                                                   "tup_ptr");
            auto* innerVal = builder_->CreateLoad(innerTy, gep, "tup_inner");
            return static_cast<llvm::Value*>(
                builder_->CreateExtractValue(innerVal, {idx2}, "tup_chain"));
        }
    }

    // Generic path: evaluate base, extract twice
    auto* tupleVal = castValue(visit(ctx->expression()));
    auto* innerVal = builder_->CreateExtractValue(tupleVal, {idx1}, "tup_inner");
    return static_cast<llvm::Value*>(
        builder_->CreateExtractValue(innerVal, {idx2}, "tup_chain"));
}

// Chained tuple arrow access: e.g. ptr->1.0  (FLOAT_LIT "1.0" → indices 1,0)
std::any IRGen::visitChainedTupleArrowIndexExpr(
        LucisParser::ChainedTupleArrowIndexExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto text = ctx->FLOAT_LIT()->getText();
    auto dotPos = text.find('.');
    unsigned idx1 = std::stoul(text.substr(0, dotPos));
    unsigned idx2 = std::stoul(text.substr(dotPos + 1));

    auto* ptrVal = castValue(visit(ctx->expression()));
    auto* exprTI = resolveExprTypeInfo(ctx->expression());

    const TypeInfo* tupleTI = nullptr;
    if (exprTI && exprTI->kind == TypeKind::Pointer)
        tupleTI = exprTI->pointeeType;

    if (!tupleTI || tupleTI->kind != TypeKind::Tuple) {
        std::cerr << "lucis: '->N.M' requires pointer to tuple\n";
        return static_cast<llvm::Value*>(
            llvm::UndefValue::get(llvm::Type::getInt32Ty(*context_)));
    }

    auto* tupleTy = tupleTI->toLLVMType(*context_, module_->getDataLayout());
    auto* innerTI = tupleTI->tupleElements[idx1];
    auto* innerTy = innerTI->toLLVMType(*context_, module_->getDataLayout());
    auto* gep = builder_->CreateStructGEP(tupleTy, ptrVal, idx1, "tup_arr_ptr");
    auto* innerVal = builder_->CreateLoad(innerTy, gep, "tup_arr_inner");
    return static_cast<llvm::Value*>(
        builder_->CreateExtractValue(innerVal, {idx2}, "tup_arr_chain"));
}



std::any IRGen::visitRelExpr(LucisParser::RelExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
    auto* rhsTI = resolveExprTypeInfo(ctx->expression(1));

    // ── User-defined operator overload ──
    if (lhsTI && lhsTI->kind == TypeKind::Struct) {
        auto opName = opTextToInternal(ctx->op->getText());
        if (!opName.empty()) {
            if (auto* entry = findOperatorEntryInChain(lhsTI, opName)) {
                auto* call = builder_->CreateCall(entry->fn, {lhs, rhs});
                    if (!call->getType()->isVoidTy())
                        call->setName(opName);
                    auto* result = call;
                return static_cast<llvm::Value*>(result);
            }
        }
    }

    // Pointer comparison: ptr < ptr2 → ptrtoint + unsigned icmp
    if (lhs->getType()->isPointerTy() && rhs->getType()->isPointerTy()) {
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* lInt = builder_->CreatePtrToInt(lhs, i64Ty, "l.ptr2int");
        auto* rInt = builder_->CreatePtrToInt(rhs, i64Ty, "r.ptr2int");
        switch (ctx->op->getType()) {
        case LucisLexer::LT:
            return static_cast<llvm::Value*>(builder_->CreateICmpULT(lInt, rInt, "lt"));
        case LucisLexer::GT:
            return static_cast<llvm::Value*>(builder_->CreateICmpUGT(lInt, rInt, "gt"));
        case LucisLexer::LTE:
            return static_cast<llvm::Value*>(builder_->CreateICmpULE(lInt, rInt, "lte"));
        case LucisLexer::GTE:
            return static_cast<llvm::Value*>(builder_->CreateICmpUGE(lInt, rInt, "gte"));
        default: break;
        }
    }

    auto [l, r] = promoteArithmetic(lhs, rhs);
    bool isFloat = l->getType()->isFloatingPointTy();
    bool useSignedIntCmp = true;
    if (!isFloat && l->getType()->isIntegerTy()) {
        // int1 must behave as 0/1 in relational ops.
        if (l->getType()->isIntegerTy(1)) {
            useSignedIntCmp = false;
        } else if (lhsTI && rhsTI
                && lhsTI->kind == TypeKind::Integer
                && rhsTI->kind == TypeKind::Integer) {
            useSignedIntCmp = lhsTI->isSigned || rhsTI->isSigned;
        }
    }

    switch (ctx->op->getType()) {
    case LucisLexer::LT:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpOLT(l, r, "lt")
                    : (useSignedIntCmp
                        ? builder_->CreateICmpSLT(l, r, "lt")
                        : builder_->CreateICmpULT(l, r, "lt")));
    case LucisLexer::GT:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpOGT(l, r, "gt")
                    : (useSignedIntCmp
                        ? builder_->CreateICmpSGT(l, r, "gt")
                        : builder_->CreateICmpUGT(l, r, "gt")));
    case LucisLexer::LTE:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpOLE(l, r, "lte")
                    : (useSignedIntCmp
                        ? builder_->CreateICmpSLE(l, r, "lte")
                        : builder_->CreateICmpULE(l, r, "lte")));
    case LucisLexer::GTE:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpOGE(l, r, "gte")
                    : (useSignedIntCmp
                        ? builder_->CreateICmpSGE(l, r, "gte")
                        : builder_->CreateICmpUGE(l, r, "gte")));
    default:
        return static_cast<llvm::Value*>(llvm::UndefValue::get(l->getType()));
    }
}



std::any IRGen::visitEqExpr(LucisParser::EqExprContext* ctx) {
        SET_DBG_LOC(ctx);
    auto* lhs = castValue(visit(ctx->expression(0)));
    auto* rhs = castValue(visit(ctx->expression(1)));
    auto* lhsTI = resolveExprTypeInfo(ctx->expression(0));
    auto* rhsTI = resolveExprTypeInfo(ctx->expression(1));

    // ── User-defined operator overload ──
    if (lhsTI && lhsTI->kind == TypeKind::Struct) {
        auto opName = opTextToInternal(ctx->op->getText());
        if (!opName.empty()) {
            if (auto* entry = findOperatorEntryInChain(lhsTI, opName)) {
                auto* call = builder_->CreateCall(entry->fn, {lhs, rhs});
                    if (!call->getType()->isVoidTy())
                        call->setName(opName);
                    auto* result = call;
                return static_cast<llvm::Value*>(result);
            }
        }
    }

    // String comparison: delegate to lucis_compareTo(ptr,len,ptr,len)
    if (lhsTI && rhsTI && lhsTI->kind == TypeKind::String && rhsTI->kind == TypeKind::String) {
        auto* ptrTy  = llvm::PointerType::getUnqual(*context_);
        auto* usizeTy = module_->getDataLayout().getIntPtrType(*context_);
        auto* i32Ty  = llvm::Type::getInt32Ty(*context_);

        auto* lPtr = builder_->CreateExtractValue(lhs, 0, "lhs_ptr");
        auto* lLen = builder_->CreateExtractValue(lhs, 1, "lhs_len");
        auto* rPtr = builder_->CreateExtractValue(rhs, 0, "rhs_ptr");
        auto* rLen = builder_->CreateExtractValue(rhs, 1, "rhs_len");

        auto callee = declareBuiltin("lucis_compareTo", i32Ty,
                                     {ptrTy, usizeTy, ptrTy, usizeTy});
        auto* cmpResult = builder_->CreateCall(callee,
                                               {lPtr, lLen, rPtr, rLen}, "strcmp");

        auto* zero = llvm::ConstantInt::get(i32Ty, 0);
        switch (ctx->op->getType()) {
        case LucisLexer::EQ:
            return static_cast<llvm::Value*>(
                builder_->CreateICmpEQ(cmpResult, zero, "streq"));
        case LucisLexer::NEQ:
            return static_cast<llvm::Value*>(
                builder_->CreateICmpNE(cmpResult, zero, "strneq"));
        default:
            return static_cast<llvm::Value*>(
                llvm::UndefValue::get(llvm::Type::getInt1Ty(*context_)));
        }
    }

    // Enum comparison: compare discriminants (tag)
    if (lhsTI && rhsTI && lhsTI->kind == TypeKind::Enum && rhsTI->kind == TypeKind::Enum) {
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);

        auto extractTag = [&](llvm::Value* v) -> llvm::Value* {
            llvm::Value* tag = nullptr;
            if (auto* st = llvm::dyn_cast<llvm::StructType>(v->getType())) {
                if (st->getNumElements() > 0)
                    tag = builder_->CreateExtractValue(v, {0}, "enum.tag");
            } else if (v->getType()->isIntegerTy()) {
                tag = v;
            }
            if (!tag) return nullptr;
            if (tag->getType() != i32Ty) {
                if (tag->getType()->isIntegerTy(1))
                    tag = builder_->CreateZExt(tag, i32Ty, "enum.tag.zext");
                else if (tag->getType()->isIntegerTy())
                    tag = builder_->CreateIntCast(tag, i32Ty, false, "enum.tag.cast");
                else
                    return nullptr;
            }
            return tag;
        };

        auto* ltag = extractTag(lhs);
        auto* rtag = extractTag(rhs);
        if (!ltag || !rtag)
            return static_cast<llvm::Value*>(llvm::UndefValue::get(llvm::Type::getInt1Ty(*context_)));

        if (ctx->op->getType() == LucisLexer::EQ)
            return static_cast<llvm::Value*>(builder_->CreateICmpEQ(ltag, rtag, "enum_eq"));
        if (ctx->op->getType() == LucisLexer::NEQ)
            return static_cast<llvm::Value*>(builder_->CreateICmpNE(ltag, rtag, "enum_neq"));
    }

    // Array comparison: element-by-element
    if (lhs->getType()->isArrayTy() && rhs->getType()->isArrayTy()) {
        auto* lhsArrTy = llvm::cast<llvm::ArrayType>(lhs->getType());
        auto* rhsArrTy = llvm::cast<llvm::ArrayType>(rhs->getType());
        if (lhsArrTy->getNumElements() == rhsArrTy->getNumElements() &&
            lhsArrTy->getElementType() == rhsArrTy->getElementType()) {
            auto* boolTy = llvm::Type::getInt1Ty(*context_);
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            uint64_t len = lhsArrTy->getNumElements();
            auto* elemTy = lhsArrTy->getElementType();

            auto* resultAlloca = builder_->CreateAlloca(boolTy, nullptr, "arr_eq_res");
            builder_->CreateStore(llvm::ConstantInt::get(boolTy, 1), resultAlloca);

            for (uint64_t i = 0; i < len; i++) {
                auto* idx = llvm::ConstantInt::get(i64Ty, i);
                auto* lv = builder_->CreateExtractValue(lhs, {i}, "lhs_el");
                auto* rv = builder_->CreateExtractValue(rhs, {i}, "rhs_el");
                llvm::Value* elemEq;
                if (elemTy->isIntegerTy())
                    elemEq = builder_->CreateICmpEQ(lv, rv, "elem_eq");
                else if (elemTy->isFloatingPointTy())
                    elemEq = builder_->CreateFCmpOEQ(lv, rv, "elem_eq");
                else
                    elemEq = builder_->CreateICmpEQ(lv, rv, "elem_eq");
                auto* cur = builder_->CreateLoad(boolTy, resultAlloca, "cur");
                builder_->CreateStore(builder_->CreateAnd(cur, elemEq, "and"), resultAlloca);
            }

            llvm::Value* result = builder_->CreateLoad(boolTy, resultAlloca, "arr_eq");
            if (ctx->op->getType() == LucisLexer::NEQ)
                result = builder_->CreateNot(result, "arr_neq");
            return static_cast<llvm::Value*>(result);
        }
        // Arrays of different sizes: never equal
        if (ctx->op->getType() == LucisLexer::EQ)
            return static_cast<llvm::Value*>(llvm::ConstantInt::getFalse(*context_));
        else
            return static_cast<llvm::Value*>(llvm::ConstantInt::getTrue(*context_));
    }

    // Slice comparison: compare lengths, then data via memcmp
    if (lhs->getType()->isStructTy() && rhs->getType()->isStructTy()) {
        auto* lhsST = llvm::dyn_cast<llvm::StructType>(lhs->getType());
        auto* rhsST = llvm::dyn_cast<llvm::StructType>(rhs->getType());
        if (lhsST && rhsST && lhsST->getNumElements() == 2 && rhsST->getNumElements() == 2 &&
            lhsST->getElementType(0)->isPointerTy() && rhsST->getElementType(0)->isPointerTy() &&
            lhsST->getElementType(1)->isIntegerTy() && rhsST->getElementType(1)->isIntegerTy() &&
            !(lhsTI && lhsTI->kind == TypeKind::String)) {
            auto* i32Ty = llvm::Type::getInt32Ty(*context_);
            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
            auto* ptrTy = llvm::PointerType::getUnqual(*context_);
            auto* boolTy = llvm::Type::getInt1Ty(*context_);

            auto* lPtr = builder_->CreateExtractValue(lhs, 0, "sl_ptr");
            auto* lLen = builder_->CreateExtractValue(lhs, 1, "sl_len");
            auto* rPtr = builder_->CreateExtractValue(rhs, 0, "sr_ptr");
            auto* rLen = builder_->CreateExtractValue(rhs, 1, "sr_len");

            // Compare lengths
            auto* lenEq = builder_->CreateICmpEQ(lLen, rLen, "sl_len_eq");

            // Compute element size from TypeInfo
            uint64_t elemSizeBytes = 1;
            if (lhsTI) {
                auto* elemLLTy = lhsTI->toLLVMType(*context_, module_->getDataLayout());
                elemSizeBytes = module_->getDataLayout().getTypeAllocSize(elemLLTy);
            }
            auto* elemSizeVal = llvm::ConstantInt::get(i64Ty, elemSizeBytes);

            // byteLen = lLen * elemSize
            auto* byteLen = builder_->CreateMul(lLen, elemSizeVal, "sl_bytes");

            // memcmp(lPtr, rPtr, byteLen) == 0
            llvm::SmallVector<llvm::Type*, 3> memcmpArgs = {ptrTy, ptrTy, i64Ty};
            auto memcmpFn = declareBuiltin("memcmp", i32Ty, memcmpArgs);
            auto* cmpResult = builder_->CreateCall(memcmpFn, {lPtr, rPtr, byteLen}, "sl_memcmp");
            auto* zero = llvm::ConstantInt::get(i32Ty, 0);
            auto* dataEq = builder_->CreateICmpEQ(cmpResult, zero, "sl_data_eq");

            // Both conditions must be true
            auto* result = builder_->CreateAnd(lenEq, dataEq, "slice_eq");
            if (ctx->op->getType() == LucisLexer::NEQ)
                result = builder_->CreateNot(result, "slice_neq");
            return static_cast<llvm::Value*>(result);
        }
    }

    // Generic struct equality fallback: compare first integer field (enum-like discriminant)
    if (lhs->getType()->isStructTy() && rhs->getType()->isStructTy()) {
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);

        auto extractFirstInt = [&](llvm::Value* v, const char* tagName) -> llvm::Value* {
            auto* st = llvm::dyn_cast<llvm::StructType>(v->getType());
            if (!st || st->getNumElements() == 0)
                return nullptr;
            auto* first = builder_->CreateExtractValue(v, {0}, tagName);
            if (!first->getType()->isIntegerTy())
                return nullptr;
            if (first->getType() != i32Ty) {
                if (first->getType()->isIntegerTy(1))
                    first = builder_->CreateZExt(first, i32Ty, "agg.tag.zext");
                else
                    first = builder_->CreateIntCast(first, i32Ty, false, "agg.tag.cast");
            }
            return first;
        };

        auto* ltag = extractFirstInt(lhs, "lhs_tag");
        auto* rtag = extractFirstInt(rhs, "rhs_tag");
        if (ltag && rtag) {
            if (ctx->op->getType() == LucisLexer::EQ)
                return static_cast<llvm::Value*>(builder_->CreateICmpEQ(ltag, rtag, "agg_eq"));
            if (ctx->op->getType() == LucisLexer::NEQ)
                return static_cast<llvm::Value*>(builder_->CreateICmpNE(ltag, rtag, "agg_neq"));
        }

        // Unsupported aggregate equality form: deterministic false/true fallback.
        if (ctx->op->getType() == LucisLexer::EQ)
            return static_cast<llvm::Value*>(llvm::ConstantInt::getFalse(*context_));
        if (ctx->op->getType() == LucisLexer::NEQ)
            return static_cast<llvm::Value*>(llvm::ConstantInt::getTrue(*context_));
    }

    auto [l, r] = promoteArithmetic(lhs, rhs);
    bool isFloat = l->getType()->isFloatingPointTy();

    switch (ctx->op->getType()) {
    case LucisLexer::EQ:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpOEQ(l, r, "eq")
                    : builder_->CreateICmpEQ(l, r, "eq"));
    case LucisLexer::NEQ:
        return static_cast<llvm::Value*>(
            isFloat ? builder_->CreateFCmpONE(l, r, "neq")
                    : builder_->CreateICmpNE(l, r, "neq"));
    default:
        return static_cast<llvm::Value*>(llvm::UndefValue::get(l->getType()));
    }
}

// ── Helpers ─────────────────────────────────────────────────────────────────



llvm::Value* IRGen::toBool(llvm::Value* value,
                           const TypeInfo* valueTI,
                           const std::string& name) {
    if (value->getType()->isIntegerTy(1))
        return value;

    if (valueTI && valueTI->kind == TypeKind::String) {
        // Strings are represented as fat pointers {ptr, len}; truthiness is based on length.
        auto* len = builder_->CreateExtractValue(value, 1, name + ".len");
        return builder_->CreateICmpNE(len,
            llvm::ConstantInt::get(len->getType(), 0), name);
    }

    if (value->getType()->isPointerTy()) {
        auto* ptrTy = llvm::cast<llvm::PointerType>(value->getType());
        return builder_->CreateICmpNE(value,
            llvm::ConstantPointerNull::get(ptrTy), name);
    }

    if (value->getType()->isIntegerTy()) {
        return builder_->CreateICmpNE(value,
            llvm::ConstantInt::get(value->getType(), 0), name);
    }

    if (value->getType()->isFloatingPointTy()) {
        return builder_->CreateFCmpONE(value,
            llvm::ConstantFP::get(value->getType(), 0.0), name);
    }

    // Fallback: preserve existing behavior for unexpected non-bool types.
    return value;
}

// ═══════════════════════════════════════════════════════════════════════
//  User-defined generics — monomorphization (IRGen side)
// ═══════════════════════════════════════════════════════════════════════


