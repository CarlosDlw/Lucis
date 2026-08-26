#include "lsp/DocumentIndex.h"

DocumentIndex::DocumentIndex(LucisParser::ProgramContext* tree,
                             const std::vector<antlr4::Token*>& tokens,
                             const std::string& source)
    : symbols_(tree),
      tokens_(tokens),
      docComments_(parseDocComments(source)) {
}