#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Describes a single imported symbol (e.g. "println" from "std::log").
struct ImportedSymbol {
    std::string modulePath;  // e.g. "std::log"
    std::string name;        // e.g. "println"
    std::string localName;   // local binding; equals name without an alias
};

// Manages all `use` declarations in a file and resolves
// which builtin C function to call for a given Lucis symbol.
class ImportResolver {
public:
    // Register a single import:  use std::log::println;
    void addImport(const std::string& modulePath, const std::string& symbol);

    // Register an import with a local alias: use std::log::println as log;
    void addImport(const std::string& modulePath, const std::string& symbol,
                   const std::string& localName);

    // Returns true if `symbol` was imported by any `use` declaration.
    bool isImported(const std::string& symbol) const;

    // Returns true when `name` is the exported name of some registered
    // import (matched after alias translation, e.g. "abs" for
    // `use std::math::abs as myAbs;`).
    bool isExportedSymbol(const std::string& name) const;

    // Returns the exported name for a local binding, or an empty string
    // when the binding is not a registered import.
    std::string exportedName(const std::string& localName) const;

    // Given an imported symbol and the LLVM type suffix (e.g. "i32", "i64"),
    // returns the mangled C function name (e.g. "lucis_println_i32").
    // Returns empty string if the symbol is unknown.
    std::string resolve(const std::string& symbol,
                        const std::string& typeSuffix) const;

    // Returns all registered imports (mainly for the checker).
    const std::vector<ImportedSymbol>& imports() const { return imports_; }

    // Returns a "use std::module::symbol;" hint string if the symbol is
    // exported by any known module, or an empty string otherwise.
    static std::string suggestImport(const std::string& symbol);

    // Returns true if the given module path is a standard library path.
    static bool isStdModule(const std::string& modulePath);

    // Returns true if the module exports the given symbol name.
    static bool moduleExportsSymbol(const std::string& modulePath,
                                    const std::string& symbolName);

    // Returns the standard library install path.
    static std::string stdlibPath();

    // Returns all standard library search paths (env var, compile-time, system fallbacks).
    static std::vector<std::string> stdlibSearchPaths();

    // Returns the map of known std modules → their exported symbols.
    static const std::unordered_map<std::string, std::unordered_set<std::string>>&
    knownModules() { return knownModules_; }

private:
    std::vector<ImportedSymbol>        imports_;
    std::unordered_set<std::string>    symbols_;  // local bindings, fast lookup
    std::unordered_set<std::string>    exportedNames_;  // exported names too

    // Known builtin modules and their exported symbols
    static const std::unordered_map<std::string,
                                    std::unordered_set<std::string>> knownModules_;
};
