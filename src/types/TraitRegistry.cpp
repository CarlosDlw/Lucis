#include "types/TraitRegistry.h"

TraitRegistry::TraitRegistry()
{
    registerBuiltinTraits();
}

void TraitRegistry::registerTrait(TraitInfo info)
{
    traits_[info.name] = std::move(info);
}

const TraitInfo* TraitRegistry::lookup(const std::string& name) const
{
    auto it = traits_.find(name);
    return it != traits_.end() ? &it->second : nullptr;
}

bool TraitRegistry::isTrait(const std::string& name) const
{
    return traits_.count(name) > 0;
}

std::vector<std::string> TraitRegistry::allTraitNames() const
{
    std::vector<std::string> names;
    names.reserve(traits_.size());
    for (const auto& [name, _] : traits_) names.push_back(name);
    return names;
}

void TraitRegistry::registerImpl(ImplInfo info)
{
    impls_[info.traitName][info.typeName] = std::move(info);
}

const ImplInfo* TraitRegistry::findImpl(const std::string& traitName,
                                          const std::string& typeName) const
{
    auto traitIt = impls_.find(traitName);
    if (traitIt == impls_.end()) return nullptr;
    auto typeIt = traitIt->second.find(typeName);
    return typeIt != traitIt->second.end() ? &typeIt->second : nullptr;
}

bool TraitRegistry::typeSatisfiesTrait(const TypeInfo* type,
                                         const TraitInfo* trait) const
{
    if (!trait || !type) return true;

    if (trait->isBuiltin && trait->builtinValidator)
        return trait->builtinValidator(type);

    return findImpl(trait->name, type->name) != nullptr;
}

void TraitRegistry::registerBuiltinTraits()
{
    // numeric — integer or float
    {
        TraitInfo t;
        t.name = "numeric";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Integer ||
                   ti->kind == TypeKind::Float;
        };
        registerTrait(std::move(t));
    }

    // integer — any integer type
    {
        TraitInfo t;
        t.name = "integer";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Integer;
        };
        registerTrait(std::move(t));
    }

    // float — any floating-point type
    {
        TraitInfo t;
        t.name = "float";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Float;
        };
        registerTrait(std::move(t));
    }

    // signed — signed integers only
    {
        TraitInfo t;
        t.name = "signed";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Integer && ti->isSigned;
        };
        registerTrait(std::move(t));
    }

    // unsigned — unsigned integers only
    {
        TraitInfo t;
        t.name = "unsigned";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Integer && !ti->isSigned;
        };
        registerTrait(std::move(t));
    }

    // bool
    {
        TraitInfo t;
        t.name = "bool";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::Bool;
        };
        registerTrait(std::move(t));
    }

    // string
    {
        TraitInfo t;
        t.name = "string";
        t.isBuiltin = true;
        t.builtinValidator = [](const TypeInfo* ti) {
            return ti->kind == TypeKind::String;
        };
        registerTrait(std::move(t));
    }
}
