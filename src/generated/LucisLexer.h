
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
    MATCH = 34, OR = 35, EXTERN = 36, AUTO = 37, VEC = 38, MAP = 39, SET = 40, 
    TUPLE = 41, ARROW = 42, INCLUDE_SYS = 43, INCLUDE_LOCAL = 44, INLINE_BLOCK = 45, 
    SCOPE_BLOCK = 46, NULLCOAL = 47, SPREAD = 48, RANGE_INCL = 49, RANGE = 50, 
    INT1 = 51, INT8 = 52, INT16 = 53, INT32 = 54, INT64 = 55, INT128 = 56, 
    INTINF = 57, ISIZE = 58, UINT1 = 59, UINT8 = 60, UINT16 = 61, UINT32 = 62, 
    UINT64 = 63, UINT128 = 64, USIZE = 65, FLOAT32 = 66, FLOAT64 = 67, FLOAT80 = 68, 
    FLOAT128 = 69, DOUBLE = 70, BOOL = 71, CHAR = 72, VOID = 73, STRING = 74, 
    CSTRING = 75, SUFFIXED_HEX = 76, SUFFIXED_OCT = 77, SUFFIXED_BIN = 78, 
    SUFFIXED_INT = 79, SUFFIXED_INT_FLOAT = 80, SUFFIXED_FLOAT = 81, SUFFIXED_FLOAT_INT = 82, 
    SUFFIXED_DOT_FLOAT = 83, HEX_LIT = 84, OCT_LIT = 85, BIN_LIT = 86, INT_LIT = 87, 
    FLOAT_LIT = 88, BOOL_LIT = 89, C_STR_LIT = 90, STR_LIT = 91, CHAR_LIT = 92, 
    ATTR_ERROR = 93, WILDCARD = 94, IDENTIFIER = 95, PLUS_ASSIGN = 96, MINUS_ASSIGN = 97, 
    STAR_ASSIGN = 98, SLASH_ASSIGN = 99, PERCENT_ASSIGN = 100, AMP_ASSIGN = 101, 
    PIPE_ASSIGN = 102, CARET_ASSIGN = 103, LSHIFT_ASSIGN = 104, RSHIFT_ASSIGN = 105, 
    SEMI = 106, COLON = 107, SCOPE = 108, COMMA = 109, DOT = 110, ASSIGN = 111, 
    LPAREN = 112, RPAREN = 113, LBRACE = 114, RBRACE = 115, LBRACKET = 116, 
    RBRACKET = 117, STAR = 118, AMPERSAND = 119, MINUS = 120, PLUS = 121, 
    SLASH = 122, PERCENT = 123, EQ = 124, NEQ = 125, LTE = 126, GTE = 127, 
    LT = 128, GT = 129, LAND = 130, LOR = 131, NOT = 132, INCR = 133, DECR = 134, 
    LSHIFT = 135, PIPE = 136, CARET = 137, TILDE = 138, QUESTION = 139, 
    WS = 140, LINE_COMMENT = 141, BLOCK_COMMENT = 142
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

