
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
    OR = 34, EXTERN = 35, AUTO = 36, VEC = 37, MAP = 38, SET = 39, TUPLE = 40, 
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
    ATTR_ERROR = 92, IDENTIFIER = 93, PLUS_ASSIGN = 94, MINUS_ASSIGN = 95, 
    STAR_ASSIGN = 96, SLASH_ASSIGN = 97, PERCENT_ASSIGN = 98, AMP_ASSIGN = 99, 
    PIPE_ASSIGN = 100, CARET_ASSIGN = 101, LSHIFT_ASSIGN = 102, RSHIFT_ASSIGN = 103, 
    SEMI = 104, COLON = 105, SCOPE = 106, COMMA = 107, DOT = 108, ASSIGN = 109, 
    LPAREN = 110, RPAREN = 111, LBRACE = 112, RBRACE = 113, LBRACKET = 114, 
    RBRACKET = 115, STAR = 116, AMPERSAND = 117, MINUS = 118, PLUS = 119, 
    SLASH = 120, PERCENT = 121, EQ = 122, NEQ = 123, LTE = 124, GTE = 125, 
    LT = 126, GT = 127, LAND = 128, LOR = 129, NOT = 130, INCR = 131, DECR = 132, 
    LSHIFT = 133, PIPE = 134, CARET = 135, TILDE = 136, QUESTION = 137, 
    WS = 138, LINE_COMMENT = 139, BLOCK_COMMENT = 140
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

