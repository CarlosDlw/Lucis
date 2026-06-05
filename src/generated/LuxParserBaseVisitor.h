
// Generated from LuxParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuxParserVisitor.h"


/**
 * This class provides an empty implementation of LuxParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  LuxParserBaseVisitor : public LuxParserVisitor {
public:

  virtual std::any visitProgram(LuxParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreambleDecl(LuxParser::PreambleDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNamespaceDecl(LuxParser::NamespaceDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseRoot(LuxParser::UseRootContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseItem(LuxParser::UseItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseGroup(LuxParser::UseGroupContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModulePath(LuxParser::ModulePathContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIncludeDecl(LuxParser::IncludeDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTopLevelDecl(LuxParser::TopLevelDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeAliasDecl(LuxParser::TypeAliasDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumDecl(LuxParser::EnumDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumVariant(LuxParser::EnumVariantContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumPayloadField(LuxParser::EnumPayloadFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDecl(LuxParser::StructDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructField(LuxParser::StructFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnionDecl(LuxParser::UnionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnionField(LuxParser::UnionFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternDecl(LuxParser::ExternDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParamList(LuxParser::ExternParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParam(LuxParser::ExternParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionDecl(LuxParser::FunctionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExtendDecl(LuxParser::ExtendDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeParamList(LuxParser::TypeParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeParam(LuxParser::TypeParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExtendMethod(LuxParser::ExtendMethodContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(LuxParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(LuxParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(LuxParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(LuxParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeferStmt(LuxParser::DeferStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNakedBlockStmt(LuxParser::NakedBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInlineBlockStmt(LuxParser::InlineBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeBlockStmt(LuxParser::ScopeBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeCallbackList(LuxParser::ScopeCallbackListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeCallback(LuxParser::ScopeCallbackContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(LuxParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarDeclStmt(LuxParser::VarDeclStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignStmt(LuxParser::AssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundAssignStmt(LuxParser::CompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldAssignStmt(LuxParser::FieldAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldCompoundAssignStmt(LuxParser::FieldCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexFieldAssignStmt(LuxParser::IndexFieldAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldIndexAssignStmt(LuxParser::FieldIndexAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefAssignStmt(LuxParser::DerefAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefCompoundAssignStmt(LuxParser::DerefCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowAssignStmt(LuxParser::ArrowAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowCompoundAssignStmt(LuxParser::ArrowCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallStmt(LuxParser::CallStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(LuxParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(LuxParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt(LuxParser::IfStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseIfClause(LuxParser::ElseIfClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseClause(LuxParser::ElseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfBody(LuxParser::IfBodyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForInStmt(LuxParser::ForInStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForClassicStmt(LuxParser::ForClassicStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(LuxParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(LuxParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLoopStmt(LuxParser::LoopStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(LuxParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDoWhileStmt(LuxParser::DoWhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLockStmt(LuxParser::LockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTryCatchStmt(LuxParser::TryCatchStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCatchClause(LuxParser::CatchClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFinallyClause(LuxParser::FinallyClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitThrowStmt(LuxParser::ThrowStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchStmt(LuxParser::SwitchStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCaseClause(LuxParser::CaseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDefaultClause(LuxParser::DefaultClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructPosInitExpr(LuxParser::StructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldAccessExpr(LuxParser::FieldAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeofExpr(LuxParser::TypeofExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumNamedVariantExpr(LuxParser::GenericEnumNamedVariantExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRshiftExpr(LuxParser::RshiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowMethodCallExpr(LuxParser::ArrowMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOctLitExpr(LuxParser::OctLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitXorExpr(LuxParser::BitXorExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalNotExpr(LuxParser::LogicalNotExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdentExpr(LuxParser::IdentExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreIncrExpr(LuxParser::PreIncrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTernaryExpr(LuxParser::TernaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitChainedTupleIndexExpr(LuxParser::ChainedTupleIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNullLitExpr(LuxParser::NullLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMulExpr(LuxParser::MulExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitAndExpr(LuxParser::BitAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIsExpr(LuxParser::IsExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLshiftExpr(LuxParser::LshiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleLitExpr(LuxParser::TupleLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddSubExpr(LuxParser::AddSubExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntLitExpr(LuxParser::IntLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddrOfExpr(LuxParser::AddrOfExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleIndexExpr(LuxParser::TupleIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFloatLitExpr(LuxParser::FloatLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStructLitExpr(LuxParser::GenericStructLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSpawnExpr(LuxParser::SpawnExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowAccessExpr(LuxParser::ArrowAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStaticMethodCallExpr(LuxParser::GenericStaticMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListCompExpr(LuxParser::ListCompExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(LuxParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNegExpr(LuxParser::NegExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefExpr(LuxParser::DerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreDecrExpr(LuxParser::PreDecrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSpreadExpr(LuxParser::SpreadExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCatchUnwrapExpr(LuxParser::CatchUnwrapExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStaticMethodCallExpr(LuxParser::StaticMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNullCoalExpr(LuxParser::NullCoalExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStructPosInitExpr(LuxParser::GenericStructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpr(LuxParser::CastExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericFnCallExpr(LuxParser::GenericFnCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumAccessExpr(LuxParser::GenericEnumAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumAccessExpr(LuxParser::EnumAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParenExpr(LuxParser::ParenExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitNotExpr(LuxParser::BitNotExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayLitExpr(LuxParser::ArrayLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMethodCallExpr(LuxParser::MethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLeadingDotFloatLitExpr(LuxParser::LeadingDotFloatLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructLitExpr(LuxParser::StructLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostDecrExpr(LuxParser::PostDecrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRelExpr(LuxParser::RelExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinLitExpr(LuxParser::BinLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRangeInclExpr(LuxParser::RangeInclExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleArrowIndexExpr(LuxParser::TupleArrowIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalAndExpr(LuxParser::LogicalAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedStructPosInitExpr(LuxParser::QualifiedStructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStrLitExpr(LuxParser::StrLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAwaitExpr(LuxParser::AwaitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedStructNamedInitExpr(LuxParser::QualifiedStructNamedInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCStrLitExpr(LuxParser::CStrLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFnCallExpr(LuxParser::FnCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalOrExpr(LuxParser::LogicalOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSizeofExpr(LuxParser::SizeofExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEqExpr(LuxParser::EqExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitOrExpr(LuxParser::BitOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCharLitExpr(LuxParser::CharLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTryExpr(LuxParser::TryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostIncrExpr(LuxParser::PostIncrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumPosVariantExpr(LuxParser::GenericEnumPosVariantExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBoolLitExpr(LuxParser::BoolLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHexLitExpr(LuxParser::HexLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitChainedTupleArrowIndexExpr(LuxParser::ChainedTupleArrowIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRangeExpr(LuxParser::RangeExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpec(LuxParser::TypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFnTypeSpec(LuxParser::FnTypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimitiveType(LuxParser::PrimitiveTypeContext *ctx) override {
    return visitChildren(ctx);
  }


};

