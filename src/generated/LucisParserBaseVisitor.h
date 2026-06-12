
// Generated from LucisParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LucisParserVisitor.h"


/**
 * This class provides an empty implementation of LucisParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  LucisParserBaseVisitor : public LucisParserVisitor {
public:

  virtual std::any visitProgram(LucisParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreambleDecl(LucisParser::PreambleDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNamespaceDecl(LucisParser::NamespaceDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseRoot(LucisParser::UseRootContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseItem(LucisParser::UseItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseGroup(LucisParser::UseGroupContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUseEnumWildcard(LucisParser::UseEnumWildcardContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModulePath(LucisParser::ModulePathContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIncludeDecl(LucisParser::IncludeDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTopLevelDecl(LucisParser::TopLevelDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeAliasDecl(LucisParser::TypeAliasDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumDecl(LucisParser::EnumDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumVariant(LucisParser::EnumVariantContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumPayloadField(LucisParser::EnumPayloadFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDecl(LucisParser::StructDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructField(LucisParser::StructFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnionDecl(LucisParser::UnionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnionField(LucisParser::UnionFieldContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternDecl(LucisParser::ExternDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParamList(LucisParser::ExternParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternParam(LucisParser::ExternParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionDecl(LucisParser::FunctionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExtendDecl(LucisParser::ExtendDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeParamList(LucisParser::TypeParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeParam(LucisParser::TypeParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExtendMethod(LucisParser::ExtendMethodContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(LucisParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(LucisParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(LucisParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(LucisParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeferStmt(LucisParser::DeferStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNakedBlockStmt(LucisParser::NakedBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInlineBlockStmt(LucisParser::InlineBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeBlockStmt(LucisParser::ScopeBlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeCallbackList(LucisParser::ScopeCallbackListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopeCallback(LucisParser::ScopeCallbackContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(LucisParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarDeclStmt(LucisParser::VarDeclStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignStmt(LucisParser::AssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundAssignStmt(LucisParser::CompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldAssignStmt(LucisParser::FieldAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexFieldAssignStmt(LucisParser::IndexFieldAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldIndexAssignStmt(LucisParser::FieldIndexAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefAssignStmt(LucisParser::DerefAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowAssignStmt(LucisParser::ArrowAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallStmt(LucisParser::CallStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(LucisParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(LucisParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt(LucisParser::IfStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseIfClause(LucisParser::ElseIfClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseClause(LucisParser::ElseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfBody(LucisParser::IfBodyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForInStmt(LucisParser::ForInStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForClassicStmt(LucisParser::ForClassicStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(LucisParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(LucisParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLoopStmt(LucisParser::LoopStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(LucisParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDoWhileStmt(LucisParser::DoWhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLockStmt(LucisParser::LockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTryCatchStmt(LucisParser::TryCatchStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCatchClause(LucisParser::CatchClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFinallyClause(LucisParser::FinallyClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitThrowStmt(LucisParser::ThrowStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchStmt(LucisParser::SwitchStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCaseClause(LucisParser::CaseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDefaultClause(LucisParser::DefaultClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructPosInitExpr(LucisParser::StructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFieldAccessExpr(LucisParser::FieldAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeofExpr(LucisParser::TypeofExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumNamedVariantExpr(LucisParser::GenericEnumNamedVariantExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRshiftExpr(LucisParser::RshiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowMethodCallExpr(LucisParser::ArrowMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOctLitExpr(LucisParser::OctLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitXorExpr(LucisParser::BitXorExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalNotExpr(LucisParser::LogicalNotExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdentExpr(LucisParser::IdentExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreIncrExpr(LucisParser::PreIncrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTernaryExpr(LucisParser::TernaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitChainedTupleIndexExpr(LucisParser::ChainedTupleIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNullLitExpr(LucisParser::NullLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMulExpr(LucisParser::MulExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitAndExpr(LucisParser::BitAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIsExpr(LucisParser::IsExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLshiftExpr(LucisParser::LshiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleLitExpr(LucisParser::TupleLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPropagateExpr(LucisParser::PropagateExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddSubExpr(LucisParser::AddSubExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntLitExpr(LucisParser::IntLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddrOfExpr(LucisParser::AddrOfExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleIndexExpr(LucisParser::TupleIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFloatLitExpr(LucisParser::FloatLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStructLitExpr(LucisParser::GenericStructLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSpawnExpr(LucisParser::SpawnExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrowAccessExpr(LucisParser::ArrowAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStaticMethodCallExpr(LucisParser::GenericStaticMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitListCompExpr(LucisParser::ListCompExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(LucisParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNegExpr(LucisParser::NegExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefExpr(LucisParser::DerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreDecrExpr(LucisParser::PreDecrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSpreadExpr(LucisParser::SpreadExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCatchUnwrapExpr(LucisParser::CatchUnwrapExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStaticMethodCallExpr(LucisParser::StaticMethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNullCoalExpr(LucisParser::NullCoalExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericStructPosInitExpr(LucisParser::GenericStructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpr(LucisParser::CastExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericFnCallExpr(LucisParser::GenericFnCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumAccessExpr(LucisParser::GenericEnumAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumAccessExpr(LucisParser::EnumAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParenExpr(LucisParser::ParenExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitNotExpr(LucisParser::BitNotExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayLitExpr(LucisParser::ArrayLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMethodCallExpr(LucisParser::MethodCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLeadingDotFloatLitExpr(LucisParser::LeadingDotFloatLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructLitExpr(LucisParser::StructLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostDecrExpr(LucisParser::PostDecrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRelExpr(LucisParser::RelExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinLitExpr(LucisParser::BinLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRangeInclExpr(LucisParser::RangeInclExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTupleArrowIndexExpr(LucisParser::TupleArrowIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericQualifiedFnCallExpr(LucisParser::GenericQualifiedFnCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalAndExpr(LucisParser::LogicalAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedStructPosInitExpr(LucisParser::QualifiedStructPosInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStrLitExpr(LucisParser::StrLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAwaitExpr(LucisParser::AwaitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedStructNamedInitExpr(LucisParser::QualifiedStructNamedInitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCStrLitExpr(LucisParser::CStrLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFnCallExpr(LucisParser::FnCallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLogicalOrExpr(LucisParser::LogicalOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSizeofExpr(LucisParser::SizeofExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEqExpr(LucisParser::EqExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitOrExpr(LucisParser::BitOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCharLitExpr(LucisParser::CharLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTryExpr(LucisParser::TryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostIncrExpr(LucisParser::PostIncrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericEnumPosVariantExpr(LucisParser::GenericEnumPosVariantExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBoolLitExpr(LucisParser::BoolLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHexLitExpr(LucisParser::HexLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitChainedTupleArrowIndexExpr(LucisParser::ChainedTupleArrowIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRangeExpr(LucisParser::RangeExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpec(LucisParser::TypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFnTypeSpec(LucisParser::FnTypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrimitiveType(LucisParser::PrimitiveTypeContext *ctx) override {
    return visitChildren(ctx);
  }


};

