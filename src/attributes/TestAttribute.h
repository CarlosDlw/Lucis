#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "generated/LucisParser.h"
#include "attributes/Attribute.h"

// Parsed form of a #[test(...)] attribute.
struct TestSpec {
    std::string displayName;   // empty -> use the function name
    bool skip = false;
    std::string skipReason;    // empty when skip carries no reason
    bool expectedFail = false;
    int64_t timeoutMs = 0;     // 0 -> use the global CLI default
};

// A test case collected by the checker and carried through to IRGen.
struct TestCase {
    std::string modulePath;
    std::string funcName;
    std::string symbolName;    // resolved emit name (filled by LucisPipeline)
    TestSpec spec;
    std::string filePath;
    int line = 0;
    int col = 0;
    bool returnsVoid = false;  // void -> no return value on the LLVM type
};

// Rebuild the flat Attribute from a parse-tree attribute node (shared with
// Checker::validateAttributeList). Returns false when the node has no name.
bool populateAttribute(LucisParser::AttributeContext* ctx, Attribute& out);

// Find the first attribute with the given name in an attribute list.
LucisParser::AttributeContext* findAttribute(
    LucisParser::AttributeListContext* attrs, const std::string& name);

// Parse and validate a #[test(...)] attribute into a TestSpec.
// Returns false and appends user-facing errors on malformed usage.
bool parseTestSpec(const Attribute& attr, TestSpec& out, std::vector<std::string>& errors);