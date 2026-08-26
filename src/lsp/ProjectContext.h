#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

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
    // Like build(), but parses the anchor file from the given in-memory
    // source (the editor's opened document) instead of from disk. Used for
    // unsaved/in-memory documents; imported modules are still read from disk.
    bool buildFromText(const std::string& filePath, const std::string& source);
    bool buildImpl(const std::string& filePath, const std::string* anchorSource);

    ModuleRegistry& registry() { return registry_; }
    const ModuleRegistry& registry() const { return registry_; }
    const CBindings& cBindings() const { return cBindings_; }

    // Returns the module path for the given source file (relative to project root).
    std::string modulePathFor(const std::string& filePath) const;

    // Returns the project root.
    const std::string& projectRoot() const { return projectRoot_; }

    // Source paths from lucis.yaml (for import resolution).
    const std::vector<std::string>& sourcePaths() const { return sourcePaths_; }

    // Check if context is valid.
    bool isValid() const { return valid_; }

    // Structured import error with file location info.
    struct ImportError {
        std::string message;
        std::string filePath;
        size_t      line = 0;
        size_t      col  = 0;
    };

    // Import errors (e.g. circular imports) detected during build.
    const std::vector<ImportError>& importErrors() const { return importErrors_; }

    // Discover project root by walking up from the file path.
    static std::string findProjectRoot(const std::string& filePath);

    // Parse a stdlib .lc file and cache the result persistently (across rebuilds).
    // Returns nullptr if filePath is not inside any stdlib directory.
    std::shared_ptr<ParseResult> getStdlibParse(const std::string& filePath) const;

    // Parse a project file, reusing the cached parse when the file is
    // unchanged on disk since the last build.
    std::shared_ptr<ParseResult> parseProjectFile(const std::string& canonPath) const;

private:
    ModuleRegistry              registry_;
    CBindings                   cBindings_;
    TypeRegistry                cTypeReg_;
    std::string                 projectRoot_;
    std::vector<std::string>    sourcePaths_;
    bool                        valid_ = false;

    // Keep parse results alive (they own the ASTs referenced by the registry).
    struct SourceUnit {
        std::string   filePath;
        std::string   modulePath;
        std::shared_ptr<ParseResult> parseResult;
    };
    std::vector<SourceUnit> units_;

    // Import errors (circular imports, etc.)
    std::vector<ImportError> importErrors_;

    // Map: filePath → module path
    std::unordered_map<std::string, std::string> fileModulePaths_;

    // Persistent cache of stdlib parse results (never invalidated within a session).
    mutable std::unordered_map<std::string, std::shared_ptr<ParseResult>> stdlibCache_;

    // Persistent cache of project-file parses, keyed by canonical path.
    // Entries whose mtime is unchanged are reused across rebuilds — the
    // project is only re-parsed when a file actually changed on disk.
    struct CachedProjectFile {
        std::filesystem::file_time_type mtime;
        std::shared_ptr<ParseResult>    result;
    };
    mutable std::unordered_map<std::string, CachedProjectFile> projectFileCache_;

    // Resolve a use ident (e.g. "std::log") to a file path.
    std::string resolveUseToFile(const std::string& useIdent) const;
};
