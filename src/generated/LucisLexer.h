
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
    OPERATOR = 33, TRY = 34, CATCH = 35, FINALLY = 36, THROW = 37, BAIL = 38, 
    DEFER = 39, MATCH = 40, OR = 41, EXTERN = 42, ASM = 43, VOLATILE = 44, 
    GOTO = 45, INTEL = 46, AUTO = 47, SELF = 48, VEC = 49, MAP = 50, SET = 51, 
    BITSTRUCT = 52, TUPLE = 53, ARROW = 54, INCLUDE_SYS = 55, INCLUDE_LOCAL = 56, 
    INLINE_BLOCK = 57, SCOPE_BLOCK = 58, C_MACRO_BLOCK = 59, ASM_B_BLOCK = 60, 
    NULLCOAL_ASSIGN = 61, NULLCOAL = 62, SPREAD = 63, RANGE_INCL = 64, RANGE = 65, 
    INT1 = 66, INT8 = 67, INT16 = 68, INT32 = 69, INT64 = 70, INT128 = 71, 
    INTINF = 72, ISIZE = 73, UINT1 = 74, UINT8 = 75, UINT16 = 76, UINT32 = 77, 
    UINT64 = 78, UINT128 = 79, USIZE = 80, FLOAT32 = 81, FLOAT64 = 82, FLOAT80 = 83, 
    FLOAT128 = 84, DOUBLE = 85, BOOL = 86, CHAR = 87, VOID = 88, STRING = 89, 
    CSTRING = 90, SUFFIXED_HEX = 91, SUFFIXED_OCT = 92, SUFFIXED_BIN = 93, 
    SUFFIXED_INT = 94, SUFFIXED_INT_FLOAT = 95, SUFFIXED_FLOAT = 96, SUFFIXED_FLOAT_INT = 97, 
    SUFFIXED_DOT_FLOAT = 98, HEX_LIT = 99, OCT_LIT = 100, BIN_LIT = 101, 
    INT_LIT = 102, FLOAT_LIT = 103, BOOL_LIT = 104, C_STR_LIT = 105, STR_LIT = 106, 
    BTICK = 107, RAW_BTICK = 108, INT_BTICK = 109, SHELL_BTICK = 110, CMPT_BTICK = 111, 
    CHAR_LIT = 112, ATTR_OPEN = 113, WILDCARD = 114, IDENTIFIER = 115, PLUS_ASSIGN = 116, 
    MINUS_ASSIGN = 117, STAR_ASSIGN = 118, SLASH_ASSIGN = 119, PERCENT_ASSIGN = 120, 
    AMP_ASSIGN = 121, PIPE_ASSIGN = 122, CARET_ASSIGN = 123, LSHIFT_ASSIGN = 124, 
    RSHIFT_ASSIGN = 125, SEMI = 126, COLON = 127, SCOPE = 128, COMMA = 129, 
    DOT = 130, ASSIGN = 131, LPAREN = 132, RPAREN = 133, LBRACE = 134, RBRACE = 135, 
    LBRACKET = 136, RBRACKET = 137, STAR = 138, AMPERSAND = 139, MINUS = 140, 
    PLUS = 141, SLASH = 142, PERCENT = 143, EQ = 144, NEQ = 145, LTE = 146, 
    GTE = 147, LT = 148, GT = 149, LAND = 150, LOR = 151, NOT = 152, INCR = 153, 
    DECR = 154, LSHIFT = 155, PIPE = 156, CARET = 157, TILDE = 158, QUESTION = 159, 
    AT = 160, WS = 161, LINE_COMMENT = 162, BLOCK_COMMENT = 163
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

