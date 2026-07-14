#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct AttributeArg {
    enum Kind { Ident, String, Int, Float };
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
};
