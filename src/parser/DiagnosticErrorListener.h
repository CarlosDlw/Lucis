#pragma once

#include <antlr4-runtime.h>
#include <iostream>
#include <string>
#include <vector>

#include "lsp/Diagnostic.h"

// Silences the default ANTLR error output and reformats syntax errors
// to match the lucis diagnostic style:  lucis: <line>:<col>: <message>
//
// Two modes:
//   - Default (collectMode_ = false): prints to stderr (CLI usage)
//   - Collect  (collectMode_ = true):  stores structured Diagnostics (LSP usage)
class DiagnosticErrorListener : public antlr4::BaseErrorListener {
public:
    // Enable collect mode to store errors instead of printing.
    void setCollectMode(bool enabled) { collectMode_ = enabled; }

    // Retrieve collected diagnostics (only in collect mode). Clears the buffer.
    std::vector<Diagnostic> takeDiagnostics() {
        return std::move(collected_);
    }

    void syntaxError(antlr4::Recognizer* /*recognizer*/,
                     antlr4::Token* offendingSymbol,
                     size_t line, size_t charPositionInLine,
                     const std::string& msg,
                     std::exception_ptr /*e*/) override {

        std::string clean = simplify(msg, offendingSymbol);

        if (collectMode_) {
            Diagnostic d;
            d.line     = (line > 0) ? line - 1 : 0;  // LSP uses 0-based
            d.col      = charPositionInLine;           // already 0-based
            d.endLine  = d.line;
            // Highlight the full offending token when available
            if (offendingSymbol && offendingSymbol->getType() != antlr4::Token::EOF) {
                d.endCol = d.col + offendingSymbol->getText().size();
            } else {
                d.endCol = d.col + 1;
            }
            d.severity = Diagnostic::Error;
            d.message  = clean;
            collected_.push_back(std::move(d));
        } else {
            std::cerr << "lucis: " << line << ":" << charPositionInLine
                      << ": " << clean << "\n";
        }
    }

private:
    bool collectMode_ = false;
    std::vector<Diagnostic> collected_;

    static std::string simplify(const std::string& msg,
                                antlr4::Token* offending) {
        // "mismatched input 'X' expecting {long set...}" → extract expecting
        if (msg.rfind("mismatched input", 0) == 0) {
            std::string tok = offending ? quote(offending) : "?";
            auto pos = msg.find("expecting ");
            if (pos != std::string::npos) {
                auto expecting = msg.substr(pos + 10);
                return "unexpected " + tok + ", expecting " + simplifyExpecting(expecting);
            }
            return "unexpected " + tok;
        }

        // "extraneous input 'X' expecting ..."
        if (msg.rfind("extraneous input", 0) == 0) {
            std::string tok = offending ? quote(offending) : "?";
            auto pos = msg.find("expecting ");
            if (pos != std::string::npos) {
                auto expecting = msg.substr(pos + 10);
                return "unexpected " + tok + ", expecting " + simplifyExpecting(expecting);
            }
            return "unexpected " + tok;
        }

        // "missing 'X' at 'Y'"
        if (msg.rfind("missing", 0) == 0)
            return msg;  // already short enough

        // "no viable alternative at input 'X'"
        if (msg.rfind("no viable alternative", 0) == 0) {
            std::string tok = offending ? quote(offending) : "?";
            return "unexpected " + tok;
        }

        // "token recognition error at: 'X'" (lexer)
        if (msg.rfind("token recognition error", 0) == 0)
            return msg;

        return msg;
    }

    // Simplify ANTLR "expecting" sets into readable form
    static std::string simplifyExpecting(const std::string& raw) {
        // If it's a single token like "'ret'" or "';'", return as-is
        if (raw.size() < 40)
            return raw;
        // For large sets, just summarize
        if (raw.find("SEMI") != std::string::npos || raw.find("';'") != std::string::npos)
            return "';'";
        if (raw.find("RBRACE") != std::string::npos || raw.find("'}'") != std::string::npos)
            return "'}'";
        if (raw.find("RPAREN") != std::string::npos || raw.find("')'") != std::string::npos)
            return "')'";
        if (raw.find("RBRACKET") != std::string::npos || raw.find("']'") != std::string::npos)
            return "']'";
        return "a valid token";
    }

    static std::string quote(antlr4::Token* tok) {
        if (tok->getType() == antlr4::Token::EOF)
            return "end of file";
        return "'" + tok->getText() + "'";
    }
};
