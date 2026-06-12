#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "parser/Parser.h"
#include "namespace/NamespaceRegistry.h"
#include "ffi/CBindings.h"
#include "types/TypeRegistry.h"

// Holds the parsed state of the entire project (all .lc files).
// Used by LSP components to resolve cross-file symbols.
class ProjectContext {
public:
    // Build context from a file path. Scans the project root for .lc files,
    // parses them all, and builds a NamespaceRegistry + shared CBindings.
    // Returns false if no project could be determined.
    bool build(const std::string& filePath);

    const NamespaceRegistry& registry() const { return registry_; }
    const CBindings& cBindings() const { return cBindings_; }

    // Returns the namespace name and file path for the given source file.
    std::string namespaceFor(const std::string& filePath) const;

    // Returns the project root.
    const std::string& projectRoot() const { return projectRoot_; }

    // Check if context is valid.
    bool isValid() const { return valid_; }

    // Discover project root by walking up from the file path.
    static std::string findProjectRoot(const std::string& filePath);

private:
    // Extract namespace from a parse tree.
    static std::string extractNamespace(LucisParser::ProgramContext* tree);

    NamespaceRegistry registry_;
    CBindings         cBindings_;
    TypeRegistry      cTypeReg_;
    std::string       projectRoot_;
    bool              valid_ = false;

    // Keep parse results alive (they own the ASTs referenced by the registry).
    struct SourceUnit {
        std::string   filePath;
        std::string   namespaceName;
        ParseResult   parseResult;
    };
    std::vector<SourceUnit> units_;

    // Map: filePath → namespace name
    std::unordered_map<std::string, std::string> fileNamespaces_;
};
