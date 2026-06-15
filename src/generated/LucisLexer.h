
// Generated from LucisLexer.g4 by ANTLR 4.13.2

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
    SUFFIXED_INT = 78, SUFFIXED_INT_FLOAT = 79, SUFFIXED_FLOAT = 80, SUFFIXED_FLOAT_INT = 81, 
    SUFFIXED_DOT_FLOAT = 82, HEX_LIT = 83, OCT_LIT = 84, BIN_LIT = 85, INT_LIT = 86, 
    FLOAT_LIT = 87, BOOL_LIT = 88, C_STR_LIT = 89, STR_LIT = 90, CHAR_LIT = 91, 
    IDENTIFIER = 92, PLUS_ASSIGN = 93, MINUS_ASSIGN = 94, STAR_ASSIGN = 95, 
    SLASH_ASSIGN = 96, PERCENT_ASSIGN = 97, AMP_ASSIGN = 98, PIPE_ASSIGN = 99, 
    CARET_ASSIGN = 100, LSHIFT_ASSIGN = 101, RSHIFT_ASSIGN = 102, SEMI = 103, 
    COLON = 104, SCOPE = 105, COMMA = 106, DOT = 107, ASSIGN = 108, LPAREN = 109, 
    RPAREN = 110, LBRACE = 111, RBRACE = 112, LBRACKET = 113, RBRACKET = 114, 
    STAR = 115, AMPERSAND = 116, MINUS = 117, PLUS = 118, SLASH = 119, PERCENT = 120, 
    EQ = 121, NEQ = 122, LTE = 123, GTE = 124, LT = 125, GT = 126, LAND = 127, 
    LOR = 128, NOT = 129, INCR = 130, DECR = 131, LSHIFT = 132, PIPE = 133, 
    CARET = 134, TILDE = 135, QUESTION = 136, WS = 137, LINE_COMMENT = 138, 
    BLOCK_COMMENT = 139
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

