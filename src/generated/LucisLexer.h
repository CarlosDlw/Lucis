
// Generated from grammar/LucisLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisLexer : public antlr4::Lexer {
public:
  enum {
    NAMESPACE = 1, USE = 2, RET = 3, STRUCT = 4, UNION = 5, ENUM = 6, NULL_LIT = 7, 
    FN = 8, TYPE = 9, AS = 10, IS = 11, SIZEOF = 12, TYPEOF = 13, IF = 14, 
    ELSE = 15, FOR = 16, IN = 17, LOOP = 18, WHILE = 19, DO = 20, BREAK = 21, 
    CONTINUE = 22, SWITCH = 23, CASE = 24, DEFAULT = 25, SPAWN = 26, AWAIT = 27, 
    LOCK = 28, EXTEND = 29, TRY = 30, CATCH = 31, FINALLY = 32, THROW = 33, 
    DEFER = 34, EXTERN = 35, AUTO = 36, VEC = 37, MAP = 38, SET = 39, TUPLE = 40, 
    ARROW = 41, INCLUDE_SYS = 42, INCLUDE_LOCAL = 43, INLINE_BLOCK = 44, 
    SCOPE_BLOCK = 45, NULLCOAL = 46, SPREAD = 47, RANGE_INCL = 48, RANGE = 49, 
    INT1 = 50, INT8 = 51, INT16 = 52, INT32 = 53, INT64 = 54, INT128 = 55, 
    INTINF = 56, ISIZE = 57, UINT1 = 58, UINT8 = 59, UINT16 = 60, UINT32 = 61, 
    UINT64 = 62, UINT128 = 63, USIZE = 64, FLOAT32 = 65, FLOAT64 = 66, FLOAT80 = 67, 
    FLOAT128 = 68, DOUBLE = 69, BOOL = 70, CHAR = 71, VOID = 72, STRING = 73, 
    CSTRING = 74, SUFFIXED_HEX = 75, SUFFIXED_OCT = 76, SUFFIXED_BIN = 77, 
    SUFFIXED_INT = 78, SUFFIXED_FLOAT = 79, SUFFIXED_DOT_FLOAT = 80, HEX_LIT = 81, 
    OCT_LIT = 82, BIN_LIT = 83, INT_LIT = 84, FLOAT_LIT = 85, BOOL_LIT = 86, 
    C_STR_LIT = 87, STR_LIT = 88, CHAR_LIT = 89, IDENTIFIER = 90, PLUS_ASSIGN = 91, 
    MINUS_ASSIGN = 92, STAR_ASSIGN = 93, SLASH_ASSIGN = 94, PERCENT_ASSIGN = 95, 
    AMP_ASSIGN = 96, PIPE_ASSIGN = 97, CARET_ASSIGN = 98, LSHIFT_ASSIGN = 99, 
    RSHIFT_ASSIGN = 100, SEMI = 101, COLON = 102, SCOPE = 103, COMMA = 104, 
    DOT = 105, ASSIGN = 106, LPAREN = 107, RPAREN = 108, LBRACE = 109, RBRACE = 110, 
    LBRACKET = 111, RBRACKET = 112, STAR = 113, AMPERSAND = 114, MINUS = 115, 
    PLUS = 116, SLASH = 117, PERCENT = 118, EQ = 119, NEQ = 120, LTE = 121, 
    GTE = 122, LT = 123, GT = 124, LAND = 125, LOR = 126, NOT = 127, INCR = 128, 
    DECR = 129, LSHIFT = 130, PIPE = 131, CARET = 132, TILDE = 133, QUESTION = 134, 
    WS = 135, LINE_COMMENT = 136, BLOCK_COMMENT = 137
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

