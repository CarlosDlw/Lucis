
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
    INTEL = 40, AUTO = 41, VEC = 42, MAP = 43, SET = 44, TUPLE = 45, ARROW = 46, 
    INCLUDE_SYS = 47, INCLUDE_LOCAL = 48, INLINE_BLOCK = 49, SCOPE_BLOCK = 50, 
    NULLCOAL = 51, SPREAD = 52, RANGE_INCL = 53, RANGE = 54, INT1 = 55, 
    INT8 = 56, INT16 = 57, INT32 = 58, INT64 = 59, INT128 = 60, INTINF = 61, 
    ISIZE = 62, UINT1 = 63, UINT8 = 64, UINT16 = 65, UINT32 = 66, UINT64 = 67, 
    UINT128 = 68, USIZE = 69, FLOAT32 = 70, FLOAT64 = 71, FLOAT80 = 72, 
    FLOAT128 = 73, DOUBLE = 74, BOOL = 75, CHAR = 76, VOID = 77, STRING = 78, 
    CSTRING = 79, SUFFIXED_HEX = 80, SUFFIXED_OCT = 81, SUFFIXED_BIN = 82, 
    SUFFIXED_INT = 83, SUFFIXED_INT_FLOAT = 84, SUFFIXED_FLOAT = 85, SUFFIXED_FLOAT_INT = 86, 
    SUFFIXED_DOT_FLOAT = 87, HEX_LIT = 88, OCT_LIT = 89, BIN_LIT = 90, INT_LIT = 91, 
    FLOAT_LIT = 92, BOOL_LIT = 93, C_STR_LIT = 94, STR_LIT = 95, CHAR_LIT = 96, 
    ATTR_ERROR = 97, WILDCARD = 98, IDENTIFIER = 99, PLUS_ASSIGN = 100, 
    MINUS_ASSIGN = 101, STAR_ASSIGN = 102, SLASH_ASSIGN = 103, PERCENT_ASSIGN = 104, 
    AMP_ASSIGN = 105, PIPE_ASSIGN = 106, CARET_ASSIGN = 107, LSHIFT_ASSIGN = 108, 
    RSHIFT_ASSIGN = 109, SEMI = 110, COLON = 111, SCOPE = 112, COMMA = 113, 
    DOT = 114, ASSIGN = 115, LPAREN = 116, RPAREN = 117, LBRACE = 118, RBRACE = 119, 
    LBRACKET = 120, RBRACKET = 121, STAR = 122, AMPERSAND = 123, MINUS = 124, 
    PLUS = 125, SLASH = 126, PERCENT = 127, EQ = 128, NEQ = 129, LTE = 130, 
    GTE = 131, LT = 132, GT = 133, LAND = 134, LOR = 135, NOT = 136, INCR = 137, 
    DECR = 138, LSHIFT = 139, PIPE = 140, CARET = 141, TILDE = 142, QUESTION = 143, 
    WS = 144, LINE_COMMENT = 145, BLOCK_COMMENT = 146
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

