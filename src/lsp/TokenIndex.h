#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "antlr4-runtime.h"

// Per-file index of the ANTLR token stream, keyed by line. The LSP
// providers used to scan every token of the document to find the token
// under the cursor; this index turns that into O(tokens-on-line) lookups
// (the index itself is built once per parse and cached by the server).
//
// The index stores raw Token* pointers — the owning token stream must
// outlive it (the server keeps both alive for the same document).
class TokenIndex {
public:
    // Builds the index from a token stream's tokens. Tokens of every
    // channel are kept, matching the linear scans the index replaces;
    // the EOF sentinel is skipped.
    explicit TokenIndex(const std::vector<antlr4::Token*>& tokens);

    // Returns the token covering the given 0-based line:col (LSP
    // coordinates), or nullptr. Same semantics as the old linear scan:
    // the first token in stream order whose [start, end) spans the column.
    antlr4::Token* tokenAt(size_t line, size_t col) const;

private:
    // 1-based line → tokens on that line, in stream order.
    std::unordered_map<size_t, std::vector<antlr4::Token*>> tokensByLine_;
};