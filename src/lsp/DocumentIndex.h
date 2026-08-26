#pragma once

#include <string>
#include <vector>

#include "lsp/FileSymbolIndex.h"
#include "lsp/TokenIndex.h"
#include "lsp/DocComment.h"

// Cached per-document analysis: symbol index, token index and parsed
// doc-comments, built once per parse. The LSP server keeps one per URI
// (rebuilt only when the document text changes) so providers stop
// re-scanning the source and the parse tree on every request.
//
// Like its parts, the index borrows from the ParseResult (AST nodes and
// tokens) — it must not outlive the parse it was built from. The server
// keeps both alive for the same document.
class DocumentIndex {
public:
    DocumentIndex(LucisParser::ProgramContext* tree,
                  const std::vector<antlr4::Token*>& tokens,
                  const std::string& source);

    const FileSymbolIndex& symbols() const { return symbols_; }
    const TokenIndex&      tokens() const  { return tokens_; }
    const std::vector<DocComment>& docComments() const { return docComments_; }

private:
    FileSymbolIndex        symbols_;
    TokenIndex             tokens_;
    std::vector<DocComment> docComments_;
};