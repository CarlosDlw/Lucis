#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declaration for raw parse-tree access in complex attributes.
namespace antlr4 { class ParserRuleContext; }

struct AttributeArg {
    enum Kind { Ident, String, Int, Float, Bool };
    Kind kind;
    std::string identValue;
    std::string stringValue;
    int64_t intValue;
    double floatValue;
};

struct Attribute {
    std::string name;
    std::vector<AttributeArg> args;
    int line = 0;
    int col = 0;

    // Points to the parse-tree AttributeContext node.
    // Used by complex attributes (e.g., #[cfg(...)]) that need to
    // inspect structured arguments beyond the flat args list above.
    antlr4::ParserRuleContext* rawCtx = nullptr;
};
