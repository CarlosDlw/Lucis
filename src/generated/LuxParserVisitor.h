
// Generated from LuxParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LuxParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by LuxParser.
 */
class  LuxParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by LuxParser.
   */
    virtual std::any visitProgram(LuxParser::ProgramContext *context) = 0;

    virtual std::any visitPreambleDecl(LuxParser::PreambleDeclContext *context) = 0;

    virtual std::any visitNamespaceDecl(LuxParser::NamespaceDeclContext *context) = 0;

    virtual std::any visitUseRoot(LuxParser::UseRootContext *context) = 0;

    virtual std::any visitUseItem(LuxParser::UseItemContext *context) = 0;

    virtual std::any visitUseGroup(LuxParser::UseGroupContext *context) = 0;

    virtual std::any visitModulePath(LuxParser::ModulePathContext *context) = 0;

    virtual std::any visitIncludeDecl(LuxParser::IncludeDeclContext *context) = 0;

    virtual std::any visitTopLevelDecl(LuxParser::TopLevelDeclContext *context) = 0;

    virtual std::any visitTypeAliasDecl(LuxParser::TypeAliasDeclContext *context) = 0;

    virtual std::any visitEnumDecl(LuxParser::EnumDeclContext *context) = 0;

    virtual std::any visitEnumVariant(LuxParser::EnumVariantContext *context) = 0;

    virtual std::any visitEnumPayloadField(LuxParser::EnumPayloadFieldContext *context) = 0;

    virtual std::any visitStructDecl(LuxParser::StructDeclContext *context) = 0;

    virtual std::any visitStructField(LuxParser::StructFieldContext *context) = 0;

    virtual std::any visitUnionDecl(LuxParser::UnionDeclContext *context) = 0;

    virtual std::any visitUnionField(LuxParser::UnionFieldContext *context) = 0;

    virtual std::any visitExternDecl(LuxParser::ExternDeclContext *context) = 0;

    virtual std::any visitExternParamList(LuxParser::ExternParamListContext *context) = 0;

    virtual std::any visitExternParam(LuxParser::ExternParamContext *context) = 0;

    virtual std::any visitFunctionDecl(LuxParser::FunctionDeclContext *context) = 0;

    virtual std::any visitExtendDecl(LuxParser::ExtendDeclContext *context) = 0;

    virtual std::any visitTypeParamList(LuxParser::TypeParamListContext *context) = 0;

    virtual std::any visitTypeParam(LuxParser::TypeParamContext *context) = 0;

    virtual std::any visitExtendMethod(LuxParser::ExtendMethodContext *context) = 0;

    virtual std::any visitParamList(LuxParser::ParamListContext *context) = 0;

    virtual std::any visitParam(LuxParser::ParamContext *context) = 0;

    virtual std::any visitBlock(LuxParser::BlockContext *context) = 0;

    virtual std::any visitStatement(LuxParser::StatementContext *context) = 0;

    virtual std::any visitDeferStmt(LuxParser::DeferStmtContext *context) = 0;

    virtual std::any visitNakedBlockStmt(LuxParser::NakedBlockStmtContext *context) = 0;

    virtual std::any visitInlineBlockStmt(LuxParser::InlineBlockStmtContext *context) = 0;

    virtual std::any visitScopeBlockStmt(LuxParser::ScopeBlockStmtContext *context) = 0;

    virtual std::any visitScopeCallbackList(LuxParser::ScopeCallbackListContext *context) = 0;

    virtual std::any visitScopeCallback(LuxParser::ScopeCallbackContext *context) = 0;

    virtual std::any visitExprStmt(LuxParser::ExprStmtContext *context) = 0;

    virtual std::any visitVarDeclStmt(LuxParser::VarDeclStmtContext *context) = 0;

    virtual std::any visitAssignStmt(LuxParser::AssignStmtContext *context) = 0;

    virtual std::any visitCompoundAssignStmt(LuxParser::CompoundAssignStmtContext *context) = 0;

    virtual std::any visitFieldAssignStmt(LuxParser::FieldAssignStmtContext *context) = 0;

    virtual std::any visitFieldCompoundAssignStmt(LuxParser::FieldCompoundAssignStmtContext *context) = 0;

    virtual std::any visitIndexFieldAssignStmt(LuxParser::IndexFieldAssignStmtContext *context) = 0;

    virtual std::any visitFieldIndexAssignStmt(LuxParser::FieldIndexAssignStmtContext *context) = 0;

    virtual std::any visitDerefAssignStmt(LuxParser::DerefAssignStmtContext *context) = 0;

    virtual std::any visitDerefCompoundAssignStmt(LuxParser::DerefCompoundAssignStmtContext *context) = 0;

    virtual std::any visitArrowAssignStmt(LuxParser::ArrowAssignStmtContext *context) = 0;

    virtual std::any visitArrowCompoundAssignStmt(LuxParser::ArrowCompoundAssignStmtContext *context) = 0;

    virtual std::any visitCallStmt(LuxParser::CallStmtContext *context) = 0;

    virtual std::any visitArgList(LuxParser::ArgListContext *context) = 0;

    virtual std::any visitReturnStmt(LuxParser::ReturnStmtContext *context) = 0;

    virtual std::any visitIfStmt(LuxParser::IfStmtContext *context) = 0;

    virtual std::any visitElseIfClause(LuxParser::ElseIfClauseContext *context) = 0;

    virtual std::any visitElseClause(LuxParser::ElseClauseContext *context) = 0;

    virtual std::any visitIfBody(LuxParser::IfBodyContext *context) = 0;

    virtual std::any visitForInStmt(LuxParser::ForInStmtContext *context) = 0;

    virtual std::any visitForClassicStmt(LuxParser::ForClassicStmtContext *context) = 0;

    virtual std::any visitBreakStmt(LuxParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(LuxParser::ContinueStmtContext *context) = 0;

    virtual std::any visitLoopStmt(LuxParser::LoopStmtContext *context) = 0;

    virtual std::any visitWhileStmt(LuxParser::WhileStmtContext *context) = 0;

    virtual std::any visitDoWhileStmt(LuxParser::DoWhileStmtContext *context) = 0;

    virtual std::any visitLockStmt(LuxParser::LockStmtContext *context) = 0;

    virtual std::any visitTryCatchStmt(LuxParser::TryCatchStmtContext *context) = 0;

    virtual std::any visitCatchClause(LuxParser::CatchClauseContext *context) = 0;

    virtual std::any visitFinallyClause(LuxParser::FinallyClauseContext *context) = 0;

    virtual std::any visitThrowStmt(LuxParser::ThrowStmtContext *context) = 0;

    virtual std::any visitSwitchStmt(LuxParser::SwitchStmtContext *context) = 0;

    virtual std::any visitCaseClause(LuxParser::CaseClauseContext *context) = 0;

    virtual std::any visitDefaultClause(LuxParser::DefaultClauseContext *context) = 0;

    virtual std::any visitFieldAccessExpr(LuxParser::FieldAccessExprContext *context) = 0;

    virtual std::any visitTypeofExpr(LuxParser::TypeofExprContext *context) = 0;

    virtual std::any visitGenericEnumNamedVariantExpr(LuxParser::GenericEnumNamedVariantExprContext *context) = 0;

    virtual std::any visitRshiftExpr(LuxParser::RshiftExprContext *context) = 0;

    virtual std::any visitArrowMethodCallExpr(LuxParser::ArrowMethodCallExprContext *context) = 0;

    virtual std::any visitOctLitExpr(LuxParser::OctLitExprContext *context) = 0;

    virtual std::any visitBitXorExpr(LuxParser::BitXorExprContext *context) = 0;

    virtual std::any visitLogicalNotExpr(LuxParser::LogicalNotExprContext *context) = 0;

    virtual std::any visitIdentExpr(LuxParser::IdentExprContext *context) = 0;

    virtual std::any visitPreIncrExpr(LuxParser::PreIncrExprContext *context) = 0;

    virtual std::any visitTernaryExpr(LuxParser::TernaryExprContext *context) = 0;

    virtual std::any visitChainedTupleIndexExpr(LuxParser::ChainedTupleIndexExprContext *context) = 0;

    virtual std::any visitNullLitExpr(LuxParser::NullLitExprContext *context) = 0;

    virtual std::any visitMulExpr(LuxParser::MulExprContext *context) = 0;

    virtual std::any visitBitAndExpr(LuxParser::BitAndExprContext *context) = 0;

    virtual std::any visitIsExpr(LuxParser::IsExprContext *context) = 0;

    virtual std::any visitLshiftExpr(LuxParser::LshiftExprContext *context) = 0;

    virtual std::any visitTupleLitExpr(LuxParser::TupleLitExprContext *context) = 0;

    virtual std::any visitAddSubExpr(LuxParser::AddSubExprContext *context) = 0;

    virtual std::any visitIntLitExpr(LuxParser::IntLitExprContext *context) = 0;

    virtual std::any visitAddrOfExpr(LuxParser::AddrOfExprContext *context) = 0;

    virtual std::any visitTupleIndexExpr(LuxParser::TupleIndexExprContext *context) = 0;

    virtual std::any visitFloatLitExpr(LuxParser::FloatLitExprContext *context) = 0;

    virtual std::any visitGenericStructLitExpr(LuxParser::GenericStructLitExprContext *context) = 0;

    virtual std::any visitSpawnExpr(LuxParser::SpawnExprContext *context) = 0;

    virtual std::any visitArrowAccessExpr(LuxParser::ArrowAccessExprContext *context) = 0;

    virtual std::any visitGenericStaticMethodCallExpr(LuxParser::GenericStaticMethodCallExprContext *context) = 0;

    virtual std::any visitListCompExpr(LuxParser::ListCompExprContext *context) = 0;

    virtual std::any visitIndexExpr(LuxParser::IndexExprContext *context) = 0;

    virtual std::any visitNegExpr(LuxParser::NegExprContext *context) = 0;

    virtual std::any visitDerefExpr(LuxParser::DerefExprContext *context) = 0;

    virtual std::any visitPreDecrExpr(LuxParser::PreDecrExprContext *context) = 0;

    virtual std::any visitSpreadExpr(LuxParser::SpreadExprContext *context) = 0;

    virtual std::any visitCatchUnwrapExpr(LuxParser::CatchUnwrapExprContext *context) = 0;

    virtual std::any visitStaticMethodCallExpr(LuxParser::StaticMethodCallExprContext *context) = 0;

    virtual std::any visitNullCoalExpr(LuxParser::NullCoalExprContext *context) = 0;

    virtual std::any visitCastExpr(LuxParser::CastExprContext *context) = 0;

    virtual std::any visitGenericFnCallExpr(LuxParser::GenericFnCallExprContext *context) = 0;

    virtual std::any visitGenericEnumAccessExpr(LuxParser::GenericEnumAccessExprContext *context) = 0;

    virtual std::any visitEnumAccessExpr(LuxParser::EnumAccessExprContext *context) = 0;

    virtual std::any visitParenExpr(LuxParser::ParenExprContext *context) = 0;

    virtual std::any visitBitNotExpr(LuxParser::BitNotExprContext *context) = 0;

    virtual std::any visitArrayLitExpr(LuxParser::ArrayLitExprContext *context) = 0;

    virtual std::any visitMethodCallExpr(LuxParser::MethodCallExprContext *context) = 0;

    virtual std::any visitLeadingDotFloatLitExpr(LuxParser::LeadingDotFloatLitExprContext *context) = 0;

    virtual std::any visitEnumNamedVariantExpr(LuxParser::EnumNamedVariantExprContext *context) = 0;

    virtual std::any visitStructLitExpr(LuxParser::StructLitExprContext *context) = 0;

    virtual std::any visitPostDecrExpr(LuxParser::PostDecrExprContext *context) = 0;

    virtual std::any visitRelExpr(LuxParser::RelExprContext *context) = 0;

    virtual std::any visitBinLitExpr(LuxParser::BinLitExprContext *context) = 0;

    virtual std::any visitRangeInclExpr(LuxParser::RangeInclExprContext *context) = 0;

    virtual std::any visitTupleArrowIndexExpr(LuxParser::TupleArrowIndexExprContext *context) = 0;

    virtual std::any visitLogicalAndExpr(LuxParser::LogicalAndExprContext *context) = 0;

    virtual std::any visitStrLitExpr(LuxParser::StrLitExprContext *context) = 0;

    virtual std::any visitAwaitExpr(LuxParser::AwaitExprContext *context) = 0;

    virtual std::any visitCStrLitExpr(LuxParser::CStrLitExprContext *context) = 0;

    virtual std::any visitFnCallExpr(LuxParser::FnCallExprContext *context) = 0;

    virtual std::any visitLogicalOrExpr(LuxParser::LogicalOrExprContext *context) = 0;

    virtual std::any visitSizeofExpr(LuxParser::SizeofExprContext *context) = 0;

    virtual std::any visitEqExpr(LuxParser::EqExprContext *context) = 0;

    virtual std::any visitBitOrExpr(LuxParser::BitOrExprContext *context) = 0;

    virtual std::any visitCharLitExpr(LuxParser::CharLitExprContext *context) = 0;

    virtual std::any visitTryExpr(LuxParser::TryExprContext *context) = 0;

    virtual std::any visitPostIncrExpr(LuxParser::PostIncrExprContext *context) = 0;

    virtual std::any visitBoolLitExpr(LuxParser::BoolLitExprContext *context) = 0;

    virtual std::any visitHexLitExpr(LuxParser::HexLitExprContext *context) = 0;

    virtual std::any visitChainedTupleArrowIndexExpr(LuxParser::ChainedTupleArrowIndexExprContext *context) = 0;

    virtual std::any visitRangeExpr(LuxParser::RangeExprContext *context) = 0;

    virtual std::any visitTypeSpec(LuxParser::TypeSpecContext *context) = 0;

    virtual std::any visitFnTypeSpec(LuxParser::FnTypeSpecContext *context) = 0;

    virtual std::any visitPrimitiveType(LuxParser::PrimitiveTypeContext *context) = 0;


};

