
// Generated from LucisParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "LucisParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by LucisParser.
 */
class  LucisParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by LucisParser.
   */
    virtual std::any visitProgram(LucisParser::ProgramContext *context) = 0;

    virtual std::any visitPreambleDecl(LucisParser::PreambleDeclContext *context) = 0;

    virtual std::any visitUseRoot(LucisParser::UseRootContext *context) = 0;

    virtual std::any visitUseItem(LucisParser::UseItemContext *context) = 0;

    virtual std::any visitUseGroup(LucisParser::UseGroupContext *context) = 0;

    virtual std::any visitUseEnumWildcard(LucisParser::UseEnumWildcardContext *context) = 0;

    virtual std::any visitModulePath(LucisParser::ModulePathContext *context) = 0;

    virtual std::any visitIncludeDecl(LucisParser::IncludeDeclContext *context) = 0;

    virtual std::any visitAttrArg(LucisParser::AttrArgContext *context) = 0;

    virtual std::any visitAttrArgList(LucisParser::AttrArgListContext *context) = 0;

    virtual std::any visitAttribute(LucisParser::AttributeContext *context) = 0;

    virtual std::any visitAttributeList(LucisParser::AttributeListContext *context) = 0;

    virtual std::any visitTopLevelDecl(LucisParser::TopLevelDeclContext *context) = 0;

    virtual std::any visitTypeAliasDecl(LucisParser::TypeAliasDeclContext *context) = 0;

    virtual std::any visitEnumDecl(LucisParser::EnumDeclContext *context) = 0;

    virtual std::any visitEnumVariant(LucisParser::EnumVariantContext *context) = 0;

    virtual std::any visitEnumPayloadField(LucisParser::EnumPayloadFieldContext *context) = 0;

    virtual std::any visitStructDecl(LucisParser::StructDeclContext *context) = 0;

    virtual std::any visitStructField(LucisParser::StructFieldContext *context) = 0;

    virtual std::any visitUnionDecl(LucisParser::UnionDeclContext *context) = 0;

    virtual std::any visitUnionField(LucisParser::UnionFieldContext *context) = 0;

    virtual std::any visitExternDecl(LucisParser::ExternDeclContext *context) = 0;

    virtual std::any visitExternParamList(LucisParser::ExternParamListContext *context) = 0;

    virtual std::any visitExternParam(LucisParser::ExternParamContext *context) = 0;

    virtual std::any visitFunctionDecl(LucisParser::FunctionDeclContext *context) = 0;

    virtual std::any visitExtendDecl(LucisParser::ExtendDeclContext *context) = 0;

    virtual std::any visitTypeParamList(LucisParser::TypeParamListContext *context) = 0;

    virtual std::any visitTypeParam(LucisParser::TypeParamContext *context) = 0;

    virtual std::any visitExtendMethod(LucisParser::ExtendMethodContext *context) = 0;

    virtual std::any visitParamList(LucisParser::ParamListContext *context) = 0;

    virtual std::any visitParam(LucisParser::ParamContext *context) = 0;

    virtual std::any visitBlock(LucisParser::BlockContext *context) = 0;

    virtual std::any visitStatement(LucisParser::StatementContext *context) = 0;

    virtual std::any visitDeferStmt(LucisParser::DeferStmtContext *context) = 0;

    virtual std::any visitNakedBlockStmt(LucisParser::NakedBlockStmtContext *context) = 0;

    virtual std::any visitInlineBlockStmt(LucisParser::InlineBlockStmtContext *context) = 0;

    virtual std::any visitScopeBlockStmt(LucisParser::ScopeBlockStmtContext *context) = 0;

    virtual std::any visitScopeCallbackList(LucisParser::ScopeCallbackListContext *context) = 0;

    virtual std::any visitScopeCallback(LucisParser::ScopeCallbackContext *context) = 0;

    virtual std::any visitLabelDef(LucisParser::LabelDefContext *context) = 0;

    virtual std::any visitAsmStmt(LucisParser::AsmStmtContext *context) = 0;

    virtual std::any visitAsmOutputList(LucisParser::AsmOutputListContext *context) = 0;

    virtual std::any visitAsmInputList(LucisParser::AsmInputListContext *context) = 0;

    virtual std::any visitAsmClobberList(LucisParser::AsmClobberListContext *context) = 0;

    virtual std::any visitAsmGotoLabelList(LucisParser::AsmGotoLabelListContext *context) = 0;

    virtual std::any visitAsmOutput(LucisParser::AsmOutputContext *context) = 0;

    virtual std::any visitAsmOperand(LucisParser::AsmOperandContext *context) = 0;

    virtual std::any visitExprStmt(LucisParser::ExprStmtContext *context) = 0;

    virtual std::any visitVarDeclStmt(LucisParser::VarDeclStmtContext *context) = 0;

    virtual std::any visitVarDeclarator(LucisParser::VarDeclaratorContext *context) = 0;

    virtual std::any visitConstDeclStmt(LucisParser::ConstDeclStmtContext *context) = 0;

    virtual std::any visitConstDeclarator(LucisParser::ConstDeclaratorContext *context) = 0;

    virtual std::any visitAssignStmt(LucisParser::AssignStmtContext *context) = 0;

    virtual std::any visitCompoundAssignStmt(LucisParser::CompoundAssignStmtContext *context) = 0;

    virtual std::any visitFieldAssignStmt(LucisParser::FieldAssignStmtContext *context) = 0;

    virtual std::any visitFieldCompoundAssignStmt(LucisParser::FieldCompoundAssignStmtContext *context) = 0;

    virtual std::any visitIndexFieldAssignStmt(LucisParser::IndexFieldAssignStmtContext *context) = 0;

    virtual std::any visitFieldIndexAssignStmt(LucisParser::FieldIndexAssignStmtContext *context) = 0;

    virtual std::any visitDerefAssignStmt(LucisParser::DerefAssignStmtContext *context) = 0;

    virtual std::any visitDerefCompoundAssignStmt(LucisParser::DerefCompoundAssignStmtContext *context) = 0;

    virtual std::any visitArrowAssignStmt(LucisParser::ArrowAssignStmtContext *context) = 0;

    virtual std::any visitArrowCompoundAssignStmt(LucisParser::ArrowCompoundAssignStmtContext *context) = 0;

    virtual std::any visitArrowAnyAssignStmt(LucisParser::ArrowAnyAssignStmtContext *context) = 0;

    virtual std::any visitArrowAnyCompoundAssignStmt(LucisParser::ArrowAnyCompoundAssignStmtContext *context) = 0;

    virtual std::any visitCallStmt(LucisParser::CallStmtContext *context) = 0;

    virtual std::any visitArgList(LucisParser::ArgListContext *context) = 0;

    virtual std::any visitReturnStmt(LucisParser::ReturnStmtContext *context) = 0;

    virtual std::any visitIfStmt(LucisParser::IfStmtContext *context) = 0;

    virtual std::any visitElseIfClause(LucisParser::ElseIfClauseContext *context) = 0;

    virtual std::any visitElseClause(LucisParser::ElseClauseContext *context) = 0;

    virtual std::any visitIfBody(LucisParser::IfBodyContext *context) = 0;

    virtual std::any visitForInStmt(LucisParser::ForInStmtContext *context) = 0;

    virtual std::any visitForClassicStmt(LucisParser::ForClassicStmtContext *context) = 0;

    virtual std::any visitBreakStmt(LucisParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(LucisParser::ContinueStmtContext *context) = 0;

    virtual std::any visitLoopStmt(LucisParser::LoopStmtContext *context) = 0;

    virtual std::any visitWhileStmt(LucisParser::WhileStmtContext *context) = 0;

    virtual std::any visitDoWhileStmt(LucisParser::DoWhileStmtContext *context) = 0;

    virtual std::any visitLockStmt(LucisParser::LockStmtContext *context) = 0;

    virtual std::any visitTryCatchStmt(LucisParser::TryCatchStmtContext *context) = 0;

    virtual std::any visitCatchClause(LucisParser::CatchClauseContext *context) = 0;

    virtual std::any visitFinallyClause(LucisParser::FinallyClauseContext *context) = 0;

    virtual std::any visitThrowStmt(LucisParser::ThrowStmtContext *context) = 0;

    virtual std::any visitSwitchStmt(LucisParser::SwitchStmtContext *context) = 0;

    virtual std::any visitCaseClause(LucisParser::CaseClauseContext *context) = 0;

    virtual std::any visitDefaultClause(LucisParser::DefaultClauseContext *context) = 0;

    virtual std::any visitMatchArm(LucisParser::MatchArmContext *context) = 0;

    virtual std::any visitPattern(LucisParser::PatternContext *context) = 0;

    virtual std::any visitLiteralPattern(LucisParser::LiteralPatternContext *context) = 0;

    virtual std::any visitStructPosInitExpr(LucisParser::StructPosInitExprContext *context) = 0;

    virtual std::any visitSuffixedOctLitExpr(LucisParser::SuffixedOctLitExprContext *context) = 0;

    virtual std::any visitFieldAccessExpr(LucisParser::FieldAccessExprContext *context) = 0;

    virtual std::any visitTypeofExpr(LucisParser::TypeofExprContext *context) = 0;

    virtual std::any visitGenericEnumNamedVariantExpr(LucisParser::GenericEnumNamedVariantExprContext *context) = 0;

    virtual std::any visitBtickExpr(LucisParser::BtickExprContext *context) = 0;

    virtual std::any visitRshiftExpr(LucisParser::RshiftExprContext *context) = 0;

    virtual std::any visitArrowMethodCallExpr(LucisParser::ArrowMethodCallExprContext *context) = 0;

    virtual std::any visitAsmExpr(LucisParser::AsmExprContext *context) = 0;

    virtual std::any visitOctLitExpr(LucisParser::OctLitExprContext *context) = 0;

    virtual std::any visitBitXorExpr(LucisParser::BitXorExprContext *context) = 0;

    virtual std::any visitLogicalNotExpr(LucisParser::LogicalNotExprContext *context) = 0;

    virtual std::any visitIdentExpr(LucisParser::IdentExprContext *context) = 0;

    virtual std::any visitPreIncrExpr(LucisParser::PreIncrExprContext *context) = 0;

    virtual std::any visitTernaryExpr(LucisParser::TernaryExprContext *context) = 0;

    virtual std::any visitSuffixedIntFloatExpr(LucisParser::SuffixedIntFloatExprContext *context) = 0;

    virtual std::any visitChainedTupleIndexExpr(LucisParser::ChainedTupleIndexExprContext *context) = 0;

    virtual std::any visitNullLitExpr(LucisParser::NullLitExprContext *context) = 0;

    virtual std::any visitMulExpr(LucisParser::MulExprContext *context) = 0;

    virtual std::any visitSuffixedLeadingDotFloatExpr(LucisParser::SuffixedLeadingDotFloatExprContext *context) = 0;

    virtual std::any visitBitAndExpr(LucisParser::BitAndExprContext *context) = 0;

    virtual std::any visitIntBtickExpr(LucisParser::IntBtickExprContext *context) = 0;

    virtual std::any visitIsExpr(LucisParser::IsExprContext *context) = 0;

    virtual std::any visitLshiftExpr(LucisParser::LshiftExprContext *context) = 0;

    virtual std::any visitTupleLitExpr(LucisParser::TupleLitExprContext *context) = 0;

    virtual std::any visitPropagateExpr(LucisParser::PropagateExprContext *context) = 0;

    virtual std::any visitSuffixedHexLitExpr(LucisParser::SuffixedHexLitExprContext *context) = 0;

    virtual std::any visitSuffixedFloatIntExpr(LucisParser::SuffixedFloatIntExprContext *context) = 0;

    virtual std::any visitAddSubExpr(LucisParser::AddSubExprContext *context) = 0;

    virtual std::any visitSuffixedFloatLitExpr(LucisParser::SuffixedFloatLitExprContext *context) = 0;

    virtual std::any visitIntLitExpr(LucisParser::IntLitExprContext *context) = 0;

    virtual std::any visitAddrOfExpr(LucisParser::AddrOfExprContext *context) = 0;

    virtual std::any visitSuffixedBinLitExpr(LucisParser::SuffixedBinLitExprContext *context) = 0;

    virtual std::any visitTupleIndexExpr(LucisParser::TupleIndexExprContext *context) = 0;

    virtual std::any visitFloatLitExpr(LucisParser::FloatLitExprContext *context) = 0;

    virtual std::any visitGenericStructLitExpr(LucisParser::GenericStructLitExprContext *context) = 0;

    virtual std::any visitSpawnExpr(LucisParser::SpawnExprContext *context) = 0;

    virtual std::any visitArrowAccessExpr(LucisParser::ArrowAccessExprContext *context) = 0;

    virtual std::any visitGenericStaticMethodCallExpr(LucisParser::GenericStaticMethodCallExprContext *context) = 0;

    virtual std::any visitListCompExpr(LucisParser::ListCompExprContext *context) = 0;

    virtual std::any visitIndexExpr(LucisParser::IndexExprContext *context) = 0;

    virtual std::any visitNegExpr(LucisParser::NegExprContext *context) = 0;

    virtual std::any visitDerefExpr(LucisParser::DerefExprContext *context) = 0;

    virtual std::any visitPreDecrExpr(LucisParser::PreDecrExprContext *context) = 0;

    virtual std::any visitSpreadExpr(LucisParser::SpreadExprContext *context) = 0;

    virtual std::any visitRawBtickExpr(LucisParser::RawBtickExprContext *context) = 0;

    virtual std::any visitShellBtickExpr(LucisParser::ShellBtickExprContext *context) = 0;

    virtual std::any visitCatchUnwrapExpr(LucisParser::CatchUnwrapExprContext *context) = 0;

    virtual std::any visitStaticMethodCallExpr(LucisParser::StaticMethodCallExprContext *context) = 0;

    virtual std::any visitNullCoalExpr(LucisParser::NullCoalExprContext *context) = 0;

    virtual std::any visitGenericStructPosInitExpr(LucisParser::GenericStructPosInitExprContext *context) = 0;

    virtual std::any visitCastExpr(LucisParser::CastExprContext *context) = 0;

    virtual std::any visitOffsetofExpr(LucisParser::OffsetofExprContext *context) = 0;

    virtual std::any visitGenericFnCallExpr(LucisParser::GenericFnCallExprContext *context) = 0;

    virtual std::any visitGenericEnumAccessExpr(LucisParser::GenericEnumAccessExprContext *context) = 0;

    virtual std::any visitMatchExpr(LucisParser::MatchExprContext *context) = 0;

    virtual std::any visitEnumAccessExpr(LucisParser::EnumAccessExprContext *context) = 0;

    virtual std::any visitParenExpr(LucisParser::ParenExprContext *context) = 0;

    virtual std::any visitBitNotExpr(LucisParser::BitNotExprContext *context) = 0;

    virtual std::any visitArrayLitExpr(LucisParser::ArrayLitExprContext *context) = 0;

    virtual std::any visitMethodCallExpr(LucisParser::MethodCallExprContext *context) = 0;

    virtual std::any visitAlignofExpr(LucisParser::AlignofExprContext *context) = 0;

    virtual std::any visitLeadingDotFloatLitExpr(LucisParser::LeadingDotFloatLitExprContext *context) = 0;

    virtual std::any visitLambdaExpr(LucisParser::LambdaExprContext *context) = 0;

    virtual std::any visitStructLitExpr(LucisParser::StructLitExprContext *context) = 0;

    virtual std::any visitPostDecrExpr(LucisParser::PostDecrExprContext *context) = 0;

    virtual std::any visitRelExpr(LucisParser::RelExprContext *context) = 0;

    virtual std::any visitBinLitExpr(LucisParser::BinLitExprContext *context) = 0;

    virtual std::any visitRangeInclExpr(LucisParser::RangeInclExprContext *context) = 0;

    virtual std::any visitLambdaBlockExpr(LucisParser::LambdaBlockExprContext *context) = 0;

    virtual std::any visitTupleArrowIndexExpr(LucisParser::TupleArrowIndexExprContext *context) = 0;

    virtual std::any visitGenericQualifiedFnCallExpr(LucisParser::GenericQualifiedFnCallExprContext *context) = 0;

    virtual std::any visitLogicalAndExpr(LucisParser::LogicalAndExprContext *context) = 0;

    virtual std::any visitQualifiedStructPosInitExpr(LucisParser::QualifiedStructPosInitExprContext *context) = 0;

    virtual std::any visitStrLitExpr(LucisParser::StrLitExprContext *context) = 0;

    virtual std::any visitAwaitExpr(LucisParser::AwaitExprContext *context) = 0;

    virtual std::any visitQualifiedStructNamedInitExpr(LucisParser::QualifiedStructNamedInitExprContext *context) = 0;

    virtual std::any visitCStrLitExpr(LucisParser::CStrLitExprContext *context) = 0;

    virtual std::any visitFnCallExpr(LucisParser::FnCallExprContext *context) = 0;

    virtual std::any visitLogicalOrExpr(LucisParser::LogicalOrExprContext *context) = 0;

    virtual std::any visitSizeofExpr(LucisParser::SizeofExprContext *context) = 0;

    virtual std::any visitCmptBtickExpr(LucisParser::CmptBtickExprContext *context) = 0;

    virtual std::any visitEqExpr(LucisParser::EqExprContext *context) = 0;

    virtual std::any visitBitOrExpr(LucisParser::BitOrExprContext *context) = 0;

    virtual std::any visitCharLitExpr(LucisParser::CharLitExprContext *context) = 0;

    virtual std::any visitTryExpr(LucisParser::TryExprContext *context) = 0;

    virtual std::any visitPostIncrExpr(LucisParser::PostIncrExprContext *context) = 0;

    virtual std::any visitGenericEnumPosVariantExpr(LucisParser::GenericEnumPosVariantExprContext *context) = 0;

    virtual std::any visitBoolLitExpr(LucisParser::BoolLitExprContext *context) = 0;

    virtual std::any visitHexLitExpr(LucisParser::HexLitExprContext *context) = 0;

    virtual std::any visitSuffixedIntLitExpr(LucisParser::SuffixedIntLitExprContext *context) = 0;

    virtual std::any visitChainedTupleArrowIndexExpr(LucisParser::ChainedTupleArrowIndexExprContext *context) = 0;

    virtual std::any visitRangeExpr(LucisParser::RangeExprContext *context) = 0;

    virtual std::any visitTypeSpec(LucisParser::TypeSpecContext *context) = 0;

    virtual std::any visitFnTypeSpec(LucisParser::FnTypeSpecContext *context) = 0;

    virtual std::any visitPrimitiveType(LucisParser::PrimitiveTypeContext *context) = 0;

    virtual std::any visitCMacroBlock(LucisParser::CMacroBlockContext *context) = 0;

    virtual std::any visitAsmBBlock(LucisParser::AsmBBlockContext *context) = 0;


};

