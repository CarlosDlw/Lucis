
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
    OPERATOR = 33, TRY = 34, CATCH = 35, FINALLY = 36, THROW = 37, DEFER = 38, 
    MATCH = 39, OR = 40, EXTERN = 41, ASM = 42, VOLATILE = 43, GOTO = 44, 
    INTEL = 45, AUTO = 46, VEC = 47, MAP = 48, SET = 49, TUPLE = 50, ARROW = 51, 
    INCLUDE_SYS = 52, INCLUDE_LOCAL = 53, INLINE_BLOCK = 54, SCOPE_BLOCK = 55, 
    C_MACRO_BLOCK = 56, ASM_B_BLOCK = 57, NULLCOAL_ASSIGN = 58, NULLCOAL = 59, 
    SPREAD = 60, RANGE_INCL = 61, RANGE = 62, INT1 = 63, INT8 = 64, INT16 = 65, 
    INT32 = 66, INT64 = 67, INT128 = 68, INTINF = 69, ISIZE = 70, UINT1 = 71, 
    UINT8 = 72, UINT16 = 73, UINT32 = 74, UINT64 = 75, UINT128 = 76, USIZE = 77, 
    FLOAT32 = 78, FLOAT64 = 79, FLOAT80 = 80, FLOAT128 = 81, DOUBLE = 82, 
    BOOL = 83, CHAR = 84, VOID = 85, STRING = 86, CSTRING = 87, SUFFIXED_HEX = 88, 
    SUFFIXED_OCT = 89, SUFFIXED_BIN = 90, SUFFIXED_INT = 91, SUFFIXED_INT_FLOAT = 92, 
    SUFFIXED_FLOAT = 93, SUFFIXED_FLOAT_INT = 94, SUFFIXED_DOT_FLOAT = 95, 
    HEX_LIT = 96, OCT_LIT = 97, BIN_LIT = 98, INT_LIT = 99, FLOAT_LIT = 100, 
    BOOL_LIT = 101, C_STR_LIT = 102, STR_LIT = 103, BTICK = 104, RAW_BTICK = 105, 
    INT_BTICK = 106, SHELL_BTICK = 107, CMPT_BTICK = 108, CHAR_LIT = 109, 
    ATTR_OPEN = 110, WILDCARD = 111, IDENTIFIER = 112, PLUS_ASSIGN = 113, 
    MINUS_ASSIGN = 114, STAR_ASSIGN = 115, SLASH_ASSIGN = 116, PERCENT_ASSIGN = 117, 
    AMP_ASSIGN = 118, PIPE_ASSIGN = 119, CARET_ASSIGN = 120, LSHIFT_ASSIGN = 121, 
    RSHIFT_ASSIGN = 122, SEMI = 123, COLON = 124, SCOPE = 125, COMMA = 126, 
    DOT = 127, ASSIGN = 128, LPAREN = 129, RPAREN = 130, LBRACE = 131, RBRACE = 132, 
    LBRACKET = 133, RBRACKET = 134, STAR = 135, AMPERSAND = 136, MINUS = 137, 
    PLUS = 138, SLASH = 139, PERCENT = 140, EQ = 141, NEQ = 142, LTE = 143, 
    GTE = 144, LT = 145, GT = 146, LAND = 147, LOR = 148, NOT = 149, INCR = 150, 
    DECR = 151, LSHIFT = 152, PIPE = 153, CARET = 154, TILDE = 155, QUESTION = 156, 
    AT = 157, WS = 158, LINE_COMMENT = 159, BLOCK_COMMENT = 160
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

