#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Extended data structures for helpc display.
// These carry more information than CBindings (param names, docs, sizes).

struct HelpCParam {
    std::string name;
    std::string cType;     // raw C type spelling (e.g. "const char *")
    std::string lucisType;     // Lucis language equivalent (e.g. "*char")
};

struct HelpCFunction {
    std::string name;
    std::string returnCType;
    std::string returnLucisType;
    std::vector<HelpCParam> params;
    bool isVariadic = false;
    std::string doc;       // brief doc comment (if any)
};

struct HelpCField {
    std::string name;
    std::string cType;
    std::string lucisType;
    int64_t     offset = -1;  // byte offset (-1 = unknown)
    int64_t     size   = -1;  // byte size (-1 = unknown)
};

struct HelpCStruct {
    std::string name;
    std::vector<HelpCField> fields;
    int64_t     size      = -1;
    int64_t     alignment = -1;
    std::string doc;
    bool        isUnion   = false;
};

struct HelpCEnumValue {
    std::string name;
    int64_t     value;
};

struct HelpCEnum {
    std::string name;
    std::vector<HelpCEnumValue> values;
    std::string doc;
};

struct HelpCTypedef {
    std::string name;
    std::string underlyingCType;
    std::string underlyingLucisType;
    std::string doc;
};

struct HelpCMacro {
    std::string name;
    int64_t     value;
    bool        isNull = false;
};

struct HelpCStructMacro {
    std::string name;
    std::string structType;
    std::vector<int64_t> fieldValues;
};

struct HelpCGlobal {
    std::string name;
    std::string cType;
    std::string lucisType;
    std::string doc;
};

// All symbols parsed from a single header.
struct HelpCHeaderInfo {
    std::string headerName;
    std::string linkFlag;   // e.g. "-lraylib"

    std::vector<HelpCFunction>    functions;
    std::vector<HelpCStruct>      structs;
    std::vector<HelpCEnum>        enums;
    std::vector<HelpCTypedef>     typedefs;
    std::vector<HelpCMacro>       macros;
    std::vector<HelpCStructMacro> structMacros;
    std::vector<HelpCGlobal>      globals;
};
