
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
    INLINE_BLOCK = 51, SCOPE_BLOCK = 52, NULLCOAL_ASSIGN = 53, NULLCOAL = 54, 
    SPREAD = 55, RANGE_INCL = 56, RANGE = 57, INT1 = 58, INT8 = 59, INT16 = 60, 
    INT32 = 61, INT64 = 62, INT128 = 63, INTINF = 64, ISIZE = 65, UINT1 = 66, 
    UINT8 = 67, UINT16 = 68, UINT32 = 69, UINT64 = 70, UINT128 = 71, USIZE = 72, 
    FLOAT32 = 73, FLOAT64 = 74, FLOAT80 = 75, FLOAT128 = 76, DOUBLE = 77, 
    BOOL = 78, CHAR = 79, VOID = 80, STRING = 81, CSTRING = 82, SUFFIXED_HEX = 83, 
    SUFFIXED_OCT = 84, SUFFIXED_BIN = 85, SUFFIXED_INT = 86, SUFFIXED_INT_FLOAT = 87, 
    SUFFIXED_FLOAT = 88, SUFFIXED_FLOAT_INT = 89, SUFFIXED_DOT_FLOAT = 90, 
    HEX_LIT = 91, OCT_LIT = 92, BIN_LIT = 93, INT_LIT = 94, FLOAT_LIT = 95, 
    BOOL_LIT = 96, C_STR_LIT = 97, STR_LIT = 98, CHAR_LIT = 99, ATTR_ERROR = 100, 
    WILDCARD = 101, IDENTIFIER = 102, PLUS_ASSIGN = 103, MINUS_ASSIGN = 104, 
    STAR_ASSIGN = 105, SLASH_ASSIGN = 106, PERCENT_ASSIGN = 107, AMP_ASSIGN = 108, 
    PIPE_ASSIGN = 109, CARET_ASSIGN = 110, LSHIFT_ASSIGN = 111, RSHIFT_ASSIGN = 112, 
    SEMI = 113, COLON = 114, SCOPE = 115, COMMA = 116, DOT = 117, ASSIGN = 118, 
    LPAREN = 119, RPAREN = 120, LBRACE = 121, RBRACE = 122, LBRACKET = 123, 
    RBRACKET = 124, STAR = 125, AMPERSAND = 126, MINUS = 127, PLUS = 128, 
    SLASH = 129, PERCENT = 130, EQ = 131, NEQ = 132, LTE = 133, GTE = 134, 
    LT = 135, GT = 136, LAND = 137, LOR = 138, NOT = 139, INCR = 140, DECR = 141, 
    LSHIFT = 142, PIPE = 143, CARET = 144, TILDE = 145, QUESTION = 146, 
    WS = 147, LINE_COMMENT = 148, BLOCK_COMMENT = 149
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

