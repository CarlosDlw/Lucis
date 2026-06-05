
// Generated from /home/carlos/Projects/Cpp/Lux/grammar/LuxLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LuxLexer : public antlr4::Lexer {
public:
  enum {
    NAMESPACE = 1, USE = 2, RET = 3, STRUCT = 4, UNION = 5, ENUM = 6, NULL_LIT = 7, 
    FN = 8, TYPE = 9, AS = 10, IS = 11, SIZEOF = 12, TYPEOF = 13, IF = 14, 
    ELSE = 15, FOR = 16, IN = 17, LOOP = 18, WHILE = 19, DO = 20, BREAK = 21, 
    CONTINUE = 22, SWITCH = 23, CASE = 24, DEFAULT = 25, SPAWN = 26, AWAIT = 27, 
    LOCK = 28, EXTEND = 29, TRY = 30, CATCH = 31, FINALLY = 32, THROW = 33, 
    DEFER = 34, EXTERN = 35, AUTO = 36, VEC = 37, MAP = 38, SET = 39, TUPLE = 40, 
    ARROW = 41, INCLUDE_SYS = 42, INCLUDE_LOCAL = 43, INLINE_BLOCK = 44, 
    SCOPE_BLOCK = 45, NULLCOAL = 46, SPREAD = 47, RANGE_INCL = 48, RANGE = 49, 
    INT1 = 50, INT8 = 51, INT16 = 52, INT32 = 53, INT64 = 54, INT128 = 55, 
    INTINF = 56, ISIZE = 57, UINT1 = 58, UINT8 = 59, UINT16 = 60, UINT32 = 61, 
    UINT64 = 62, UINT128 = 63, USIZE = 64, FLOAT32 = 65, FLOAT64 = 66, FLOAT80 = 67, 
    FLOAT128 = 68, DOUBLE = 69, BOOL = 70, CHAR = 71, VOID = 72, STRING = 73, 
    CSTRING = 74, HEX_LIT = 75, OCT_LIT = 76, BIN_LIT = 77, INT_LIT = 78, 
    FLOAT_LIT = 79, BOOL_LIT = 80, C_STR_LIT = 81, STR_LIT = 82, CHAR_LIT = 83, 
    IDENTIFIER = 84, PLUS_ASSIGN = 85, MINUS_ASSIGN = 86, STAR_ASSIGN = 87, 
    SLASH_ASSIGN = 88, PERCENT_ASSIGN = 89, AMP_ASSIGN = 90, PIPE_ASSIGN = 91, 
    CARET_ASSIGN = 92, LSHIFT_ASSIGN = 93, RSHIFT_ASSIGN = 94, SEMI = 95, 
    COLON = 96, SCOPE = 97, COMMA = 98, DOT = 99, ASSIGN = 100, LPAREN = 101, 
    RPAREN = 102, LBRACE = 103, RBRACE = 104, LBRACKET = 105, RBRACKET = 106, 
    STAR = 107, AMPERSAND = 108, MINUS = 109, PLUS = 110, SLASH = 111, PERCENT = 112, 
    EQ = 113, NEQ = 114, LTE = 115, GTE = 116, LT = 117, GT = 118, LAND = 119, 
    LOR = 120, NOT = 121, INCR = 122, DECR = 123, LSHIFT = 124, PIPE = 125, 
    CARET = 126, TILDE = 127, QUESTION = 128, WS = 129, LINE_COMMENT = 130, 
    BLOCK_COMMENT = 131
  };

  explicit LuxLexer(antlr4::CharStream *input);

  ~LuxLexer() override;


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

