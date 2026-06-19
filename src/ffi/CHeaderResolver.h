#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include "ffi/CBindings.h"
#include "ffi/CTypeMapper.h"
#include "types/TypeRegistry.h"

// Parses C headers via libclang and populates a CBindings registry
// with functions, structs, enums, and typedefs found in the header.
class CHeaderResolver {
public:
    CHeaderResolver(TypeRegistry& registry, CBindings& bindings,
                    const std::vector<std::string>& includePaths = {});

    // Parse a system header (e.g. "stdio.h") and populate bindings.
    // Returns true on success, false on parse failure.
    bool resolveSystemHeader(const std::string& headerName);

    // Parse a local header (e.g. "mylib.h") and populate bindings.
    // basePath is the directory of the including source file.
    bool resolveLocalHeader(const std::string& headerName,
                            const std::string& basePath);

    // Extract header name from an INCLUDE_SYS token text like: #include <stdio.h>
    static std::string extractSystemHeader(const std::string& tokenText);

    // Extract header name from an INCLUDE_LOCAL token text like: #include "mylib.h"
    static std::string extractLocalHeader(const std::string& tokenText);

    // Override the project root used for the header cache location.
    // When set, the cache is stored at <root>/.lucis/headers.cache instead
    // of auto-detecting by walking up from current_path().
    static void setHeaderCacheRoot(const std::string& root);

    // List available system header files (for completion).
    // Returns header names as they would appear in #include <...> (e.g. "stdio.h", "sys/types.h").
    static std::vector<std::string> listSystemHeaders();

private:
    CTypeMapper mapper_;
    CBindings&  bindings_;
    std::vector<std::string> includePaths_;

    // Track already-parsed headers to avoid duplicate work
    std::unordered_set<std::string> parsedHeaders_;

    // System include paths discovered from clang
    std::vector<std::string> systemIncludes_;

    bool parseHeader(const std::string& headerContent,
                     const std::vector<std::string>& clangArgs);

    // Detect and register required linker libraries for a header.
    void detectRequiredLib(const std::string& headerName);
};
