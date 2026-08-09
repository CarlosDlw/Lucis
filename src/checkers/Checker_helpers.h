// Shared file-local helpers for the Checker translation units.
// Extracted from the monolithic Checker.cpp so each TU compiles independently.
// All functions are inline (header-only); no TU-local state.
#pragma once

#include "checkers/Checker.h"

#include <string>
#include <vector>

inline const EnumVariantInfo* findEnumVariantInfo(const TypeInfo* enumType,
                                                  const std::string& variantName) {
    if (!enumType) return nullptr;

    for (const auto& info : enumType->enumVariantInfos) {
        if (info.name == variantName) return &info;
    }

    return nullptr;
}

inline std::string formatParamTypes(const std::vector<const TypeInfo*>& types) {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); i++) {
        if (i > 0) result += ", ";
        result += types[i] ? types[i]->name : "?";
    }
    result += ")";
    return result;
}

inline std::string formatParamTypes(const std::vector<std::string>& types) {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); i++) {
        if (i > 0) result += ", ";
        result += types[i];
    }
    result += ")";
    return result;
}

inline std::string opInternalName(LucisParser::OperatorNameContext* op) {
    if (op->PLUS())    return "opAdd";
    if (op->MINUS())   return "opSub";
    if (op->STAR())    return "opMul";
    if (op->SLASH())   return "opDiv";
    if (op->PERCENT()) return "opMod";
    if (op->EQ())      return "opEq";
    if (op->NEQ())     return "opNe";
    if (op->LT())      return "opLt";
    if (op->GT().size() == 2) return "opShr";
    if (op->GT().size() == 1) return "opGt";
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

inline std::string opSymbol(LucisParser::OperatorNameContext* op) {
    if (op->PLUS())    return "+";
    if (op->MINUS())   return "-";
    if (op->STAR())    return "*";
    if (op->SLASH())   return "/";
    if (op->PERCENT()) return "%";
    if (op->EQ())      return "==";
    if (op->NEQ())     return "!=";
    if (op->LT())      return "<";
    if (op->GT().size() == 2) return ">>";
    if (op->GT().size() == 1) return ">";
    if (op->LTE())     return "<=";
    if (op->GTE())     return ">=";
    if (op->AMPERSAND()) return "&";
    if (op->PIPE())    return "|";
    if (op->CARET())   return "^";
    if (op->LSHIFT())  return "<<";
    if (op->LAND())    return "&&";
    if (op->LOR())     return "||";
    if (op->NOT())     return "!";
    if (op->TILDE())   return "~";
    if (op->INCR())    return "++";
    if (op->DECR())    return "--";
    if (op->LBRACKET() && op->RBRACKET()) return "[]";
    if (op->LPAREN()  && op->RPAREN())    return "()";
    return "?";
}
