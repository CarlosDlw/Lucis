#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <llvm/IR/IRBuilder.h>
struct TypeInfo;
class TypeRegistry;

struct IntrinsicParam {
    std::string type;        // Lux type name: "void", "int32", "string", "_any"
    bool isVariadic = false; // future: start of variadic region
};

struct IntrinsicFunction {
    std::string name;
    std::string returnType;
    std::vector<IntrinsicParam> params;
    bool returnsOwned = false;
    std::vector<size_t> consumingArgs;
    std::vector<size_t> borrowedArgs;
    std::string description;
    bool isGeneric = false;

    struct Lowering {
        enum Kind { LLVMIntrinsic, BuiltinCall, InlineIR };
        Kind kind = LLVMIntrinsic;
        std::string intrinsicName; // "llvm.trap"
        std::function<llvm::Value*(
            llvm::IRBuilder<>&,
            llvm::Module*,
            llvm::LLVMContext&,
            const TypeRegistry&,
            const std::vector<llvm::Value*>&,
            const std::vector<const TypeInfo*>&
        )> emitIR;
    };
    Lowering lowering;
};

struct IntrinsicNamespace {
    std::string name;
    std::string description;
    std::vector<IntrinsicFunction> functions;
};

class IntrinsicRegistry {
public:
    IntrinsicRegistry(TypeRegistry& typeRegistry);

    const IntrinsicFunction* lookup(const std::string& ns,
                                    const std::string& funcName) const;

    bool hasNamespace(const std::string& ns) const;

    std::string namespaceDescription(const std::string& ns) const;

    std::vector<const IntrinsicFunction*>
    functionsInNamespace(const std::string& ns) const;

    std::vector<std::string> allNamespaces() const;

    static bool isIntrinsicPrefix(const std::string& firstSegment);

    static bool parseIntrinsicPath(
        const std::vector<std::string>& ids,
        std::string& outNs,
        std::string& outFunc);

    void registerNamespace(IntrinsicNamespace ns);

private:
    std::unordered_map<std::string, IntrinsicNamespace> namespaces_;
    std::unordered_map<std::string, const IntrinsicFunction*> flatLookup_;
};

// Per-namespace registration functions (defined in their own .cpp files)
void registerCoreNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
void registerDebugNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
void registerUnsafeNamespace(IntrinsicRegistry& reg, TypeRegistry& typeReg);
