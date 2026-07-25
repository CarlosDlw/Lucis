#pragma once

#include "types/TypeInfo.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace antlr4 {
class ParserRuleContext;
}

struct TraitMethodInfo {
    std::string name;
    const TypeInfo* returnType = nullptr;
    std::vector<const TypeInfo*> paramTypes;  // excludes &self
    bool isStatic = false;
    bool hasSelfReceiver = false;
};

struct TraitInfo {
    std::string name;
    std::string modulePath;
    std::vector<std::string> typeParams;
    std::vector<TraitMethodInfo> methods;
    antlr4::ParserRuleContext* decl = nullptr;
    unsigned line = 0;
    unsigned column = 0;
    std::string sourceFile;

    bool isBuiltin = false;
    std::function<bool(const TypeInfo*)> builtinValidator;
};

struct ImplInfo {
    std::string traitName;
    std::string typeName;
    std::vector<const TypeInfo*> typeArgs;
    std::vector<std::string> implTypeParams;
    std::vector<TraitMethodInfo> methodBodies;
    antlr4::ParserRuleContext* decl = nullptr;
    std::shared_ptr<void> treeAnchor;
    std::string modulePath;
    std::string sourceFile;
};

class TraitRegistry {
public:
    TraitRegistry();

    void registerTrait(TraitInfo info);
    const TraitInfo* lookup(const std::string& name) const;
    bool isTrait(const std::string& name) const;
    std::vector<std::string> allTraitNames() const;

    void registerImpl(ImplInfo info);
    const ImplInfo* findImpl(const std::string& traitName,
                             const std::string& typeName) const;

    bool typeSatisfiesTrait(const TypeInfo* type,
                            const TraitInfo* trait) const;

private:
    std::unordered_map<std::string, TraitInfo> traits_;
    std::unordered_map<std::string,
        std::unordered_map<std::string, ImplInfo>> impls_;

    void registerBuiltinTraits();
};
