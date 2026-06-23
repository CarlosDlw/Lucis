
// Generated from LucisLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisLexer : public antlr4::Lexer {
public:
  enum {
    USE = 1, RET = 2, STRUCT = 3, UNION = 4, ENUM = 5, NULL_LIT = 6, FN = 7, 
    COMPTIME = 8, TYPE = 9, AS = 10, IS = 11, SIZEOF = 12, TYPEOF = 13, 
    IF = 14, ELSE = 15, FOR = 16, IN = 17, LOOP = 18, WHILE = 19, DO = 20, 
    BREAK = 21, CONTINUE = 22, SWITCH = 23, CASE = 24, DEFAULT = 25, SPAWN = 26, 
    AWAIT = 27, LOCK = 28, EXTEND = 29, TRY = 30, CATCH = 31, FINALLY = 32, 
    THROW = 33, DEFER = 34, MATCH = 35, OR = 36, EXTERN = 37, ASM = 38, 
    VOLATILE = 39, GOTO = 40, INTEL = 41, AUTO = 42, VEC = 43, MAP = 44, 
    SET = 45, TUPLE = 46, ARROW = 47, INCLUDE_SYS = 48, INCLUDE_LOCAL = 49, 
    INLINE_BLOCK = 50, SCOPE_BLOCK = 51, NULLCOAL = 52, SPREAD = 53, RANGE_INCL = 54, 
    RANGE = 55, INT1 = 56, INT8 = 57, INT16 = 58, INT32 = 59, INT64 = 60, 
    INT128 = 61, INTINF = 62, ISIZE = 63, UINT1 = 64, UINT8 = 65, UINT16 = 66, 
    UINT32 = 67, UINT64 = 68, UINT128 = 69, USIZE = 70, FLOAT32 = 71, FLOAT64 = 72, 
    FLOAT80 = 73, FLOAT128 = 74, DOUBLE = 75, BOOL = 76, CHAR = 77, VOID = 78, 
    STRING = 79, CSTRING = 80, SUFFIXED_HEX = 81, SUFFIXED_OCT = 82, SUFFIXED_BIN = 83, 
    SUFFIXED_INT = 84, SUFFIXED_INT_FLOAT = 85, SUFFIXED_FLOAT = 86, SUFFIXED_FLOAT_INT = 87, 
    SUFFIXED_DOT_FLOAT = 88, HEX_LIT = 89, OCT_LIT = 90, BIN_LIT = 91, INT_LIT = 92, 
    FLOAT_LIT = 93, BOOL_LIT = 94, C_STR_LIT = 95, STR_LIT = 96, CHAR_LIT = 97, 
    ATTR_ERROR = 98, WILDCARD = 99, IDENTIFIER = 100, PLUS_ASSIGN = 101, 
    MINUS_ASSIGN = 102, STAR_ASSIGN = 103, SLASH_ASSIGN = 104, PERCENT_ASSIGN = 105, 
    AMP_ASSIGN = 106, PIPE_ASSIGN = 107, CARET_ASSIGN = 108, LSHIFT_ASSIGN = 109, 
    RSHIFT_ASSIGN = 110, SEMI = 111, COLON = 112, SCOPE = 113, COMMA = 114, 
    DOT = 115, ASSIGN = 116, LPAREN = 117, RPAREN = 118, LBRACE = 119, RBRACE = 120, 
    LBRACKET = 121, RBRACKET = 122, STAR = 123, AMPERSAND = 124, MINUS = 125, 
    PLUS = 126, SLASH = 127, PERCENT = 128, EQ = 129, NEQ = 130, LTE = 131, 
    GTE = 132, LT = 133, GT = 134, LAND = 135, LOR = 136, NOT = 137, INCR = 138, 
    DECR = 139, LSHIFT = 140, PIPE = 141, CARET = 142, TILDE = 143, QUESTION = 144, 
    WS = 145, LINE_COMMENT = 146, BLOCK_COMMENT = 147
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

