
// Generated from LucisLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisLexer : public antlr4::Lexer {
public:
  enum {
    USE = 1, RET = 2, STRUCT = 3, UNION = 4, ENUM = 5, NULL_LIT = 6, FN = 7, 
    COMPTIME = 8, CONST = 9, TYPE = 10, AS = 11, IS = 12, SIZEOF = 13, TYPEOF = 14, 
    ALIGNOF = 15, OFFSETOF = 16, IF = 17, ELSE = 18, FOR = 19, IN = 20, 
    LOOP = 21, WHILE = 22, DO = 23, BREAK = 24, CONTINUE = 25, SWITCH = 26, 
    CASE = 27, DEFAULT = 28, SPAWN = 29, AWAIT = 30, LOCK = 31, EXTEND = 32, 
    TRY = 33, CATCH = 34, FINALLY = 35, THROW = 36, DEFER = 37, MATCH = 38, 
    OR = 39, EXTERN = 40, ASM = 41, VOLATILE = 42, GOTO = 43, INTEL = 44, 
    AUTO = 45, VEC = 46, MAP = 47, SET = 48, TUPLE = 49, ARROW = 50, INCLUDE_SYS = 51, 
    INCLUDE_LOCAL = 52, INLINE_BLOCK = 53, SCOPE_BLOCK = 54, C_MACRO_BLOCK = 55, 
    NULLCOAL_ASSIGN = 56, NULLCOAL = 57, SPREAD = 58, RANGE_INCL = 59, RANGE = 60, 
    INT1 = 61, INT8 = 62, INT16 = 63, INT32 = 64, INT64 = 65, INT128 = 66, 
    INTINF = 67, ISIZE = 68, UINT1 = 69, UINT8 = 70, UINT16 = 71, UINT32 = 72, 
    UINT64 = 73, UINT128 = 74, USIZE = 75, FLOAT32 = 76, FLOAT64 = 77, FLOAT80 = 78, 
    FLOAT128 = 79, DOUBLE = 80, BOOL = 81, CHAR = 82, VOID = 83, STRING = 84, 
    CSTRING = 85, SUFFIXED_HEX = 86, SUFFIXED_OCT = 87, SUFFIXED_BIN = 88, 
    SUFFIXED_INT = 89, SUFFIXED_INT_FLOAT = 90, SUFFIXED_FLOAT = 91, SUFFIXED_FLOAT_INT = 92, 
    SUFFIXED_DOT_FLOAT = 93, HEX_LIT = 94, OCT_LIT = 95, BIN_LIT = 96, INT_LIT = 97, 
    FLOAT_LIT = 98, BOOL_LIT = 99, C_STR_LIT = 100, STR_LIT = 101, CHAR_LIT = 102, 
    ATTR_ERROR = 103, WILDCARD = 104, IDENTIFIER = 105, PLUS_ASSIGN = 106, 
    MINUS_ASSIGN = 107, STAR_ASSIGN = 108, SLASH_ASSIGN = 109, PERCENT_ASSIGN = 110, 
    AMP_ASSIGN = 111, PIPE_ASSIGN = 112, CARET_ASSIGN = 113, LSHIFT_ASSIGN = 114, 
    RSHIFT_ASSIGN = 115, SEMI = 116, COLON = 117, SCOPE = 118, COMMA = 119, 
    DOT = 120, ASSIGN = 121, LPAREN = 122, RPAREN = 123, LBRACE = 124, RBRACE = 125, 
    LBRACKET = 126, RBRACKET = 127, STAR = 128, AMPERSAND = 129, MINUS = 130, 
    PLUS = 131, SLASH = 132, PERCENT = 133, EQ = 134, NEQ = 135, LTE = 136, 
    GTE = 137, LT = 138, GT = 139, LAND = 140, LOR = 141, NOT = 142, INCR = 143, 
    DECR = 144, LSHIFT = 145, PIPE = 146, CARET = 147, TILDE = 148, QUESTION = 149, 
    WS = 150, LINE_COMMENT = 151, BLOCK_COMMENT = 152
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

