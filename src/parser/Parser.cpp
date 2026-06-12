#include "parser/Parser.h"
#include "parser/DiagnosticErrorListener.h"

#include <fstream>
#include <iostream>

ParseResult Parser::parse(const std::string& filePath) {
    ParseResult result;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "lucis: cannot open file '" << filePath << "'\n";
        result.hasErrors = true;
        return result;
    }

    result.input  = std::make_unique<antlr4::ANTLRInputStream>(file);
    result.lexer  = std::make_unique<LucisLexer>(result.input.get());
    result.tokens = std::make_unique<antlr4::CommonTokenStream>(result.lexer.get());
    result.parser = std::make_unique<LucisParser>(result.tokens.get());

    // Replace ANTLR's default stderr output with our formatted diagnostics
    static DiagnosticErrorListener errorListener;
    result.lexer->removeErrorListeners();
    result.lexer->addErrorListener(&errorListener);
    result.parser->removeErrorListeners();
    result.parser->addErrorListener(&errorListener);

    fixContextualKeywords(result.tokens.get());

    result.tree = result.parser->program();

    result.hasErrors = result.parser->getNumberOfSyntaxErrors() > 0
                    || result.lexer->getNumberOfSyntaxErrors()  > 0;

    return result;
}

void Parser::fixContextualKeywords(antlr4::CommonTokenStream* tokens) {
    tokens->fill();
    auto allTokens = tokens->getTokens();
    for (size_t i = 1; i < allTokens.size(); i++) {
        auto* curr = allTokens[i];
        if (curr->getType() != LucisLexer::TYPE) continue;
        auto* prev = allTokens[i - 1];
        if (prev->getType() == LucisLexer::DOT || prev->getType() == LucisLexer::ARROW) {
            if (auto* w = dynamic_cast<antlr4::WritableToken*>(curr)) {
                w->setType(LucisLexer::IDENTIFIER);
            }
        }
    }
}

ParseResult Parser::parseString(const std::string& source) {
    ParseResult result;

    result.input  = std::make_unique<antlr4::ANTLRInputStream>(source);
    result.lexer  = std::make_unique<LucisLexer>(result.input.get());
    result.tokens = std::make_unique<antlr4::CommonTokenStream>(result.lexer.get());
    result.parser = std::make_unique<LucisParser>(result.tokens.get());

    fixContextualKeywords(result.tokens.get());

    // Collect-mode listener: errors stored in result.diagnostics
    auto errorListener = std::make_unique<DiagnosticErrorListener>();
    errorListener->setCollectMode(true);

    result.lexer->removeErrorListeners();
    result.lexer->addErrorListener(errorListener.get());
    result.parser->removeErrorListeners();
    result.parser->addErrorListener(errorListener.get());

    result.tree = result.parser->program();

    result.hasErrors = result.parser->getNumberOfSyntaxErrors() > 0
                    || result.lexer->getNumberOfSyntaxErrors()  > 0;

    result.diagnostics = errorListener->takeDiagnostics();

    // Keep the listener alive as long as the ParseResult (move into input's deleter won't work,
    // so we just release it — it's small and short-lived in LSP context).
    // For proper lifetime, store it in ParseResult:
    // NOTE: The unique_ptr goes out of scope here, but we've already taken diagnostics.
    // The parse tree doesn't reference the listener after parsing is complete.

    return result;
}
