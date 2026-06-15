
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
    EXTERN = 34, AUTO = 35, VEC = 36, MAP = 37, SET = 38, TUPLE = 39, ARROW = 40, 
    INCLUDE_SYS = 41, INCLUDE_LOCAL = 42, INLINE_BLOCK = 43, SCOPE_BLOCK = 44, 
    NULLCOAL = 45, SPREAD = 46, RANGE_INCL = 47, RANGE = 48, INT1 = 49, 
    INT8 = 50, INT16 = 51, INT32 = 52, INT64 = 53, INT128 = 54, INTINF = 55, 
    ISIZE = 56, UINT1 = 57, UINT8 = 58, UINT16 = 59, UINT32 = 60, UINT64 = 61, 
    UINT128 = 62, USIZE = 63, FLOAT32 = 64, FLOAT64 = 65, FLOAT80 = 66, 
    FLOAT128 = 67, DOUBLE = 68, BOOL = 69, CHAR = 70, VOID = 71, STRING = 72, 
    CSTRING = 73, SUFFIXED_HEX = 74, SUFFIXED_OCT = 75, SUFFIXED_BIN = 76, 
    SUFFIXED_INT = 77, SUFFIXED_INT_FLOAT = 78, SUFFIXED_FLOAT = 79, SUFFIXED_FLOAT_INT = 80, 
    SUFFIXED_DOT_FLOAT = 81, HEX_LIT = 82, OCT_LIT = 83, BIN_LIT = 84, INT_LIT = 85, 
    FLOAT_LIT = 86, BOOL_LIT = 87, C_STR_LIT = 88, STR_LIT = 89, CHAR_LIT = 90, 
    IDENTIFIER = 91, PLUS_ASSIGN = 92, MINUS_ASSIGN = 93, STAR_ASSIGN = 94, 
    SLASH_ASSIGN = 95, PERCENT_ASSIGN = 96, AMP_ASSIGN = 97, PIPE_ASSIGN = 98, 
    CARET_ASSIGN = 99, LSHIFT_ASSIGN = 100, RSHIFT_ASSIGN = 101, SEMI = 102, 
    COLON = 103, SCOPE = 104, COMMA = 105, DOT = 106, ASSIGN = 107, LPAREN = 108, 
    RPAREN = 109, LBRACE = 110, RBRACE = 111, LBRACKET = 112, RBRACKET = 113, 
    STAR = 114, AMPERSAND = 115, MINUS = 116, PLUS = 117, SLASH = 118, PERCENT = 119, 
    EQ = 120, NEQ = 121, LTE = 122, GTE = 123, LT = 124, GT = 125, LAND = 126, 
    LOR = 127, NOT = 128, INCR = 129, DECR = 130, LSHIFT = 131, PIPE = 132, 
    CARET = 133, TILDE = 134, QUESTION = 135, WS = 136, LINE_COMMENT = 137, 
    BLOCK_COMMENT = 138
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

