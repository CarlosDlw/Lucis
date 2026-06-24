
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
    INLINE_BLOCK = 51, SCOPE_BLOCK = 52, NULLCOAL = 53, SPREAD = 54, RANGE_INCL = 55, 
    RANGE = 56, INT1 = 57, INT8 = 58, INT16 = 59, INT32 = 60, INT64 = 61, 
    INT128 = 62, INTINF = 63, ISIZE = 64, UINT1 = 65, UINT8 = 66, UINT16 = 67, 
    UINT32 = 68, UINT64 = 69, UINT128 = 70, USIZE = 71, FLOAT32 = 72, FLOAT64 = 73, 
    FLOAT80 = 74, FLOAT128 = 75, DOUBLE = 76, BOOL = 77, CHAR = 78, VOID = 79, 
    STRING = 80, CSTRING = 81, SUFFIXED_HEX = 82, SUFFIXED_OCT = 83, SUFFIXED_BIN = 84, 
    SUFFIXED_INT = 85, SUFFIXED_INT_FLOAT = 86, SUFFIXED_FLOAT = 87, SUFFIXED_FLOAT_INT = 88, 
    SUFFIXED_DOT_FLOAT = 89, HEX_LIT = 90, OCT_LIT = 91, BIN_LIT = 92, INT_LIT = 93, 
    FLOAT_LIT = 94, BOOL_LIT = 95, C_STR_LIT = 96, STR_LIT = 97, CHAR_LIT = 98, 
    ATTR_ERROR = 99, WILDCARD = 100, IDENTIFIER = 101, PLUS_ASSIGN = 102, 
    MINUS_ASSIGN = 103, STAR_ASSIGN = 104, SLASH_ASSIGN = 105, PERCENT_ASSIGN = 106, 
    AMP_ASSIGN = 107, PIPE_ASSIGN = 108, CARET_ASSIGN = 109, LSHIFT_ASSIGN = 110, 
    RSHIFT_ASSIGN = 111, SEMI = 112, COLON = 113, SCOPE = 114, COMMA = 115, 
    DOT = 116, ASSIGN = 117, LPAREN = 118, RPAREN = 119, LBRACE = 120, RBRACE = 121, 
    LBRACKET = 122, RBRACKET = 123, STAR = 124, AMPERSAND = 125, MINUS = 126, 
    PLUS = 127, SLASH = 128, PERCENT = 129, EQ = 130, NEQ = 131, LTE = 132, 
    GTE = 133, LT = 134, GT = 135, LAND = 136, LOR = 137, NOT = 138, INCR = 139, 
    DECR = 140, LSHIFT = 141, PIPE = 142, CARET = 143, TILDE = 144, QUESTION = 145, 
    WS = 146, LINE_COMMENT = 147, BLOCK_COMMENT = 148
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

