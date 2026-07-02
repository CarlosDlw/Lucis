#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "types/TypeInfo.h"

// Describes a C function extracted from a parsed header.
struct CFunction {
    std::string                name;
    const TypeInfo*            returnType;
    std::vector<const TypeInfo*> paramTypes;
    std::vector<std::string>   paramNames;  // Parallel to paramTypes
    bool                       isVariadic = false;
    std::string                sourceFile; // Absolute path of defining header
    unsigned                   line = 0;   // 0-based line in sourceFile
    std::string                doc;        // Raw comment cleaned up for display
};

// Describes a C struct extracted from a parsed header.
struct CStruct {
    std::string              name;
    std::vector<FieldInfo>   fields;
    unsigned                 byteSize = 0; // >0 for opaque types (unions, bitfield structs)
    std::string              sourceFile;
    unsigned                 line = 0;
};

// Describes a C enum extracted from a parsed header.
struct CEnum {
    std::string                               name;
    std::vector<std::pair<std::string, int64_t>> values;
    std::string                               sourceFile;
    unsigned                                  line = 0;
    // Per-constant locations: { constantName → {absolutePath, 0-based line} }
    std::unordered_map<std::string, std::pair<std::string, unsigned>> valueLocs;
};

// Describes a C typedef extracted from a parsed header.
struct CTypedef {
    std::string     name;
    const TypeInfo* underlying;
    std::string     sourceFile;
    unsigned        line = 0;
};

// Describes a simple #define constant extracted from a parsed header.
struct CMacro {
    std::string name;
    int64_t     value = 0;         // integer value (when isFloat/isString are false)
    double      floatValue = 0.0;  // float value (when isFloat is true)
    std::string stringValue;       // string value (when isString is true)
    bool        isNull   = false;  // true for NULL-like pointer constants
    bool        isFloat  = false;  // true for floating-point constants
    bool        isString = false;  // true for string literal constants
    std::string sourceFile;
    unsigned    line = 0;
};

// Describes a #define macro that expands to a struct compound literal.
// Example: #define RAYWHITE CLITERAL(Color){ 245, 245, 245, 255 }
struct CStructMacro {
    std::string              name;
    std::string              structType;     // e.g. "Color"
    std::vector<int64_t>     fieldValues;    // e.g. {245, 245, 245, 255}
    std::string              sourceFile;
    unsigned                 line = 0;
};

// Describes a C global variable (extern) extracted from a parsed header.
struct CGlobalVar {
    std::string     name;
    const TypeInfo* type;
    std::string     sourceFile;
    unsigned        line = 0;
};

// Describes a function-like macro: #define FOO(x, y) body...
struct CFunctionLikeMacro {
    std::string              name;
    std::vector<std::string> paramNames;
    std::vector<std::string> bodyTokens;  // tokenized macro body
    std::string              sourceFile;
    unsigned                 line = 0;
};

// Registry of all C symbols imported via #include directives.
// Populated by CHeaderResolver, queried by Checker and IRGen.
class CBindings {
public:
    void addFunction(CFunction func);
    void addStruct(CStruct s);
    void addEnum(CEnum e);
    void addTypedef(CTypedef t);
    void addMacro(CMacro m);
    void addStructMacro(CStructMacro m);
    void addFunctionLikeMacro(CFunctionLikeMacro m);
    void addGlobal(CGlobalVar g);

    // Update the source location of a previously-registered struct
    // (used when location is only known after mapStruct returns).
    void setStructLocation(const std::string& name,
                           const std::string& file, unsigned line);

    // Store / retrieve the absolute path for a parsed header by its include name.
    void addHeaderPath(const std::string& headerName,
                       const std::string& absolutePath);
    std::string resolveHeaderPath(const std::string& headerName) const;

    // Track required linker libraries detected from #include headers
    void addRequiredLib(const std::string& flag, const std::string& header);
    const std::vector<std::pair<std::string, std::string>>& requiredLibs() const {
        return requiredLibs_;
    }

    const CFunction*   findFunction(const std::string& name) const;
    const CStruct*     findStruct(const std::string& name) const;
    const CEnum*       findEnum(const std::string& name) const;
    const CTypedef*    findTypedef(const std::string& name) const;
    const CMacro*      findMacro(const std::string& name) const;
    const CStructMacro* findStructMacro(const std::string& name) const;
    const CGlobalVar*  findGlobal(const std::string& name) const;
    const CFunctionLikeMacro* findFunctionLikeMacro(const std::string& name) const;

    bool hasSymbol(const std::string& name) const;

    const std::unordered_map<std::string, CFunction>& functions() const {
        return functions_;
    }
    const std::unordered_map<std::string, CStruct>& structs() const {
        return structs_;
    }
    const std::unordered_map<std::string, CEnum>& enums() const {
        return enums_;
    }
    const std::unordered_map<std::string, CTypedef>& typedefs() const {
        return typedefs_;
    }
    const std::unordered_map<std::string, CMacro>& macros() const {
        return macros_;
    }
    const std::unordered_map<std::string, CStructMacro>& structMacros() const {
        return structMacros_;
    }
    const std::unordered_map<std::string, CGlobalVar>& globals() const {
        return globals_;
    }
    const std::unordered_map<std::string, CFunctionLikeMacro>& functionLikeMacros() const {
        return functionLikeMacros_;
    }

    // Owns dynamically created TypeInfo objects (for pointer types, etc.)
    const TypeInfo* internType(std::unique_ptr<TypeInfo> ti);

private:
    std::unordered_map<std::string, CFunction>    functions_;
    std::unordered_map<std::string, CStruct>      structs_;
    std::unordered_map<std::string, CEnum>        enums_;
    std::unordered_map<std::string, CTypedef>     typedefs_;
    std::unordered_map<std::string, CMacro>       macros_;
    std::unordered_map<std::string, CStructMacro> structMacros_;
    std::unordered_map<std::string, CGlobalVar>   globals_;
    std::unordered_map<std::string, CFunctionLikeMacro> functionLikeMacros_;

    // Maps include name (e.g. "raylib.h") to its absolute resolved path.
    std::unordered_map<std::string, std::string>  headerPaths_;

    std::vector<std::unique_ptr<TypeInfo>> ownedTypes_;

    // Linker flags detected from included headers: {flag, header}
    std::vector<std::pair<std::string, std::string>> requiredLibs_;
};
