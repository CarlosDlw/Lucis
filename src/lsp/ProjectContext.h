#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "parser/Parser.h"
#include "namespace/ModuleRegistry.h"
#include "ffi/CBindings.h"
#include "types/TypeRegistry.h"

// Holds the parsed state of the entire project (all .lc files).
// Used by LSP components to resolve cross-file symbols.
// Uses path-based BFS import resolution (no directory scanning).
class ProjectContext {
public:
    // Build context from a file path. Resolves imports transitively
    // starting from the entry point file.
    // Returns false if no project could be determined.
    bool build(const std::string& filePath);

    ModuleRegistry& registry() { return registry_; }
    const ModuleRegistry& registry() const { return registry_; }
    const CBindings& cBindings() const { return cBindings_; }

    // Returns the module path for the given source file (relative to project root).
    std::string modulePathFor(const std::string& filePath) const;

    // Returns the project root.
    const std::string& projectRoot() const { return projectRoot_; }

    // Check if context is valid.
    bool isValid() const { return valid_; }

    // Discover project root by walking up from the file path.
    static std::string findProjectRoot(const std::string& filePath);

private:
    ModuleRegistry registry_;
    CBindings      cBindings_;
    TypeRegistry   cTypeReg_;
    std::string    projectRoot_;
    bool           valid_ = false;

    // Keep parse results alive (they own the ASTs referenced by the registry).
    struct SourceUnit {
        std::string   filePath;
        std::string   modulePath;
        ParseResult   parseResult;
    };
    std::vector<SourceUnit> units_;

    // Map: filePath → module path
    std::unordered_map<std::string, std::string> fileModulePaths_;

    // Resolve a use ident (e.g. "std::log") to a file path.
    std::string resolveUseToFile(const std::string& useIdent) const;
};
