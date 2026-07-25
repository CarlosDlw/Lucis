
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
    TRAIT = 33, IMPL = 34, DYN = 35, OPERATOR = 36, TRY = 37, CATCH = 38, 
    FINALLY = 39, THROW = 40, BAIL = 41, DEFER = 42, MATCH = 43, OR = 44, 
    EXTERN = 45, ASM = 46, VOLATILE = 47, GOTO = 48, INTEL = 49, AUTO = 50, 
    SELF = 51, VEC = 52, MAP = 53, SET = 54, BITSTRUCT = 55, TUPLE = 56, 
    ARROW = 57, INCLUDE_SYS = 58, INCLUDE_LOCAL = 59, INLINE_BLOCK = 60, 
    SCOPE_BLOCK = 61, C_MACRO_BLOCK = 62, ASM_B_BLOCK = 63, NULLCOAL_ASSIGN = 64, 
    NULLCOAL = 65, SPREAD = 66, RANGE_INCL = 67, RANGE = 68, INT1 = 69, 
    INT8 = 70, INT16 = 71, INT32 = 72, INT64 = 73, INT128 = 74, INTINF = 75, 
    ISIZE = 76, UINT1 = 77, UINT8 = 78, UINT16 = 79, UINT32 = 80, UINT64 = 81, 
    UINT128 = 82, USIZE = 83, FLOAT32 = 84, FLOAT64 = 85, FLOAT80 = 86, 
    FLOAT128 = 87, DOUBLE = 88, BOOL = 89, CHAR = 90, VOID = 91, STRING = 92, 
    CSTRING = 93, SUFFIXED_HEX = 94, SUFFIXED_OCT = 95, SUFFIXED_BIN = 96, 
    SUFFIXED_INT = 97, SUFFIXED_INT_FLOAT = 98, SUFFIXED_FLOAT = 99, SUFFIXED_FLOAT_INT = 100, 
    SUFFIXED_DOT_FLOAT = 101, HEX_LIT = 102, OCT_LIT = 103, BIN_LIT = 104, 
    INT_LIT = 105, FLOAT_LIT = 106, BOOL_LIT = 107, C_STR_LIT = 108, STR_LIT = 109, 
    BTICK = 110, RAW_BTICK = 111, INT_BTICK = 112, SHELL_BTICK = 113, CMPT_BTICK = 114, 
    CHAR_LIT = 115, ATTR_OPEN = 116, WILDCARD = 117, IDENTIFIER = 118, PLUS_ASSIGN = 119, 
    MINUS_ASSIGN = 120, STAR_ASSIGN = 121, SLASH_ASSIGN = 122, PERCENT_ASSIGN = 123, 
    AMP_ASSIGN = 124, PIPE_ASSIGN = 125, CARET_ASSIGN = 126, LSHIFT_ASSIGN = 127, 
    RSHIFT_ASSIGN = 128, SEMI = 129, COLON = 130, SCOPE = 131, COMMA = 132, 
    DOT = 133, ASSIGN = 134, LPAREN = 135, RPAREN = 136, LBRACE = 137, RBRACE = 138, 
    LBRACKET = 139, RBRACKET = 140, STAR = 141, AMPERSAND = 142, MINUS = 143, 
    PLUS = 144, SLASH = 145, PERCENT = 146, EQ = 147, NEQ = 148, LTE = 149, 
    GTE = 150, LT = 151, GT = 152, LAND = 153, LOR = 154, NOT = 155, INCR = 156, 
    DECR = 157, LSHIFT = 158, PIPE = 159, CARET = 160, TILDE = 161, QUESTION = 162, 
    AT = 163, WS = 164, LINE_COMMENT = 165, BLOCK_COMMENT = 166
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

