
// Generated from LucisLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisLexer : public antlr4::Lexer {
public:
  enum {
    USE = 1, RET = 2, STRUCT = 3, UNION = 4, ENUM = 5, NULL_LIT = 6, FN = 7, 
    TYPE = 8, AS = 9, IS = 10, SIZEOF = 11, TYPEOF = 12, IF = 13, ELSE = 14, 
    FOR = 15, IN = 16, LOOP = 17, WHILE = 18, DO = 19, BREAK = 20, CONTINUE = 21, 
    SWITCH = 22, CASE = 23, DEFAULT = 24, SPAWN = 25, AWAIT = 26, LOCK = 27, 
    EXTEND = 28, TRY = 29, CATCH = 30, FINALLY = 31, THROW = 32, DEFER = 33, 
    MATCH = 34, OR = 35, EXTERN = 36, ASM = 37, VOLATILE = 38, AUTO = 39, 
    VEC = 40, MAP = 41, SET = 42, TUPLE = 43, ARROW = 44, INCLUDE_SYS = 45, 
    INCLUDE_LOCAL = 46, INLINE_BLOCK = 47, SCOPE_BLOCK = 48, NULLCOAL = 49, 
    SPREAD = 50, RANGE_INCL = 51, RANGE = 52, INT1 = 53, INT8 = 54, INT16 = 55, 
    INT32 = 56, INT64 = 57, INT128 = 58, INTINF = 59, ISIZE = 60, UINT1 = 61, 
    UINT8 = 62, UINT16 = 63, UINT32 = 64, UINT64 = 65, UINT128 = 66, USIZE = 67, 
    FLOAT32 = 68, FLOAT64 = 69, FLOAT80 = 70, FLOAT128 = 71, DOUBLE = 72, 
    BOOL = 73, CHAR = 74, VOID = 75, STRING = 76, CSTRING = 77, SUFFIXED_HEX = 78, 
    SUFFIXED_OCT = 79, SUFFIXED_BIN = 80, SUFFIXED_INT = 81, SUFFIXED_INT_FLOAT = 82, 
    SUFFIXED_FLOAT = 83, SUFFIXED_FLOAT_INT = 84, SUFFIXED_DOT_FLOAT = 85, 
    HEX_LIT = 86, OCT_LIT = 87, BIN_LIT = 88, INT_LIT = 89, FLOAT_LIT = 90, 
    BOOL_LIT = 91, C_STR_LIT = 92, STR_LIT = 93, CHAR_LIT = 94, ATTR_ERROR = 95, 
    WILDCARD = 96, IDENTIFIER = 97, PLUS_ASSIGN = 98, MINUS_ASSIGN = 99, 
    STAR_ASSIGN = 100, SLASH_ASSIGN = 101, PERCENT_ASSIGN = 102, AMP_ASSIGN = 103, 
    PIPE_ASSIGN = 104, CARET_ASSIGN = 105, LSHIFT_ASSIGN = 106, RSHIFT_ASSIGN = 107, 
    SEMI = 108, COLON = 109, SCOPE = 110, COMMA = 111, DOT = 112, ASSIGN = 113, 
    LPAREN = 114, RPAREN = 115, LBRACE = 116, RBRACE = 117, LBRACKET = 118, 
    RBRACKET = 119, STAR = 120, AMPERSAND = 121, MINUS = 122, PLUS = 123, 
    SLASH = 124, PERCENT = 125, EQ = 126, NEQ = 127, LTE = 128, GTE = 129, 
    LT = 130, GT = 131, LAND = 132, LOR = 133, NOT = 134, INCR = 135, DECR = 136, 
    LSHIFT = 137, PIPE = 138, CARET = 139, TILDE = 140, QUESTION = 141, 
    WS = 142, LINE_COMMENT = 143, BLOCK_COMMENT = 144
  };

  explicit LucisLexer(antlr4::CharStream *input);

  ~LucisLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

