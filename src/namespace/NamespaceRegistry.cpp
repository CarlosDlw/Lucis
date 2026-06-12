#include "namespace/NamespaceRegistry.h"

#include <algorithm>
#include <unordered_set>

// ── Registration ────────────────────────────────────────────────────────────

void NamespaceRegistry::registerFile(const std::string& namespaceName,
                                      const std::string& filePath,
                                      LucisParser::ProgramContext* tree) {
    auto& symbols = namespaces_[namespaceName];

    for (auto* topLevel : tree->topLevelDecl()) {
        if (auto* funcDecl = topLevel->functionDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::Function;
            sym.name          = funcDecl->IDENTIFIER(0)->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = funcDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* structDecl = topLevel->structDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::Struct;
            sym.name          = structDecl->IDENTIFIER()->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = structDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* unionDecl = topLevel->unionDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::Union;
            sym.name          = unionDecl->IDENTIFIER()->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = unionDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* enumDecl = topLevel->enumDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::Enum;
            sym.name          = enumDecl->IDENTIFIER()->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = enumDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* typeAlias = topLevel->typeAliasDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::TypeAlias;
            sym.name          = typeAlias->IDENTIFIER()->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = typeAlias;
            symbols.push_back(std::move(sym));
        }
        else if (auto* extDecl = topLevel->extendDecl()) {
            ExportedSymbol sym;
            sym.kind          = ExportedSymbol::ExtendBlock;
            sym.name          = extDecl->IDENTIFIER()->getText();
            sym.namespaceName = namespaceName;
            sym.sourceFile    = filePath;
            sym.decl          = extDecl;
            symbols.push_back(std::move(sym));
        }
    }
}

// ── Validation ──────────────────────────────────────────────────────────────

std::vector<std::string> NamespaceRegistry::validate() const {
    std::vector<std::string> errors;

    for (auto& [ns, symbols] : namespaces_) {
        // Track seen names per kind within this namespace.
        // ExtendBlocks are excluded — multiple extend blocks for the same
        // struct are valid (they add different methods).
        std::unordered_map<std::string, const ExportedSymbol*> seen;

        for (auto& sym : symbols) {
            if (sym.kind == ExportedSymbol::ExtendBlock) continue;

            auto it = seen.find(sym.name);
            if (it != seen.end()) {
                errors.push_back(
                    "duplicate symbol '" + sym.name +
                    "' in namespace '" + ns +
                    "' (defined in '" + it->second->sourceFile +
                    "' and '" + sym.sourceFile + "')");
            } else {
                seen[sym.name] = &sym;
            }
        }
    }

    return errors;
}

// ── Lookup ──────────────────────────────────────────────────────────────────

const ExportedSymbol* NamespaceRegistry::findSymbol(
    const std::string& ns, const std::string& name) const {

    auto nsIt = namespaces_.find(ns);
    if (nsIt == namespaces_.end()) return nullptr;

    for (auto& sym : nsIt->second) {
        if (sym.name == name && sym.kind != ExportedSymbol::ExtendBlock)
            return &sym;
    }
    return nullptr;
}

std::vector<const ExportedSymbol*> NamespaceRegistry::getNamespaceSymbols(
    const std::string& ns) const {

    std::vector<const ExportedSymbol*> result;
    auto nsIt = namespaces_.find(ns);
    if (nsIt == namespaces_.end()) return result;

    for (auto& sym : nsIt->second)
        result.push_back(&sym);
    return result;
}

std::vector<const ExportedSymbol*> NamespaceRegistry::getExternalSymbols(
    const std::string& ns, const std::string& excludeFile) const {

    std::vector<const ExportedSymbol*> result;
    auto nsIt = namespaces_.find(ns);
    if (nsIt == namespaces_.end()) return result;

    for (auto& sym : nsIt->second) {
        if (sym.sourceFile != excludeFile)
            result.push_back(&sym);
    }
    return result;
}

bool NamespaceRegistry::hasNamespace(const std::string& ns) const {
    return namespaces_.count(ns) > 0;
}

bool NamespaceRegistry::isStdModule(const std::string& modulePath) {
    // Standard library modules all start with "std::"
    return modulePath.rfind("std::", 0) == 0;
}

std::string NamespaceRegistry::mangle(const std::string& ns,
                                       const std::string& name) {
    if (name == "main") return "main";
    return ns + "__" + name;
}

std::vector<std::string> NamespaceRegistry::allNamespaces() const {
    std::vector<std::string> result;
    result.reserve(namespaces_.size());
    for (auto& [ns, _] : namespaces_)
        result.push_back(ns);
    std::sort(result.begin(), result.end());
    return result;
}
