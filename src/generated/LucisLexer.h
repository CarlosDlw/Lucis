
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
    MATCH = 34, OR = 35, EXTERN = 36, ASM = 37, VOLATILE = 38, GOTO = 39, 
    AUTO = 40, VEC = 41, MAP = 42, SET = 43, TUPLE = 44, ARROW = 45, INCLUDE_SYS = 46, 
    INCLUDE_LOCAL = 47, INLINE_BLOCK = 48, SCOPE_BLOCK = 49, NULLCOAL = 50, 
    SPREAD = 51, RANGE_INCL = 52, RANGE = 53, INT1 = 54, INT8 = 55, INT16 = 56, 
    INT32 = 57, INT64 = 58, INT128 = 59, INTINF = 60, ISIZE = 61, UINT1 = 62, 
    UINT8 = 63, UINT16 = 64, UINT32 = 65, UINT64 = 66, UINT128 = 67, USIZE = 68, 
    FLOAT32 = 69, FLOAT64 = 70, FLOAT80 = 71, FLOAT128 = 72, DOUBLE = 73, 
    BOOL = 74, CHAR = 75, VOID = 76, STRING = 77, CSTRING = 78, SUFFIXED_HEX = 79, 
    SUFFIXED_OCT = 80, SUFFIXED_BIN = 81, SUFFIXED_INT = 82, SUFFIXED_INT_FLOAT = 83, 
    SUFFIXED_FLOAT = 84, SUFFIXED_FLOAT_INT = 85, SUFFIXED_DOT_FLOAT = 86, 
    HEX_LIT = 87, OCT_LIT = 88, BIN_LIT = 89, INT_LIT = 90, FLOAT_LIT = 91, 
    BOOL_LIT = 92, C_STR_LIT = 93, STR_LIT = 94, CHAR_LIT = 95, ATTR_ERROR = 96, 
    WILDCARD = 97, IDENTIFIER = 98, PLUS_ASSIGN = 99, MINUS_ASSIGN = 100, 
    STAR_ASSIGN = 101, SLASH_ASSIGN = 102, PERCENT_ASSIGN = 103, AMP_ASSIGN = 104, 
    PIPE_ASSIGN = 105, CARET_ASSIGN = 106, LSHIFT_ASSIGN = 107, RSHIFT_ASSIGN = 108, 
    SEMI = 109, COLON = 110, SCOPE = 111, COMMA = 112, DOT = 113, ASSIGN = 114, 
    LPAREN = 115, RPAREN = 116, LBRACE = 117, RBRACE = 118, LBRACKET = 119, 
    RBRACKET = 120, STAR = 121, AMPERSAND = 122, MINUS = 123, PLUS = 124, 
    SLASH = 125, PERCENT = 126, EQ = 127, NEQ = 128, LTE = 129, GTE = 130, 
    LT = 131, GT = 132, LAND = 133, LOR = 134, NOT = 135, INCR = 136, DECR = 137, 
    LSHIFT = 138, PIPE = 139, CARET = 140, TILDE = 141, QUESTION = 142, 
    WS = 143, LINE_COMMENT = 144, BLOCK_COMMENT = 145
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

