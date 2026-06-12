#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "generated/LucisParser.h"

// Describes a single exported top-level symbol within a namespace.
struct ExportedSymbol {
    enum Kind { Function, Struct, Union, Enum, TypeAlias, ExtendBlock };

    Kind        kind;
    std::string name;
    std::string namespaceName;
    std::string sourceFile;

    // Non-owning pointer to the parse tree node (owned by ParseResult).
    // Use the appropriate cast based on `kind`.
    antlr4::ParserRuleContext* decl = nullptr;
};

// Central registry mapping namespace names to their exported symbols.
// Built during the project scan phase, before any compilation begins.
class NamespaceRegistry {
public:
    // Register all top-level declarations from a parsed file.
    void registerFile(const std::string& namespaceName,
                      const std::string& filePath,
                      LucisParser::ProgramContext* tree);

    // Validate the registry: check for duplicate symbols within namespaces.
    // Returns a list of human-readable error messages (empty = OK).
    std::vector<std::string> validate() const;

    // Find a symbol by namespace and name. Returns nullptr if not found.
    const ExportedSymbol* findSymbol(const std::string& ns,
                                     const std::string& name) const;

    // Get all symbols in a namespace.
    std::vector<const ExportedSymbol*> getNamespaceSymbols(
        const std::string& ns) const;

    // Get symbols in a namespace defined in files OTHER than `excludeFile`.
    std::vector<const ExportedSymbol*> getExternalSymbols(
        const std::string& ns,
        const std::string& excludeFile) const;

    // Check if a namespace exists in the registry.
    bool hasNamespace(const std::string& ns) const;

    // Check if a namespace is a standard library module.
    static bool isStdModule(const std::string& modulePath);

    // Mangle a symbol name: "Namespace__name"
    // Exception: "main" always stays "main".
    static std::string mangle(const std::string& ns, const std::string& name);

    // Get all registered namespace names.
    std::vector<std::string> allNamespaces() const;

private:
    // namespace name → list of exported symbols
    std::unordered_map<std::string, std::vector<ExportedSymbol>> namespaces_;
};
