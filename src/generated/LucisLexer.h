
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
    TUPLE = 52, ARROW = 53, INCLUDE_SYS = 54, INCLUDE_LOCAL = 55, INLINE_BLOCK = 56, 
    SCOPE_BLOCK = 57, C_MACRO_BLOCK = 58, ASM_B_BLOCK = 59, NULLCOAL_ASSIGN = 60, 
    NULLCOAL = 61, SPREAD = 62, RANGE_INCL = 63, RANGE = 64, INT1 = 65, 
    INT8 = 66, INT16 = 67, INT32 = 68, INT64 = 69, INT128 = 70, INTINF = 71, 
    ISIZE = 72, UINT1 = 73, UINT8 = 74, UINT16 = 75, UINT32 = 76, UINT64 = 77, 
    UINT128 = 78, USIZE = 79, FLOAT32 = 80, FLOAT64 = 81, FLOAT80 = 82, 
    FLOAT128 = 83, DOUBLE = 84, BOOL = 85, CHAR = 86, VOID = 87, STRING = 88, 
    CSTRING = 89, SUFFIXED_HEX = 90, SUFFIXED_OCT = 91, SUFFIXED_BIN = 92, 
    SUFFIXED_INT = 93, SUFFIXED_INT_FLOAT = 94, SUFFIXED_FLOAT = 95, SUFFIXED_FLOAT_INT = 96, 
    SUFFIXED_DOT_FLOAT = 97, HEX_LIT = 98, OCT_LIT = 99, BIN_LIT = 100, 
    INT_LIT = 101, FLOAT_LIT = 102, BOOL_LIT = 103, C_STR_LIT = 104, STR_LIT = 105, 
    BTICK = 106, RAW_BTICK = 107, INT_BTICK = 108, SHELL_BTICK = 109, CMPT_BTICK = 110, 
    CHAR_LIT = 111, ATTR_OPEN = 112, WILDCARD = 113, IDENTIFIER = 114, PLUS_ASSIGN = 115, 
    MINUS_ASSIGN = 116, STAR_ASSIGN = 117, SLASH_ASSIGN = 118, PERCENT_ASSIGN = 119, 
    AMP_ASSIGN = 120, PIPE_ASSIGN = 121, CARET_ASSIGN = 122, LSHIFT_ASSIGN = 123, 
    RSHIFT_ASSIGN = 124, SEMI = 125, COLON = 126, SCOPE = 127, COMMA = 128, 
    DOT = 129, ASSIGN = 130, LPAREN = 131, RPAREN = 132, LBRACE = 133, RBRACE = 134, 
    LBRACKET = 135, RBRACKET = 136, STAR = 137, AMPERSAND = 138, MINUS = 139, 
    PLUS = 140, SLASH = 141, PERCENT = 142, EQ = 143, NEQ = 144, LTE = 145, 
    GTE = 146, LT = 147, GT = 148, LAND = 149, LOR = 150, NOT = 151, INCR = 152, 
    DECR = 153, LSHIFT = 154, PIPE = 155, CARET = 156, TILDE = 157, QUESTION = 158, 
    AT = 159, WS = 160, LINE_COMMENT = 161, BLOCK_COMMENT = 162
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

