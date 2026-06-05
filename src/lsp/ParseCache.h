#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include "parser/Parser.h"

// Thread-compatible cache for ANTLR parse results keyed by URI.
// The LSP server is single-threaded, so the returned pointer is valid
// until the next invalidate() or getOrParse() call for the same URI.
class ParseCache {
public:
    // Returns cached parse for URI if source matches, otherwise parses & caches.
    ParseResult* getOrParse(const std::string& uri, const std::string& source);

    // Remove cached entry for URI (call on didChange / didOpen / didClose).
    void invalidate(const std::string& uri);

    // Clear all cached entries.
    void clear();

private:
    struct Entry {
        std::string source;
        std::unique_ptr<ParseResult> result;
    };
    std::unordered_map<std::string, Entry> cache_;
};
