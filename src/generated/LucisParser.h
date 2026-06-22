
// Generated from LucisParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  LucisParser : public antlr4::Parser {
public:
  enum {
    USE = 1, RET = 2, STRUCT = 3, UNION = 4, ENUM = 5, NULL_LIT = 6, FN = 7, 
    TYPE = 8, AS = 9, IS = 10, SIZEOF = 11, TYPEOF = 12, IF = 13, ELSE = 14, 
    FOR = 15, IN = 16, LOOP = 17, WHILE = 18, DO = 19, BREAK = 20, CONTINUE = 21, 
    SWITCH = 22, CASE = 23, DEFAULT = 24, SPAWN = 25, AWAIT = 26, LOCK = 27, 
    EXTEND = 28, TRY = 29, CATCH = 30, FINALLY = 31, THROW = 32, DEFER = 33, 
    MATCH = 34, OR = 35, EXTERN = 36, ASM = 37, VOLATILE = 38, GOTO = 39, 
    INTEL = 40, AUTO = 41, VEC = 42, MAP = 43, SET = 44, TUPLE = 45, ARROW = 46, 
    INCLUDE_SYS = 47, INCLUDE_LOCAL = 48, INLINE_BLOCK = 49, SCOPE_BLOCK = 50, 
    NULLCOAL = 51, SPREAD = 52, RANGE_INCL = 53, RANGE = 54, INT1 = 55, 
    INT8 = 56, INT16 = 57, INT32 = 58, INT64 = 59, INT128 = 60, INTINF = 61, 
    ISIZE = 62, UINT1 = 63, UINT8 = 64, UINT16 = 65, UINT32 = 66, UINT64 = 67, 
    UINT128 = 68, USIZE = 69, FLOAT32 = 70, FLOAT64 = 71, FLOAT80 = 72, 
    FLOAT128 = 73, DOUBLE = 74, BOOL = 75, CHAR = 76, VOID = 77, STRING = 78, 
    CSTRING = 79, SUFFIXED_HEX = 80, SUFFIXED_OCT = 81, SUFFIXED_BIN = 82, 
    SUFFIXED_INT = 83, SUFFIXED_INT_FLOAT = 84, SUFFIXED_FLOAT = 85, SUFFIXED_FLOAT_INT = 86, 
    SUFFIXED_DOT_FLOAT = 87, HEX_LIT = 88, OCT_LIT = 89, BIN_LIT = 90, INT_LIT = 91, 
    FLOAT_LIT = 92, BOOL_LIT = 93, C_STR_LIT = 94, STR_LIT = 95, CHAR_LIT = 96, 
    ATTR_ERROR = 97, WILDCARD = 98, IDENTIFIER = 99, PLUS_ASSIGN = 100, 
    MINUS_ASSIGN = 101, STAR_ASSIGN = 102, SLASH_ASSIGN = 103, PERCENT_ASSIGN = 104, 
    AMP_ASSIGN = 105, PIPE_ASSIGN = 106, CARET_ASSIGN = 107, LSHIFT_ASSIGN = 108, 
    RSHIFT_ASSIGN = 109, SEMI = 110, COLON = 111, SCOPE = 112, COMMA = 113, 
    DOT = 114, ASSIGN = 115, LPAREN = 116, RPAREN = 117, LBRACE = 118, RBRACE = 119, 
    LBRACKET = 120, RBRACKET = 121, STAR = 122, AMPERSAND = 123, MINUS = 124, 
    PLUS = 125, SLASH = 126, PERCENT = 127, EQ = 128, NEQ = 129, LTE = 130, 
    GTE = 131, LT = 132, GT = 133, LAND = 134, LOR = 135, NOT = 136, INCR = 137, 
    DECR = 138, LSHIFT = 139, PIPE = 140, CARET = 141, TILDE = 142, QUESTION = 143, 
    WS = 144, LINE_COMMENT = 145, BLOCK_COMMENT = 146
  };

  enum {
    RuleProgram = 0, RulePreambleDecl = 1, RuleUseDecl = 2, RuleModulePath = 3, 
    RuleIncludeDecl = 4, RuleTopLevelDecl = 5, RuleTypeAliasDecl = 6, RuleEnumDecl = 7, 
    RuleEnumVariant = 8, RuleEnumPayloadField = 9, RuleStructDecl = 10, 
    RuleStructField = 11, RuleUnionDecl = 12, RuleUnionField = 13, RuleExternDecl = 14, 
    RuleExternParamList = 15, RuleExternParam = 16, RuleFunctionDecl = 17, 
    RuleExtendDecl = 18, RuleTypeParamList = 19, RuleTypeParam = 20, RuleExtendMethod = 21, 
    RuleParamList = 22, RuleParam = 23, RuleBlock = 24, RuleStatement = 25, 
    RuleDeferStmt = 26, RuleNakedBlockStmt = 27, RuleInlineBlockStmt = 28, 
    RuleScopeBlockStmt = 29, RuleScopeCallbackList = 30, RuleScopeCallback = 31, 
    RuleLabelDef = 32, RuleAsmStmt = 33, RuleAsmOutputList = 34, RuleAsmInputList = 35, 
    RuleAsmClobberList = 36, RuleAsmGotoLabelList = 37, RuleAsmOutput = 38, 
    RuleAsmOperand = 39, RuleExprStmt = 40, RuleVarDeclStmt = 41, RuleAssignStmt = 42, 
    RuleCompoundAssignStmt = 43, RuleFieldAssignStmt = 44, RuleFieldCompoundAssignStmt = 45, 
    RuleIndexFieldAssignStmt = 46, RuleFieldIndexAssignStmt = 47, RuleDerefAssignStmt = 48, 
    RuleDerefCompoundAssignStmt = 49, RuleArrowAssignStmt = 50, RuleArrowCompoundAssignStmt = 51, 
    RuleArrowAnyAssignStmt = 52, RuleArrowAnyCompoundAssignStmt = 53, RuleCallStmt = 54, 
    RuleArgList = 55, RuleReturnStmt = 56, RuleIfStmt = 57, RuleElseIfClause = 58, 
    RuleElseClause = 59, RuleIfBody = 60, RuleForStmt = 61, RuleBreakStmt = 62, 
    RuleContinueStmt = 63, RuleLoopStmt = 64, RuleWhileStmt = 65, RuleDoWhileStmt = 66, 
    RuleLockStmt = 67, RuleTryCatchStmt = 68, RuleCatchClause = 69, RuleFinallyClause = 70, 
    RuleThrowStmt = 71, RuleSwitchStmt = 72, RuleCaseClause = 73, RuleDefaultClause = 74, 
    RuleMatchArm = 75, RulePattern = 76, RuleLiteralPattern = 77, RuleExpression = 78, 
    RuleTypeSpec = 79, RuleFnTypeSpec = 80, RulePrimitiveType = 81
  };

  explicit LucisParser(antlr4::TokenStream *input);

  LucisParser(antlr4::TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options);

  ~LucisParser() override;

  std::string getGrammarFileName() const override;

  const antlr4::atn::ATN& getATN() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;


  class ProgramContext;
  class PreambleDeclContext;
  class UseDeclContext;
  class ModulePathContext;
  class IncludeDeclContext;
  class TopLevelDeclContext;
  class TypeAliasDeclContext;
  class EnumDeclContext;
  class EnumVariantContext;
  class EnumPayloadFieldContext;
  class StructDeclContext;
  class StructFieldContext;
  class UnionDeclContext;
  class UnionFieldContext;
  class ExternDeclContext;
  class ExternParamListContext;
  class ExternParamContext;
  class FunctionDeclContext;
  class ExtendDeclContext;
  class TypeParamListContext;
  class TypeParamContext;
  class ExtendMethodContext;
  class ParamListContext;
  class ParamContext;
  class BlockContext;
  class StatementContext;
  class DeferStmtContext;
  class NakedBlockStmtContext;
  class InlineBlockStmtContext;
  class ScopeBlockStmtContext;
  class ScopeCallbackListContext;
  class ScopeCallbackContext;
  class LabelDefContext;
  class AsmStmtContext;
  class AsmOutputListContext;
  class AsmInputListContext;
  class AsmClobberListContext;
  class AsmGotoLabelListContext;
  class AsmOutputContext;
  class AsmOperandContext;
  class ExprStmtContext;
  class VarDeclStmtContext;
  class AssignStmtContext;
  class CompoundAssignStmtContext;
  class FieldAssignStmtContext;
  class FieldCompoundAssignStmtContext;
  class IndexFieldAssignStmtContext;
  class FieldIndexAssignStmtContext;
  class DerefAssignStmtContext;
  class DerefCompoundAssignStmtContext;
  class ArrowAssignStmtContext;
  class ArrowCompoundAssignStmtContext;
  class ArrowAnyAssignStmtContext;
  class ArrowAnyCompoundAssignStmtContext;
  class CallStmtContext;
  class ArgListContext;
  class ReturnStmtContext;
  class IfStmtContext;
  class ElseIfClauseContext;
  class ElseClauseContext;
  class IfBodyContext;
  class ForStmtContext;
  class BreakStmtContext;
  class ContinueStmtContext;
  class LoopStmtContext;
  class WhileStmtContext;
  class DoWhileStmtContext;
  class LockStmtContext;
  class TryCatchStmtContext;
  class CatchClauseContext;
  class FinallyClauseContext;
  class ThrowStmtContext;
  class SwitchStmtContext;
  class CaseClauseContext;
  class DefaultClauseContext;
  class MatchArmContext;
  class PatternContext;
  class LiteralPatternContext;
  class ExpressionContext;
  class TypeSpecContext;
  class FnTypeSpecContext;
  class PrimitiveTypeContext; 

  class  ProgramContext : public antlr4::ParserRuleContext {
  public:
    ProgramContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EOF();
    std::vector<PreambleDeclContext *> preambleDecl();
    PreambleDeclContext* preambleDecl(size_t i);
    std::vector<TopLevelDeclContext *> topLevelDecl();
    TopLevelDeclContext* topLevelDecl(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ProgramContext* program();

  class  PreambleDeclContext : public antlr4::ParserRuleContext {
  public:
    PreambleDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    UseDeclContext *useDecl();
    IncludeDeclContext *includeDecl();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  PreambleDeclContext* preambleDecl();

  class  UseDeclContext : public antlr4::ParserRuleContext {
  public:
    UseDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    UseDeclContext() = default;
    void copyFrom(UseDeclContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  UseItemContext : public UseDeclContext {
  public:
    UseItemContext(UseDeclContext *ctx);

    antlr4::tree::TerminalNode *USE();
    ModulePathContext *modulePath();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  UseRootContext : public UseDeclContext {
  public:
    UseRootContext(UseDeclContext *ctx);

    antlr4::tree::TerminalNode *USE();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  UseGroupContext : public UseDeclContext {
  public:
    UseGroupContext(UseDeclContext *ctx);

    antlr4::tree::TerminalNode *USE();
    ModulePathContext *modulePath();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  UseEnumWildcardContext : public UseDeclContext {
  public:
    UseEnumWildcardContext(UseDeclContext *ctx);

    antlr4::tree::TerminalNode *USE();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  UseDeclContext* useDecl();

  class  ModulePathContext : public antlr4::ParserRuleContext {
  public:
    ModulePathContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    std::vector<antlr4::tree::TerminalNode *> SCOPE();
    antlr4::tree::TerminalNode* SCOPE(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ModulePathContext* modulePath();

  class  IncludeDeclContext : public antlr4::ParserRuleContext {
  public:
    IncludeDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INCLUDE_SYS();
    antlr4::tree::TerminalNode *INCLUDE_LOCAL();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  IncludeDeclContext* includeDecl();

  class  TopLevelDeclContext : public antlr4::ParserRuleContext {
  public:
    TopLevelDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    UseDeclContext *useDecl();
    IncludeDeclContext *includeDecl();
    TypeAliasDeclContext *typeAliasDecl();
    StructDeclContext *structDecl();
    UnionDeclContext *unionDecl();
    EnumDeclContext *enumDecl();
    ExtendDeclContext *extendDecl();
    ExternDeclContext *externDecl();
    FunctionDeclContext *functionDecl();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TopLevelDeclContext* topLevelDecl();

  class  TypeAliasDeclContext : public antlr4::ParserRuleContext {
  public:
    TypeAliasDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *TYPE();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *ASSIGN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeAliasDeclContext* typeAliasDecl();

  class  EnumDeclContext : public antlr4::ParserRuleContext {
  public:
    EnumDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *ENUM();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<EnumVariantContext *> enumVariant();
    EnumVariantContext* enumVariant(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    TypeParamListContext *typeParamList();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  EnumDeclContext* enumDecl();

  class  EnumVariantContext : public antlr4::ParserRuleContext {
  public:
    EnumVariantContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *ATTR_ERROR();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<EnumPayloadFieldContext *> enumPayloadField();
    EnumPayloadFieldContext* enumPayloadField(size_t i);
    antlr4::tree::TerminalNode *RBRACE();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  EnumVariantContext* enumVariant();

  class  EnumPayloadFieldContext : public antlr4::ParserRuleContext {
  public:
    EnumPayloadFieldContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *COLON();
    TypeSpecContext *typeSpec();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  EnumPayloadFieldContext* enumPayloadField();

  class  StructDeclContext : public antlr4::ParserRuleContext {
  public:
    StructDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STRUCT();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    TypeParamListContext *typeParamList();
    antlr4::tree::TerminalNode *COLON();
    std::vector<StructFieldContext *> structField();
    StructFieldContext* structField(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  StructDeclContext* structDecl();

  class  StructFieldContext : public antlr4::ParserRuleContext {
  public:
    StructFieldContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  StructFieldContext* structField();

  class  UnionDeclContext : public antlr4::ParserRuleContext {
  public:
    UnionDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *UNION();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    TypeParamListContext *typeParamList();
    std::vector<UnionFieldContext *> unionField();
    UnionFieldContext* unionField(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  UnionDeclContext* unionDecl();

  class  UnionFieldContext : public antlr4::ParserRuleContext {
  public:
    UnionFieldContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  UnionFieldContext* unionField();

  class  ExternDeclContext : public antlr4::ParserRuleContext {
  public:
    ExternDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EXTERN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *SEMI();
    ExternParamListContext *externParamList();
    antlr4::tree::TerminalNode *COMMA();
    antlr4::tree::TerminalNode *SPREAD();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExternDeclContext* externDecl();

  class  ExternParamListContext : public antlr4::ParserRuleContext {
  public:
    ExternParamListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ExternParamContext *> externParam();
    ExternParamContext* externParam(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExternParamListContext* externParamList();

  class  ExternParamContext : public antlr4::ParserRuleContext {
  public:
    ExternParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExternParamContext* externParam();

  class  FunctionDeclContext : public antlr4::ParserRuleContext {
  public:
    FunctionDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *FN();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    TypeSpecContext *typeSpec();
    BlockContext *block();
    TypeParamListContext *typeParamList();
    ParamListContext *paramList();
    antlr4::tree::TerminalNode *SCOPE();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FunctionDeclContext* functionDecl();

  class  ExtendDeclContext : public antlr4::ParserRuleContext {
  public:
    ExtendDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EXTEND();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    TypeParamListContext *typeParamList();
    std::vector<ExtendMethodContext *> extendMethod();
    ExtendMethodContext* extendMethod(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExtendDeclContext* extendDecl();

  class  TypeParamListContext : public antlr4::ParserRuleContext {
  public:
    TypeParamListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeParamContext *> typeParam();
    TypeParamContext* typeParam(size_t i);
    antlr4::tree::TerminalNode *GT();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeParamListContext* typeParamList();

  class  TypeParamContext : public antlr4::ParserRuleContext {
  public:
    TypeParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *COLON();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeParamContext* typeParam();

  class  ExtendMethodContext : public antlr4::ParserRuleContext {
  public:
    ExtendMethodContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *FN();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *AMPERSAND();
    antlr4::tree::TerminalNode *RPAREN();
    TypeSpecContext *typeSpec();
    BlockContext *block();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    std::vector<ParamContext *> param();
    ParamContext* param(size_t i);
    ParamListContext *paramList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExtendMethodContext* extendMethod();

  class  ParamListContext : public antlr4::ParserRuleContext {
  public:
    ParamListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ParamContext *> param();
    ParamContext* param(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ParamListContext* paramList();

  class  ParamContext : public antlr4::ParserRuleContext {
  public:
    ParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *SPREAD();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ParamContext* param();

  class  BlockContext : public antlr4::ParserRuleContext {
  public:
    BlockContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  BlockContext* block();

  class  StatementContext : public antlr4::ParserRuleContext {
  public:
    StatementContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    LabelDefContext *labelDef();
    UseDeclContext *useDecl();
    VarDeclStmtContext *varDeclStmt();
    AssignStmtContext *assignStmt();
    CompoundAssignStmtContext *compoundAssignStmt();
    DerefAssignStmtContext *derefAssignStmt();
    DerefCompoundAssignStmtContext *derefCompoundAssignStmt();
    FieldIndexAssignStmtContext *fieldIndexAssignStmt();
    IndexFieldAssignStmtContext *indexFieldAssignStmt();
    FieldAssignStmtContext *fieldAssignStmt();
    FieldCompoundAssignStmtContext *fieldCompoundAssignStmt();
    ArrowAssignStmtContext *arrowAssignStmt();
    ArrowCompoundAssignStmtContext *arrowCompoundAssignStmt();
    ArrowAnyAssignStmtContext *arrowAnyAssignStmt();
    ArrowAnyCompoundAssignStmtContext *arrowAnyCompoundAssignStmt();
    CallStmtContext *callStmt();
    ExprStmtContext *exprStmt();
    ReturnStmtContext *returnStmt();
    IfStmtContext *ifStmt();
    ForStmtContext *forStmt();
    LoopStmtContext *loopStmt();
    WhileStmtContext *whileStmt();
    DoWhileStmtContext *doWhileStmt();
    BreakStmtContext *breakStmt();
    ContinueStmtContext *continueStmt();
    SwitchStmtContext *switchStmt();
    LockStmtContext *lockStmt();
    TryCatchStmtContext *tryCatchStmt();
    ThrowStmtContext *throwStmt();
    DeferStmtContext *deferStmt();
    NakedBlockStmtContext *nakedBlockStmt();
    InlineBlockStmtContext *inlineBlockStmt();
    ScopeBlockStmtContext *scopeBlockStmt();
    AsmStmtContext *asmStmt();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  StatementContext* statement();

  class  DeferStmtContext : public antlr4::ParserRuleContext {
  public:
    DeferStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *DEFER();
    CallStmtContext *callStmt();
    ExprStmtContext *exprStmt();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  DeferStmtContext* deferStmt();

  class  NakedBlockStmtContext : public antlr4::ParserRuleContext {
  public:
    NakedBlockStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  NakedBlockStmtContext* nakedBlockStmt();

  class  InlineBlockStmtContext : public antlr4::ParserRuleContext {
  public:
    InlineBlockStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INLINE_BLOCK();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  InlineBlockStmtContext* inlineBlockStmt();

  class  ScopeBlockStmtContext : public antlr4::ParserRuleContext {
  public:
    ScopeBlockStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *SCOPE_BLOCK();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    ScopeCallbackListContext *scopeCallbackList();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ScopeBlockStmtContext* scopeBlockStmt();

  class  ScopeCallbackListContext : public antlr4::ParserRuleContext {
  public:
    ScopeCallbackListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ScopeCallbackContext *> scopeCallback();
    ScopeCallbackContext* scopeCallback(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ScopeCallbackListContext* scopeCallbackList();

  class  ScopeCallbackContext : public antlr4::ParserRuleContext {
  public:
    ScopeCallbackContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *DOT();
    ArgListContext *argList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ScopeCallbackContext* scopeCallback();

  class  LabelDefContext : public antlr4::ParserRuleContext {
  public:
    LabelDefContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *COLON();
    StatementContext *statement();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LabelDefContext* labelDef();

  class  AsmStmtContext : public antlr4::ParserRuleContext {
  public:
    AsmStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *ASM();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<antlr4::tree::TerminalNode *> STR_LIT();
    antlr4::tree::TerminalNode* STR_LIT(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *VOLATILE();
    antlr4::tree::TerminalNode *GOTO();
    antlr4::tree::TerminalNode *INTEL();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    AsmOutputListContext *asmOutputList();
    AsmInputListContext *asmInputList();
    AsmClobberListContext *asmClobberList();
    AsmGotoLabelListContext *asmGotoLabelList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmStmtContext* asmStmt();

  class  AsmOutputListContext : public antlr4::ParserRuleContext {
  public:
    AsmOutputListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<AsmOutputContext *> asmOutput();
    AsmOutputContext* asmOutput(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmOutputListContext* asmOutputList();

  class  AsmInputListContext : public antlr4::ParserRuleContext {
  public:
    AsmInputListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<AsmOperandContext *> asmOperand();
    AsmOperandContext* asmOperand(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmInputListContext* asmInputList();

  class  AsmClobberListContext : public antlr4::ParserRuleContext {
  public:
    AsmClobberListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> STR_LIT();
    antlr4::tree::TerminalNode* STR_LIT(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmClobberListContext* asmClobberList();

  class  AsmGotoLabelListContext : public antlr4::ParserRuleContext {
  public:
    AsmGotoLabelListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmGotoLabelListContext* asmGotoLabelList();

  class  AsmOutputContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *constraint = nullptr;
    AsmOutputContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STR_LIT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmOutputContext* asmOutput();

  class  AsmOperandContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *constraint = nullptr;
    AsmOperandContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *STR_LIT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AsmOperandContext* asmOperand();

  class  ExprStmtContext : public antlr4::ParserRuleContext {
  public:
    ExprStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExprStmtContext* exprStmt();

  class  VarDeclStmtContext : public antlr4::ParserRuleContext {
  public:
    VarDeclStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *ASSIGN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *SCOPE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  VarDeclStmtContext* varDeclStmt();

  class  AssignStmtContext : public antlr4::ParserRuleContext {
  public:
    AssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> LBRACKET();
    antlr4::tree::TerminalNode* LBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> RBRACKET();
    antlr4::tree::TerminalNode* RBRACKET(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AssignStmtContext* assignStmt();

  class  CompoundAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *op = nullptr;
    CompoundAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *PLUS_ASSIGN();
    antlr4::tree::TerminalNode *MINUS_ASSIGN();
    antlr4::tree::TerminalNode *STAR_ASSIGN();
    antlr4::tree::TerminalNode *SLASH_ASSIGN();
    antlr4::tree::TerminalNode *PERCENT_ASSIGN();
    antlr4::tree::TerminalNode *AMP_ASSIGN();
    antlr4::tree::TerminalNode *PIPE_ASSIGN();
    antlr4::tree::TerminalNode *CARET_ASSIGN();
    antlr4::tree::TerminalNode *LSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *RSHIFT_ASSIGN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  CompoundAssignStmtContext* compoundAssignStmt();

  class  FieldAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    FieldAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ASSIGN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FieldAssignStmtContext* fieldAssignStmt();

  class  FieldCompoundAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *op = nullptr;
    FieldCompoundAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *PLUS_ASSIGN();
    antlr4::tree::TerminalNode *MINUS_ASSIGN();
    antlr4::tree::TerminalNode *STAR_ASSIGN();
    antlr4::tree::TerminalNode *SLASH_ASSIGN();
    antlr4::tree::TerminalNode *PERCENT_ASSIGN();
    antlr4::tree::TerminalNode *AMP_ASSIGN();
    antlr4::tree::TerminalNode *PIPE_ASSIGN();
    antlr4::tree::TerminalNode *CARET_ASSIGN();
    antlr4::tree::TerminalNode *LSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *RSHIFT_ASSIGN();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FieldCompoundAssignStmtContext* fieldCompoundAssignStmt();

  class  IndexFieldAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    IndexFieldAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> LBRACKET();
    antlr4::tree::TerminalNode* LBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> RBRACKET();
    antlr4::tree::TerminalNode* RBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  IndexFieldAssignStmtContext* indexFieldAssignStmt();

  class  FieldIndexAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    FieldIndexAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);
    std::vector<antlr4::tree::TerminalNode *> LBRACKET();
    antlr4::tree::TerminalNode* LBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> RBRACKET();
    antlr4::tree::TerminalNode* RBRACKET(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FieldIndexAssignStmtContext* fieldIndexAssignStmt();

  class  DerefAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    DerefAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  DerefAssignStmtContext* derefAssignStmt();

  class  DerefCompoundAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *op = nullptr;
    DerefCompoundAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *IDENTIFIER();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *PLUS_ASSIGN();
    antlr4::tree::TerminalNode *MINUS_ASSIGN();
    antlr4::tree::TerminalNode *STAR_ASSIGN();
    antlr4::tree::TerminalNode *SLASH_ASSIGN();
    antlr4::tree::TerminalNode *PERCENT_ASSIGN();
    antlr4::tree::TerminalNode *AMP_ASSIGN();
    antlr4::tree::TerminalNode *PIPE_ASSIGN();
    antlr4::tree::TerminalNode *CARET_ASSIGN();
    antlr4::tree::TerminalNode *LSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *RSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  DerefCompoundAssignStmtContext* derefCompoundAssignStmt();

  class  ArrowAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    ArrowAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ARROW();
    antlr4::tree::TerminalNode *ASSIGN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrowAssignStmtContext* arrowAssignStmt();

  class  ArrowCompoundAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *op = nullptr;
    ArrowCompoundAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ARROW();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *PLUS_ASSIGN();
    antlr4::tree::TerminalNode *MINUS_ASSIGN();
    antlr4::tree::TerminalNode *STAR_ASSIGN();
    antlr4::tree::TerminalNode *SLASH_ASSIGN();
    antlr4::tree::TerminalNode *PERCENT_ASSIGN();
    antlr4::tree::TerminalNode *AMP_ASSIGN();
    antlr4::tree::TerminalNode *PIPE_ASSIGN();
    antlr4::tree::TerminalNode *CARET_ASSIGN();
    antlr4::tree::TerminalNode *LSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *RSHIFT_ASSIGN();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrowCompoundAssignStmtContext* arrowCompoundAssignStmt();

  class  ArrowAnyAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    ArrowAnyAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);
    std::vector<antlr4::tree::TerminalNode *> ARROW();
    antlr4::tree::TerminalNode* ARROW(size_t i);
    std::vector<antlr4::tree::TerminalNode *> LBRACKET();
    antlr4::tree::TerminalNode* LBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> RBRACKET();
    antlr4::tree::TerminalNode* RBRACKET(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrowAnyAssignStmtContext* arrowAnyAssignStmt();

  class  ArrowAnyCompoundAssignStmtContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *op = nullptr;
    ArrowAnyCompoundAssignStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *PLUS_ASSIGN();
    antlr4::tree::TerminalNode *MINUS_ASSIGN();
    antlr4::tree::TerminalNode *STAR_ASSIGN();
    antlr4::tree::TerminalNode *SLASH_ASSIGN();
    antlr4::tree::TerminalNode *PERCENT_ASSIGN();
    antlr4::tree::TerminalNode *AMP_ASSIGN();
    antlr4::tree::TerminalNode *PIPE_ASSIGN();
    antlr4::tree::TerminalNode *CARET_ASSIGN();
    antlr4::tree::TerminalNode *LSHIFT_ASSIGN();
    antlr4::tree::TerminalNode *RSHIFT_ASSIGN();
    std::vector<antlr4::tree::TerminalNode *> DOT();
    antlr4::tree::TerminalNode* DOT(size_t i);
    std::vector<antlr4::tree::TerminalNode *> ARROW();
    antlr4::tree::TerminalNode* ARROW(size_t i);
    std::vector<antlr4::tree::TerminalNode *> LBRACKET();
    antlr4::tree::TerminalNode* LBRACKET(size_t i);
    std::vector<antlr4::tree::TerminalNode *> RBRACKET();
    antlr4::tree::TerminalNode* RBRACKET(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrowAnyCompoundAssignStmtContext* arrowAnyCompoundAssignStmt();

  class  CallStmtContext : public antlr4::ParserRuleContext {
  public:
    CallStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *SEMI();
    ArgListContext *argList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  CallStmtContext* callStmt();

  class  ArgListContext : public antlr4::ParserRuleContext {
  public:
    ArgListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArgListContext* argList();

  class  ReturnStmtContext : public antlr4::ParserRuleContext {
  public:
    ReturnStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *RET();
    antlr4::tree::TerminalNode *SEMI();
    ExpressionContext *expression();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ReturnStmtContext* returnStmt();

  class  IfStmtContext : public antlr4::ParserRuleContext {
  public:
    IfStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IF();
    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();
    IfBodyContext *ifBody();
    std::vector<ElseIfClauseContext *> elseIfClause();
    ElseIfClauseContext* elseIfClause(size_t i);
    ElseClauseContext *elseClause();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  IfStmtContext* ifStmt();

  class  ElseIfClauseContext : public antlr4::ParserRuleContext {
  public:
    ElseIfClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *ELSE();
    antlr4::tree::TerminalNode *IF();
    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();
    IfBodyContext *ifBody();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ElseIfClauseContext* elseIfClause();

  class  ElseClauseContext : public antlr4::ParserRuleContext {
  public:
    ElseClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *ELSE();
    IfBodyContext *ifBody();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ElseClauseContext* elseClause();

  class  IfBodyContext : public antlr4::ParserRuleContext {
  public:
    IfBodyContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    BlockContext *block();
    StatementContext *statement();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  IfBodyContext* ifBody();

  class  ForStmtContext : public antlr4::ParserRuleContext {
  public:
    ForStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    ForStmtContext() = default;
    void copyFrom(ForStmtContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  ForInStmtContext : public ForStmtContext {
  public:
    ForInStmtContext(ForStmtContext *ctx);

    antlr4::tree::TerminalNode *FOR();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *IN();
    ExpressionContext *expression();
    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ForClassicStmtContext : public ForStmtContext {
  public:
    ForClassicStmtContext(ForStmtContext *ctx);

    antlr4::tree::TerminalNode *FOR();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> SEMI();
    antlr4::tree::TerminalNode* SEMI(size_t i);
    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ForStmtContext* forStmt();

  class  BreakStmtContext : public antlr4::ParserRuleContext {
  public:
    BreakStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *BREAK();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  BreakStmtContext* breakStmt();

  class  ContinueStmtContext : public antlr4::ParserRuleContext {
  public:
    ContinueStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *CONTINUE();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ContinueStmtContext* continueStmt();

  class  LoopStmtContext : public antlr4::ParserRuleContext {
  public:
    LoopStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LOOP();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LoopStmtContext* loopStmt();

  class  WhileStmtContext : public antlr4::ParserRuleContext {
  public:
    WhileStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *WHILE();
    ExpressionContext *expression();
    BlockContext *block();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  WhileStmtContext* whileStmt();

  class  DoWhileStmtContext : public antlr4::ParserRuleContext {
  public:
    DoWhileStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *DO();
    BlockContext *block();
    antlr4::tree::TerminalNode *WHILE();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  DoWhileStmtContext* doWhileStmt();

  class  LockStmtContext : public antlr4::ParserRuleContext {
  public:
    LockStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LOCK();
    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LockStmtContext* lockStmt();

  class  TryCatchStmtContext : public antlr4::ParserRuleContext {
  public:
    TryCatchStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *TRY();
    BlockContext *block();
    std::vector<CatchClauseContext *> catchClause();
    CatchClauseContext* catchClause(size_t i);
    FinallyClauseContext *finallyClause();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TryCatchStmtContext* tryCatchStmt();

  class  CatchClauseContext : public antlr4::ParserRuleContext {
  public:
    CatchClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *CATCH();
    antlr4::tree::TerminalNode *LPAREN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *RPAREN();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  CatchClauseContext* catchClause();

  class  FinallyClauseContext : public antlr4::ParserRuleContext {
  public:
    FinallyClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *FINALLY();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FinallyClauseContext* finallyClause();

  class  ThrowStmtContext : public antlr4::ParserRuleContext {
  public:
    ThrowStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *THROW();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *SEMI();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ThrowStmtContext* throwStmt();

  class  SwitchStmtContext : public antlr4::ParserRuleContext {
  public:
    SwitchStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *SWITCH();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<CaseClauseContext *> caseClause();
    CaseClauseContext* caseClause(size_t i);
    DefaultClauseContext *defaultClause();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  SwitchStmtContext* switchStmt();

  class  CaseClauseContext : public antlr4::ParserRuleContext {
  public:
    CaseClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *CASE();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    BlockContext *block();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  CaseClauseContext* caseClause();

  class  DefaultClauseContext : public antlr4::ParserRuleContext {
  public:
    DefaultClauseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *DEFAULT();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  DefaultClauseContext* defaultClause();

  class  MatchArmContext : public antlr4::ParserRuleContext {
  public:
    MatchArmContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<PatternContext *> pattern();
    PatternContext* pattern(size_t i);
    antlr4::tree::TerminalNode *ARROW();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> PIPE();
    antlr4::tree::TerminalNode* PIPE(size_t i);
    antlr4::tree::TerminalNode *IF();
    BlockContext *block();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  MatchArmContext* matchArm();

  class  PatternContext : public antlr4::ParserRuleContext {
  public:
    PatternContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *WILDCARD();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    LiteralPatternContext *literalPattern();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  PatternContext* pattern();

  class  LiteralPatternContext : public antlr4::ParserRuleContext {
  public:
    LiteralPatternContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INT_LIT();
    antlr4::tree::TerminalNode *HEX_LIT();
    antlr4::tree::TerminalNode *OCT_LIT();
    antlr4::tree::TerminalNode *BIN_LIT();
    antlr4::tree::TerminalNode *FLOAT_LIT();
    antlr4::tree::TerminalNode *STR_LIT();
    antlr4::tree::TerminalNode *C_STR_LIT();
    antlr4::tree::TerminalNode *CHAR_LIT();
    antlr4::tree::TerminalNode *NULL_LIT();
    antlr4::tree::TerminalNode *BOOL_LIT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LiteralPatternContext* literalPattern();

  class  ExpressionContext : public antlr4::ParserRuleContext {
  public:
    ExpressionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    ExpressionContext() = default;
    void copyFrom(ExpressionContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  StructPosInitExprContext : public ExpressionContext {
  public:
    StructPosInitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedOctLitExprContext : public ExpressionContext {
  public:
    SuffixedOctLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_OCT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  FieldAccessExprContext : public ExpressionContext {
  public:
    FieldAccessExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *DOT();
    antlr4::tree::TerminalNode *IDENTIFIER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TypeofExprContext : public ExpressionContext {
  public:
    TypeofExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *TYPEOF();
    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericEnumNamedVariantExprContext : public ExpressionContext {
  public:
    GenericEnumNamedVariantExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  RshiftExprContext : public ExpressionContext {
  public:
    RshiftExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> GT();
    antlr4::tree::TerminalNode* GT(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ArrowMethodCallExprContext : public ExpressionContext {
  public:
    ArrowMethodCallExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *ARROW();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AsmExprContext : public ExpressionContext {
  public:
    AsmExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *ASM();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<antlr4::tree::TerminalNode *> STR_LIT();
    antlr4::tree::TerminalNode* STR_LIT(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *VOLATILE();
    antlr4::tree::TerminalNode *INTEL();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    AsmOutputListContext *asmOutputList();
    AsmInputListContext *asmInputList();
    AsmClobberListContext *asmClobberList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  OctLitExprContext : public ExpressionContext {
  public:
    OctLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *OCT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitXorExprContext : public ExpressionContext {
  public:
    BitXorExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *CARET();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LogicalNotExprContext : public ExpressionContext {
  public:
    LogicalNotExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *NOT();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IdentExprContext : public ExpressionContext {
  public:
    IdentExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *IDENTIFIER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PreIncrExprContext : public ExpressionContext {
  public:
    PreIncrExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *INCR();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TernaryExprContext : public ExpressionContext {
  public:
    TernaryExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *QUESTION();
    antlr4::tree::TerminalNode *COLON();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedIntFloatExprContext : public ExpressionContext {
  public:
    SuffixedIntFloatExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_INT_FLOAT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ChainedTupleIndexExprContext : public ExpressionContext {
  public:
    ChainedTupleIndexExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *DOT();
    antlr4::tree::TerminalNode *FLOAT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NullLitExprContext : public ExpressionContext {
  public:
    NullLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *NULL_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  MulExprContext : public ExpressionContext {
  public:
    MulExprContext(ExpressionContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *SLASH();
    antlr4::tree::TerminalNode *PERCENT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedLeadingDotFloatExprContext : public ExpressionContext {
  public:
    SuffixedLeadingDotFloatExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_DOT_FLOAT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitAndExprContext : public ExpressionContext {
  public:
    BitAndExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *AMPERSAND();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IsExprContext : public ExpressionContext {
  public:
    IsExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *IS();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *SCOPE();
    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LshiftExprContext : public ExpressionContext {
  public:
    LshiftExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *LSHIFT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TupleLitExprContext : public ExpressionContext {
  public:
    TupleLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PropagateExprContext : public ExpressionContext {
  public:
    PropagateExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *QUESTION();
    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedHexLitExprContext : public ExpressionContext {
  public:
    SuffixedHexLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_HEX();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedFloatIntExprContext : public ExpressionContext {
  public:
    SuffixedFloatIntExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_FLOAT_INT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AddSubExprContext : public ExpressionContext {
  public:
    AddSubExprContext(ExpressionContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *PLUS();
    antlr4::tree::TerminalNode *MINUS();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedFloatLitExprContext : public ExpressionContext {
  public:
    SuffixedFloatLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_FLOAT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IntLitExprContext : public ExpressionContext {
  public:
    IntLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *INT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AddrOfExprContext : public ExpressionContext {
  public:
    AddrOfExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *AMPERSAND();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedBinLitExprContext : public ExpressionContext {
  public:
    SuffixedBinLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_BIN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TupleIndexExprContext : public ExpressionContext {
  public:
    TupleIndexExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *DOT();
    antlr4::tree::TerminalNode *INT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  FloatLitExprContext : public ExpressionContext {
  public:
    FloatLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *FLOAT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericStructLitExprContext : public ExpressionContext {
  public:
    GenericStructLitExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SpawnExprContext : public ExpressionContext {
  public:
    SpawnExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SPAWN();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ArrowAccessExprContext : public ExpressionContext {
  public:
    ArrowAccessExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *ARROW();
    antlr4::tree::TerminalNode *IDENTIFIER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericStaticMethodCallExprContext : public ExpressionContext {
  public:
    GenericStaticMethodCallExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ListCompExprContext : public ExpressionContext {
  public:
    ListCompExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *LBRACKET();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *PIPE();
    antlr4::tree::TerminalNode *FOR();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *IN();
    antlr4::tree::TerminalNode *RBRACKET();
    antlr4::tree::TerminalNode *IF();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IndexExprContext : public ExpressionContext {
  public:
    IndexExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *LBRACKET();
    antlr4::tree::TerminalNode *RBRACKET();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NegExprContext : public ExpressionContext {
  public:
    NegExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *MINUS();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  DerefExprContext : public ExpressionContext {
  public:
    DerefExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *STAR();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PreDecrExprContext : public ExpressionContext {
  public:
    PreDecrExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *DECR();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SpreadExprContext : public ExpressionContext {
  public:
    SpreadExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SPREAD();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CatchUnwrapExprContext : public ExpressionContext {
  public:
    CatchUnwrapExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *CATCH();
    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  StaticMethodCallExprContext : public ExpressionContext {
  public:
    StaticMethodCallExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<antlr4::tree::TerminalNode *> SCOPE();
    antlr4::tree::TerminalNode* SCOPE(size_t i);
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NullCoalExprContext : public ExpressionContext {
  public:
    NullCoalExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *NULLCOAL();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericStructPosInitExprContext : public ExpressionContext {
  public:
    GenericStructPosInitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CastExprContext : public ExpressionContext {
  public:
    CastExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *AS();
    TypeSpecContext *typeSpec();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericFnCallExprContext : public ExpressionContext {
  public:
    GenericFnCallExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericEnumAccessExprContext : public ExpressionContext {
  public:
    GenericEnumAccessExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *SCOPE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  MatchExprContext : public ExpressionContext {
  public:
    MatchExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *MATCH();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<MatchArmContext *> matchArm();
    MatchArmContext* matchArm(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  EnumAccessExprContext : public ExpressionContext {
  public:
    EnumAccessExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *SCOPE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ParenExprContext : public ExpressionContext {
  public:
    ParenExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    ExpressionContext *expression();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitNotExprContext : public ExpressionContext {
  public:
    BitNotExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *TILDE();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ArrayLitExprContext : public ExpressionContext {
  public:
    ArrayLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *LBRACKET();
    antlr4::tree::TerminalNode *RBRACKET();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  MethodCallExprContext : public ExpressionContext {
  public:
    MethodCallExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *DOT();
    antlr4::tree::TerminalNode *IDENTIFIER();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LeadingDotFloatLitExprContext : public ExpressionContext {
  public:
    LeadingDotFloatLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *DOT();
    antlr4::tree::TerminalNode *INT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  StructLitExprContext : public ExpressionContext {
  public:
    StructLitExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PostDecrExprContext : public ExpressionContext {
  public:
    PostDecrExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *DECR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  RelExprContext : public ExpressionContext {
  public:
    RelExprContext(ExpressionContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *LT();
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LTE();
    antlr4::tree::TerminalNode *GTE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BinLitExprContext : public ExpressionContext {
  public:
    BinLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *BIN_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  RangeInclExprContext : public ExpressionContext {
  public:
    RangeInclExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RANGE_INCL();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TupleArrowIndexExprContext : public ExpressionContext {
  public:
    TupleArrowIndexExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *ARROW();
    antlr4::tree::TerminalNode *INT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericQualifiedFnCallExprContext : public ExpressionContext {
  public:
    GenericQualifiedFnCallExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<antlr4::tree::TerminalNode *> SCOPE();
    antlr4::tree::TerminalNode* SCOPE(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LogicalAndExprContext : public ExpressionContext {
  public:
    LogicalAndExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *LAND();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  QualifiedStructPosInitExprContext : public ExpressionContext {
  public:
    QualifiedStructPosInitExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  StrLitExprContext : public ExpressionContext {
  public:
    StrLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *STR_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AwaitExprContext : public ExpressionContext {
  public:
    AwaitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *AWAIT();
    ExpressionContext *expression();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  QualifiedStructNamedInitExprContext : public ExpressionContext {
  public:
    QualifiedStructNamedInitExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COLON();
    antlr4::tree::TerminalNode* COLON(size_t i);
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CStrLitExprContext : public ExpressionContext {
  public:
    CStrLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *C_STR_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  FnCallExprContext : public ExpressionContext {
  public:
    FnCallExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LogicalOrExprContext : public ExpressionContext {
  public:
    LogicalOrExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *LOR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SizeofExprContext : public ExpressionContext {
  public:
    SizeofExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SIZEOF();
    antlr4::tree::TerminalNode *LPAREN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  EqExprContext : public ExpressionContext {
  public:
    EqExprContext(ExpressionContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *EQ();
    antlr4::tree::TerminalNode *NEQ();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitOrExprContext : public ExpressionContext {
  public:
    BitOrExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *PIPE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CharLitExprContext : public ExpressionContext {
  public:
    CharLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *CHAR_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TryExprContext : public ExpressionContext {
  public:
    TryExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *TRY();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *OR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PostIncrExprContext : public ExpressionContext {
  public:
    PostIncrExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *INCR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GenericEnumPosVariantExprContext : public ExpressionContext {
  public:
    GenericEnumPosVariantExprContext(ExpressionContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> IDENTIFIER();
    antlr4::tree::TerminalNode* IDENTIFIER(size_t i);
    antlr4::tree::TerminalNode *LT();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *SCOPE();
    antlr4::tree::TerminalNode *LBRACE();
    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BoolLitExprContext : public ExpressionContext {
  public:
    BoolLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *BOOL_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  HexLitExprContext : public ExpressionContext {
  public:
    HexLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *HEX_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SuffixedIntLitExprContext : public ExpressionContext {
  public:
    SuffixedIntLitExprContext(ExpressionContext *ctx);

    antlr4::tree::TerminalNode *SUFFIXED_INT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ChainedTupleArrowIndexExprContext : public ExpressionContext {
  public:
    ChainedTupleArrowIndexExprContext(ExpressionContext *ctx);

    ExpressionContext *expression();
    antlr4::tree::TerminalNode *ARROW();
    antlr4::tree::TerminalNode *FLOAT_LIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  RangeExprContext : public ExpressionContext {
  public:
    RangeExprContext(ExpressionContext *ctx);

    std::vector<ExpressionContext *> expression();
    ExpressionContext* expression(size_t i);
    antlr4::tree::TerminalNode *RANGE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ExpressionContext* expression();
  ExpressionContext* expression(int precedence);
  class  TypeSpecContext : public antlr4::ParserRuleContext {
  public:
    TypeSpecContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STAR();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    antlr4::tree::TerminalNode *LBRACKET();
    antlr4::tree::TerminalNode *INT_LIT();
    antlr4::tree::TerminalNode *RBRACKET();
    FnTypeSpecContext *fnTypeSpec();
    antlr4::tree::TerminalNode *VEC();
    antlr4::tree::TerminalNode *LT();
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *MAP();
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);
    antlr4::tree::TerminalNode *SET();
    antlr4::tree::TerminalNode *TUPLE();
    antlr4::tree::TerminalNode *IDENTIFIER();
    PrimitiveTypeContext *primitiveType();
    antlr4::tree::TerminalNode *AUTO();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeSpecContext* typeSpec();
  TypeSpecContext* typeSpec(int precedence);
  class  FnTypeSpecContext : public antlr4::ParserRuleContext {
  public:
    FnTypeSpecContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *FN();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *ARROW();
    std::vector<TypeSpecContext *> typeSpec();
    TypeSpecContext* typeSpec(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FnTypeSpecContext* fnTypeSpec();

  class  PrimitiveTypeContext : public antlr4::ParserRuleContext {
  public:
    PrimitiveTypeContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INT1();
    antlr4::tree::TerminalNode *INT8();
    antlr4::tree::TerminalNode *INT16();
    antlr4::tree::TerminalNode *INT32();
    antlr4::tree::TerminalNode *INT64();
    antlr4::tree::TerminalNode *INT128();
    antlr4::tree::TerminalNode *INTINF();
    antlr4::tree::TerminalNode *ISIZE();
    antlr4::tree::TerminalNode *UINT1();
    antlr4::tree::TerminalNode *UINT8();
    antlr4::tree::TerminalNode *UINT16();
    antlr4::tree::TerminalNode *UINT32();
    antlr4::tree::TerminalNode *UINT64();
    antlr4::tree::TerminalNode *UINT128();
    antlr4::tree::TerminalNode *USIZE();
    antlr4::tree::TerminalNode *FLOAT32();
    antlr4::tree::TerminalNode *FLOAT64();
    antlr4::tree::TerminalNode *FLOAT80();
    antlr4::tree::TerminalNode *FLOAT128();
    antlr4::tree::TerminalNode *DOUBLE();
    antlr4::tree::TerminalNode *BOOL();
    antlr4::tree::TerminalNode *CHAR();
    antlr4::tree::TerminalNode *VOID();
    antlr4::tree::TerminalNode *STRING();
    antlr4::tree::TerminalNode *CSTRING();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  PrimitiveTypeContext* primitiveType();


  bool sempred(antlr4::RuleContext *_localctx, size_t ruleIndex, size_t predicateIndex) override;

  bool expressionSempred(ExpressionContext *_localctx, size_t predicateIndex);
  bool typeSpecSempred(TypeSpecContext *_localctx, size_t predicateIndex);

  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

