
// Generated from /home/carlos/Projects/Cpp/Lux/grammar/LucisLexer.g4 by ANTLR 4.13.2

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
    ASM_B_BLOCK = 56, NULLCOAL_ASSIGN = 57, NULLCOAL = 58, SPREAD = 59, 
    RANGE_INCL = 60, RANGE = 61, INT1 = 62, INT8 = 63, INT16 = 64, INT32 = 65, 
    INT64 = 66, INT128 = 67, INTINF = 68, ISIZE = 69, UINT1 = 70, UINT8 = 71, 
    UINT16 = 72, UINT32 = 73, UINT64 = 74, UINT128 = 75, USIZE = 76, FLOAT32 = 77, 
    FLOAT64 = 78, FLOAT80 = 79, FLOAT128 = 80, DOUBLE = 81, BOOL = 82, CHAR = 83, 
    VOID = 84, STRING = 85, CSTRING = 86, SUFFIXED_HEX = 87, SUFFIXED_OCT = 88, 
    SUFFIXED_BIN = 89, SUFFIXED_INT = 90, SUFFIXED_INT_FLOAT = 91, SUFFIXED_FLOAT = 92, 
    SUFFIXED_FLOAT_INT = 93, SUFFIXED_DOT_FLOAT = 94, HEX_LIT = 95, OCT_LIT = 96, 
    BIN_LIT = 97, INT_LIT = 98, FLOAT_LIT = 99, BOOL_LIT = 100, C_STR_LIT = 101, 
    STR_LIT = 102, BTICK = 103, RAW_BTICK = 104, INT_BTICK = 105, SHELL_BTICK = 106, 
    CMPT_BTICK = 107, CHAR_LIT = 108, ATTR_OPEN = 109, WILDCARD = 110, IDENTIFIER = 111, 
    PLUS_ASSIGN = 112, MINUS_ASSIGN = 113, STAR_ASSIGN = 114, SLASH_ASSIGN = 115, 
    PERCENT_ASSIGN = 116, AMP_ASSIGN = 117, PIPE_ASSIGN = 118, CARET_ASSIGN = 119, 
    LSHIFT_ASSIGN = 120, RSHIFT_ASSIGN = 121, SEMI = 122, COLON = 123, SCOPE = 124, 
    COMMA = 125, DOT = 126, ASSIGN = 127, LPAREN = 128, RPAREN = 129, LBRACE = 130, 
    RBRACE = 131, LBRACKET = 132, RBRACKET = 133, STAR = 134, AMPERSAND = 135, 
    MINUS = 136, PLUS = 137, SLASH = 138, PERCENT = 139, EQ = 140, NEQ = 141, 
    LTE = 142, GTE = 143, LT = 144, GT = 145, LAND = 146, LOR = 147, NOT = 148, 
    INCR = 149, DECR = 150, LSHIFT = 151, PIPE = 152, CARET = 153, TILDE = 154, 
    QUESTION = 155, WS = 156, LINE_COMMENT = 157, BLOCK_COMMENT = 158
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

