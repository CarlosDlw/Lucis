#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct ParseResult;

// LSP Semantic Token Types (indices into the legend array)
enum class SemanticTokenType : uint32_t {
    Namespace  = 0,
    Type       = 1,
    Struct     = 2,
    Enum       = 3,
    EnumMember = 4,
    Function   = 5,
    Method     = 6,
    Parameter  = 7,
    Variable   = 8,
    Property      = 9,
    Keyword       = 10,
    Comment       = 11,
    String        = 12,
    Number        = 13,
    Operator      = 14,
    Macro         = 15,
    EscapeSequence = 16,
};

// LSP Semantic Token Modifiers (bit flags)
enum class SemanticTokenMod : uint32_t {
    None        = 0,
    Declaration = 1 << 0,
    Definition  = 1 << 1,
    Readonly    = 1 << 2,
    Static      = 1 << 3,
    DefaultLib  = 1 << 4,
    Comptime    = 1 << 5,
};

// A single classified token before delta-encoding.
struct RawSemanticToken {
    uint32_t line;      // 0-based
    uint32_t col;       // 0-based
    uint32_t length;
    uint32_t type;      // SemanticTokenType index
    uint32_t modifiers; // Bitmask of SemanticTokenMod
};

// Provides semantic token classification for Lucis source code.
// Uses the ANTLR parse tree + lexer tokens for accurate classification.
class SemanticTokensProvider {
public:
    // Returns delta-encoded semantic tokens for the full document.
    std::vector<uint32_t> tokenize(const std::string& source,
                                   ParseResult* preParsed = nullptr);

    // Token type legend (order must match SemanticTokenType enum).
    static const std::vector<std::string>& tokenTypes();

    // Token modifier legend (order must match SemanticTokenMod bit positions).
    static const std::vector<std::string>& tokenModifiers();
};
