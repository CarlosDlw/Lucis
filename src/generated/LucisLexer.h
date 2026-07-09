
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
    ASM_B_BLOCK = 56, NULLCOAL_ASSIGN = 57, NULLCOAL = 58, SPREAD = 59, 
    RANGE_INCL = 60, RANGE = 61, INT1 = 62, INT8 = 63, INT16 = 64, INT32 = 65, 
    INT64 = 66, INT128 = 67, INTINF = 68, ISIZE = 69, UINT1 = 70, UINT8 = 71, 
    UINT16 = 72, UINT32 = 73, UINT64 = 74, UINT128 = 75, USIZE = 76, FLOAT32 = 77, 
    FLOAT64 = 78, FLOAT80 = 79, FLOAT128 = 80, DOUBLE = 81, BOOL = 82, CHAR = 83, 
    VOID = 84, STRING = 85, CSTRING = 86, SUFFIXED_HEX = 87, SUFFIXED_OCT = 88, 
    SUFFIXED_BIN = 89, SUFFIXED_INT = 90, SUFFIXED_INT_FLOAT = 91, SUFFIXED_FLOAT = 92, 
    SUFFIXED_FLOAT_INT = 93, SUFFIXED_DOT_FLOAT = 94, HEX_LIT = 95, OCT_LIT = 96, 
    BIN_LIT = 97, INT_LIT = 98, FLOAT_LIT = 99, BOOL_LIT = 100, C_STR_LIT = 101, 
    STR_LIT = 102, CHAR_LIT = 103, ATTR_ERROR = 104, WILDCARD = 105, IDENTIFIER = 106, 
    PLUS_ASSIGN = 107, MINUS_ASSIGN = 108, STAR_ASSIGN = 109, SLASH_ASSIGN = 110, 
    PERCENT_ASSIGN = 111, AMP_ASSIGN = 112, PIPE_ASSIGN = 113, CARET_ASSIGN = 114, 
    LSHIFT_ASSIGN = 115, RSHIFT_ASSIGN = 116, SEMI = 117, COLON = 118, SCOPE = 119, 
    COMMA = 120, DOT = 121, ASSIGN = 122, LPAREN = 123, RPAREN = 124, LBRACE = 125, 
    RBRACE = 126, LBRACKET = 127, RBRACKET = 128, STAR = 129, AMPERSAND = 130, 
    MINUS = 131, PLUS = 132, SLASH = 133, PERCENT = 134, EQ = 135, NEQ = 136, 
    LTE = 137, GTE = 138, LT = 139, GT = 140, LAND = 141, LOR = 142, NOT = 143, 
    INCR = 144, DECR = 145, LSHIFT = 146, PIPE = 147, CARET = 148, TILDE = 149, 
    QUESTION = 150, WS = 151, LINE_COMMENT = 152, BLOCK_COMMENT = 153
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

