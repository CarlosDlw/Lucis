#include "lsp/TokenIndex.h"

TokenIndex::TokenIndex(const std::vector<antlr4::Token*>& tokens) {
    for (auto* tok : tokens) {
        if (!tok || tok->getType() == antlr4::Token::EOF) continue;
        tokensByLine_[static_cast<size_t>(tok->getLine())].push_back(tok);
    }
}

antlr4::Token* TokenIndex::tokenAt(size_t line, size_t col) const {
    auto it = tokensByLine_.find(line + 1);  // LSP 0-based → ANTLR 1-based
    if (it == tokensByLine_.end()) return nullptr;
    for (auto* tok : it->second) {
        size_t start = static_cast<size_t>(tok->getCharPositionInLine());
        size_t end   = start + tok->getText().length();
        if (col >= start && col < end)
            return tok;
    }
    return nullptr;
}