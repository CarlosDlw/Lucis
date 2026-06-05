#include "lsp/ParseCache.h"

ParseResult* ParseCache::getOrParse(const std::string& uri,
                                    const std::string& source) {
    auto it = cache_.find(uri);
    if (it != cache_.end() && it->second.source == source) {
        return it->second.result.get();
    }
    // Parse and cache (move-assignable since unique_ptr)
    auto entry = std::make_unique<ParseResult>(Parser::parseString(source));
    auto* ptr = entry.get();
    cache_[uri] = {source, std::move(entry)};
    return ptr;
}

void ParseCache::invalidate(const std::string& uri) {
    cache_.erase(uri);
}

void ParseCache::clear() {
    cache_.clear();
}
