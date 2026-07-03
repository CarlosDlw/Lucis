
// Generated from LucisLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisLexer : public antlr4::Lexer {
public:
  enum {
    USE = 1, RET = 2, STRUCT = 3, UNION = 4, ENUM = 5, NULL_LIT = 6, FN = 7, 
    COMPTIME = 8, CONST = 9, TYPE = 10, AS = 11, IS = 12, SIZEOF = 13, TYPEOF = 14, 
    IF = 15, ELSE = 16, FOR = 17, IN = 18, LOOP = 19, WHILE = 20, DO = 21, 
    BREAK = 22, CONTINUE = 23, SWITCH = 24, CASE = 25, DEFAULT = 26, SPAWN = 27, 
    AWAIT = 28, LOCK = 29, EXTEND = 30, TRY = 31, CATCH = 32, FINALLY = 33, 
    THROW = 34, DEFER = 35, MATCH = 36, OR = 37, EXTERN = 38, ASM = 39, 
    VOLATILE = 40, GOTO = 41, INTEL = 42, AUTO = 43, VEC = 44, MAP = 45, 
    SET = 46, TUPLE = 47, ARROW = 48, INCLUDE_SYS = 49, INCLUDE_LOCAL = 50, 
    INLINE_BLOCK = 51, SCOPE_BLOCK = 52, C_MACRO_BLOCK = 53, NULLCOAL_ASSIGN = 54, 
    NULLCOAL = 55, SPREAD = 56, RANGE_INCL = 57, RANGE = 58, INT1 = 59, 
    INT8 = 60, INT16 = 61, INT32 = 62, INT64 = 63, INT128 = 64, INTINF = 65, 
    ISIZE = 66, UINT1 = 67, UINT8 = 68, UINT16 = 69, UINT32 = 70, UINT64 = 71, 
    UINT128 = 72, USIZE = 73, FLOAT32 = 74, FLOAT64 = 75, FLOAT80 = 76, 
    FLOAT128 = 77, DOUBLE = 78, BOOL = 79, CHAR = 80, VOID = 81, STRING = 82, 
    CSTRING = 83, SUFFIXED_HEX = 84, SUFFIXED_OCT = 85, SUFFIXED_BIN = 86, 
    SUFFIXED_INT = 87, SUFFIXED_INT_FLOAT = 88, SUFFIXED_FLOAT = 89, SUFFIXED_FLOAT_INT = 90, 
    SUFFIXED_DOT_FLOAT = 91, HEX_LIT = 92, OCT_LIT = 93, BIN_LIT = 94, INT_LIT = 95, 
    FLOAT_LIT = 96, BOOL_LIT = 97, C_STR_LIT = 98, STR_LIT = 99, CHAR_LIT = 100, 
    ATTR_ERROR = 101, WILDCARD = 102, IDENTIFIER = 103, PLUS_ASSIGN = 104, 
    MINUS_ASSIGN = 105, STAR_ASSIGN = 106, SLASH_ASSIGN = 107, PERCENT_ASSIGN = 108, 
    AMP_ASSIGN = 109, PIPE_ASSIGN = 110, CARET_ASSIGN = 111, LSHIFT_ASSIGN = 112, 
    RSHIFT_ASSIGN = 113, SEMI = 114, COLON = 115, SCOPE = 116, COMMA = 117, 
    DOT = 118, ASSIGN = 119, LPAREN = 120, RPAREN = 121, LBRACE = 122, RBRACE = 123, 
    LBRACKET = 124, RBRACKET = 125, STAR = 126, AMPERSAND = 127, MINUS = 128, 
    PLUS = 129, SLASH = 130, PERCENT = 131, EQ = 132, NEQ = 133, LTE = 134, 
    GTE = 135, LT = 136, GT = 137, LAND = 138, LOR = 139, NOT = 140, INCR = 141, 
    DECR = 142, LSHIFT = 143, PIPE = 144, CARET = 145, TILDE = 146, QUESTION = 147, 
    WS = 148, LINE_COMMENT = 149, BLOCK_COMMENT = 150
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

