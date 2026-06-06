// Generated from /home/carlos/Projects/Cpp/Lux/grammar/LuxParser.g4 by ANTLR 4.13.1
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link LuxParser}.
 */
public interface LuxParserListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link LuxParser#program}.
	 * @param ctx the parse tree
	 */
	void enterProgram(LuxParser.ProgramContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#program}.
	 * @param ctx the parse tree
	 */
	void exitProgram(LuxParser.ProgramContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#preambleDecl}.
	 * @param ctx the parse tree
	 */
	void enterPreambleDecl(LuxParser.PreambleDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#preambleDecl}.
	 * @param ctx the parse tree
	 */
	void exitPreambleDecl(LuxParser.PreambleDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#namespaceDecl}.
	 * @param ctx the parse tree
	 */
	void enterNamespaceDecl(LuxParser.NamespaceDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#namespaceDecl}.
	 * @param ctx the parse tree
	 */
	void exitNamespaceDecl(LuxParser.NamespaceDeclContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useRoot}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseRoot(LuxParser.UseRootContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useRoot}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseRoot(LuxParser.UseRootContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useItem}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseItem(LuxParser.UseItemContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useItem}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseItem(LuxParser.UseItemContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useGroup}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseGroup(LuxParser.UseGroupContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useGroup}
	 * labeled alternative in {@link LuxParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseGroup(LuxParser.UseGroupContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#modulePath}.
	 * @param ctx the parse tree
	 */
	void enterModulePath(LuxParser.ModulePathContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#modulePath}.
	 * @param ctx the parse tree
	 */
	void exitModulePath(LuxParser.ModulePathContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#includeDecl}.
	 * @param ctx the parse tree
	 */
	void enterIncludeDecl(LuxParser.IncludeDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#includeDecl}.
	 * @param ctx the parse tree
	 */
	void exitIncludeDecl(LuxParser.IncludeDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#topLevelDecl}.
	 * @param ctx the parse tree
	 */
	void enterTopLevelDecl(LuxParser.TopLevelDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#topLevelDecl}.
	 * @param ctx the parse tree
	 */
	void exitTopLevelDecl(LuxParser.TopLevelDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#typeAliasDecl}.
	 * @param ctx the parse tree
	 */
	void enterTypeAliasDecl(LuxParser.TypeAliasDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#typeAliasDecl}.
	 * @param ctx the parse tree
	 */
	void exitTypeAliasDecl(LuxParser.TypeAliasDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#enumDecl}.
	 * @param ctx the parse tree
	 */
	void enterEnumDecl(LuxParser.EnumDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#enumDecl}.
	 * @param ctx the parse tree
	 */
	void exitEnumDecl(LuxParser.EnumDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#enumVariant}.
	 * @param ctx the parse tree
	 */
	void enterEnumVariant(LuxParser.EnumVariantContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#enumVariant}.
	 * @param ctx the parse tree
	 */
	void exitEnumVariant(LuxParser.EnumVariantContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#enumPayloadField}.
	 * @param ctx the parse tree
	 */
	void enterEnumPayloadField(LuxParser.EnumPayloadFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#enumPayloadField}.
	 * @param ctx the parse tree
	 */
	void exitEnumPayloadField(LuxParser.EnumPayloadFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#structDecl}.
	 * @param ctx the parse tree
	 */
	void enterStructDecl(LuxParser.StructDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#structDecl}.
	 * @param ctx the parse tree
	 */
	void exitStructDecl(LuxParser.StructDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#structField}.
	 * @param ctx the parse tree
	 */
	void enterStructField(LuxParser.StructFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#structField}.
	 * @param ctx the parse tree
	 */
	void exitStructField(LuxParser.StructFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#unionDecl}.
	 * @param ctx the parse tree
	 */
	void enterUnionDecl(LuxParser.UnionDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#unionDecl}.
	 * @param ctx the parse tree
	 */
	void exitUnionDecl(LuxParser.UnionDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#unionField}.
	 * @param ctx the parse tree
	 */
	void enterUnionField(LuxParser.UnionFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#unionField}.
	 * @param ctx the parse tree
	 */
	void exitUnionField(LuxParser.UnionFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#externDecl}.
	 * @param ctx the parse tree
	 */
	void enterExternDecl(LuxParser.ExternDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#externDecl}.
	 * @param ctx the parse tree
	 */
	void exitExternDecl(LuxParser.ExternDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#externParamList}.
	 * @param ctx the parse tree
	 */
	void enterExternParamList(LuxParser.ExternParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#externParamList}.
	 * @param ctx the parse tree
	 */
	void exitExternParamList(LuxParser.ExternParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#externParam}.
	 * @param ctx the parse tree
	 */
	void enterExternParam(LuxParser.ExternParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#externParam}.
	 * @param ctx the parse tree
	 */
	void exitExternParam(LuxParser.ExternParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#functionDecl}.
	 * @param ctx the parse tree
	 */
	void enterFunctionDecl(LuxParser.FunctionDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#functionDecl}.
	 * @param ctx the parse tree
	 */
	void exitFunctionDecl(LuxParser.FunctionDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#extendDecl}.
	 * @param ctx the parse tree
	 */
	void enterExtendDecl(LuxParser.ExtendDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#extendDecl}.
	 * @param ctx the parse tree
	 */
	void exitExtendDecl(LuxParser.ExtendDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#typeParamList}.
	 * @param ctx the parse tree
	 */
	void enterTypeParamList(LuxParser.TypeParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#typeParamList}.
	 * @param ctx the parse tree
	 */
	void exitTypeParamList(LuxParser.TypeParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#typeParam}.
	 * @param ctx the parse tree
	 */
	void enterTypeParam(LuxParser.TypeParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#typeParam}.
	 * @param ctx the parse tree
	 */
	void exitTypeParam(LuxParser.TypeParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#extendMethod}.
	 * @param ctx the parse tree
	 */
	void enterExtendMethod(LuxParser.ExtendMethodContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#extendMethod}.
	 * @param ctx the parse tree
	 */
	void exitExtendMethod(LuxParser.ExtendMethodContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#paramList}.
	 * @param ctx the parse tree
	 */
	void enterParamList(LuxParser.ParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#paramList}.
	 * @param ctx the parse tree
	 */
	void exitParamList(LuxParser.ParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#param}.
	 * @param ctx the parse tree
	 */
	void enterParam(LuxParser.ParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#param}.
	 * @param ctx the parse tree
	 */
	void exitParam(LuxParser.ParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#block}.
	 * @param ctx the parse tree
	 */
	void enterBlock(LuxParser.BlockContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#block}.
	 * @param ctx the parse tree
	 */
	void exitBlock(LuxParser.BlockContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#statement}.
	 * @param ctx the parse tree
	 */
	void enterStatement(LuxParser.StatementContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#statement}.
	 * @param ctx the parse tree
	 */
	void exitStatement(LuxParser.StatementContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#deferStmt}.
	 * @param ctx the parse tree
	 */
	void enterDeferStmt(LuxParser.DeferStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#deferStmt}.
	 * @param ctx the parse tree
	 */
	void exitDeferStmt(LuxParser.DeferStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#nakedBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterNakedBlockStmt(LuxParser.NakedBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#nakedBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitNakedBlockStmt(LuxParser.NakedBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#inlineBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterInlineBlockStmt(LuxParser.InlineBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#inlineBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitInlineBlockStmt(LuxParser.InlineBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#scopeBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterScopeBlockStmt(LuxParser.ScopeBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#scopeBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitScopeBlockStmt(LuxParser.ScopeBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#scopeCallbackList}.
	 * @param ctx the parse tree
	 */
	void enterScopeCallbackList(LuxParser.ScopeCallbackListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#scopeCallbackList}.
	 * @param ctx the parse tree
	 */
	void exitScopeCallbackList(LuxParser.ScopeCallbackListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#scopeCallback}.
	 * @param ctx the parse tree
	 */
	void enterScopeCallback(LuxParser.ScopeCallbackContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#scopeCallback}.
	 * @param ctx the parse tree
	 */
	void exitScopeCallback(LuxParser.ScopeCallbackContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#exprStmt}.
	 * @param ctx the parse tree
	 */
	void enterExprStmt(LuxParser.ExprStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#exprStmt}.
	 * @param ctx the parse tree
	 */
	void exitExprStmt(LuxParser.ExprStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#varDeclStmt}.
	 * @param ctx the parse tree
	 */
	void enterVarDeclStmt(LuxParser.VarDeclStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#varDeclStmt}.
	 * @param ctx the parse tree
	 */
	void exitVarDeclStmt(LuxParser.VarDeclStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#assignStmt}.
	 * @param ctx the parse tree
	 */
	void enterAssignStmt(LuxParser.AssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#assignStmt}.
	 * @param ctx the parse tree
	 */
	void exitAssignStmt(LuxParser.AssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#compoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterCompoundAssignStmt(LuxParser.CompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#compoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitCompoundAssignStmt(LuxParser.CompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#fieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldAssignStmt(LuxParser.FieldAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#fieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldAssignStmt(LuxParser.FieldAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#fieldCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldCompoundAssignStmt(LuxParser.FieldCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#fieldCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldCompoundAssignStmt(LuxParser.FieldCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#indexFieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterIndexFieldAssignStmt(LuxParser.IndexFieldAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#indexFieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitIndexFieldAssignStmt(LuxParser.IndexFieldAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#fieldIndexAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldIndexAssignStmt(LuxParser.FieldIndexAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#fieldIndexAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldIndexAssignStmt(LuxParser.FieldIndexAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#derefAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterDerefAssignStmt(LuxParser.DerefAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#derefAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitDerefAssignStmt(LuxParser.DerefAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#derefCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterDerefCompoundAssignStmt(LuxParser.DerefCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#derefCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitDerefCompoundAssignStmt(LuxParser.DerefCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#arrowAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterArrowAssignStmt(LuxParser.ArrowAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#arrowAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitArrowAssignStmt(LuxParser.ArrowAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#arrowCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterArrowCompoundAssignStmt(LuxParser.ArrowCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#arrowCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitArrowCompoundAssignStmt(LuxParser.ArrowCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#callStmt}.
	 * @param ctx the parse tree
	 */
	void enterCallStmt(LuxParser.CallStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#callStmt}.
	 * @param ctx the parse tree
	 */
	void exitCallStmt(LuxParser.CallStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#argList}.
	 * @param ctx the parse tree
	 */
	void enterArgList(LuxParser.ArgListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#argList}.
	 * @param ctx the parse tree
	 */
	void exitArgList(LuxParser.ArgListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#returnStmt}.
	 * @param ctx the parse tree
	 */
	void enterReturnStmt(LuxParser.ReturnStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#returnStmt}.
	 * @param ctx the parse tree
	 */
	void exitReturnStmt(LuxParser.ReturnStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#ifStmt}.
	 * @param ctx the parse tree
	 */
	void enterIfStmt(LuxParser.IfStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#ifStmt}.
	 * @param ctx the parse tree
	 */
	void exitIfStmt(LuxParser.IfStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#elseIfClause}.
	 * @param ctx the parse tree
	 */
	void enterElseIfClause(LuxParser.ElseIfClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#elseIfClause}.
	 * @param ctx the parse tree
	 */
	void exitElseIfClause(LuxParser.ElseIfClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#elseClause}.
	 * @param ctx the parse tree
	 */
	void enterElseClause(LuxParser.ElseClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#elseClause}.
	 * @param ctx the parse tree
	 */
	void exitElseClause(LuxParser.ElseClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#ifBody}.
	 * @param ctx the parse tree
	 */
	void enterIfBody(LuxParser.IfBodyContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#ifBody}.
	 * @param ctx the parse tree
	 */
	void exitIfBody(LuxParser.IfBodyContext ctx);
	/**
	 * Enter a parse tree produced by the {@code forInStmt}
	 * labeled alternative in {@link LuxParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void enterForInStmt(LuxParser.ForInStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code forInStmt}
	 * labeled alternative in {@link LuxParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void exitForInStmt(LuxParser.ForInStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code forClassicStmt}
	 * labeled alternative in {@link LuxParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void enterForClassicStmt(LuxParser.ForClassicStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code forClassicStmt}
	 * labeled alternative in {@link LuxParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void exitForClassicStmt(LuxParser.ForClassicStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#breakStmt}.
	 * @param ctx the parse tree
	 */
	void enterBreakStmt(LuxParser.BreakStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#breakStmt}.
	 * @param ctx the parse tree
	 */
	void exitBreakStmt(LuxParser.BreakStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#continueStmt}.
	 * @param ctx the parse tree
	 */
	void enterContinueStmt(LuxParser.ContinueStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#continueStmt}.
	 * @param ctx the parse tree
	 */
	void exitContinueStmt(LuxParser.ContinueStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#loopStmt}.
	 * @param ctx the parse tree
	 */
	void enterLoopStmt(LuxParser.LoopStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#loopStmt}.
	 * @param ctx the parse tree
	 */
	void exitLoopStmt(LuxParser.LoopStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#whileStmt}.
	 * @param ctx the parse tree
	 */
	void enterWhileStmt(LuxParser.WhileStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#whileStmt}.
	 * @param ctx the parse tree
	 */
	void exitWhileStmt(LuxParser.WhileStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#doWhileStmt}.
	 * @param ctx the parse tree
	 */
	void enterDoWhileStmt(LuxParser.DoWhileStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#doWhileStmt}.
	 * @param ctx the parse tree
	 */
	void exitDoWhileStmt(LuxParser.DoWhileStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#lockStmt}.
	 * @param ctx the parse tree
	 */
	void enterLockStmt(LuxParser.LockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#lockStmt}.
	 * @param ctx the parse tree
	 */
	void exitLockStmt(LuxParser.LockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#tryCatchStmt}.
	 * @param ctx the parse tree
	 */
	void enterTryCatchStmt(LuxParser.TryCatchStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#tryCatchStmt}.
	 * @param ctx the parse tree
	 */
	void exitTryCatchStmt(LuxParser.TryCatchStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#catchClause}.
	 * @param ctx the parse tree
	 */
	void enterCatchClause(LuxParser.CatchClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#catchClause}.
	 * @param ctx the parse tree
	 */
	void exitCatchClause(LuxParser.CatchClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#finallyClause}.
	 * @param ctx the parse tree
	 */
	void enterFinallyClause(LuxParser.FinallyClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#finallyClause}.
	 * @param ctx the parse tree
	 */
	void exitFinallyClause(LuxParser.FinallyClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#throwStmt}.
	 * @param ctx the parse tree
	 */
	void enterThrowStmt(LuxParser.ThrowStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#throwStmt}.
	 * @param ctx the parse tree
	 */
	void exitThrowStmt(LuxParser.ThrowStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#switchStmt}.
	 * @param ctx the parse tree
	 */
	void enterSwitchStmt(LuxParser.SwitchStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#switchStmt}.
	 * @param ctx the parse tree
	 */
	void exitSwitchStmt(LuxParser.SwitchStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#caseClause}.
	 * @param ctx the parse tree
	 */
	void enterCaseClause(LuxParser.CaseClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#caseClause}.
	 * @param ctx the parse tree
	 */
	void exitCaseClause(LuxParser.CaseClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#defaultClause}.
	 * @param ctx the parse tree
	 */
	void enterDefaultClause(LuxParser.DefaultClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#defaultClause}.
	 * @param ctx the parse tree
	 */
	void exitDefaultClause(LuxParser.DefaultClauseContext ctx);
	/**
	 * Enter a parse tree produced by the {@code structPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStructPosInitExpr(LuxParser.StructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code structPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStructPosInitExpr(LuxParser.StructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code fieldAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFieldAccessExpr(LuxParser.FieldAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code fieldAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFieldAccessExpr(LuxParser.FieldAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code typeofExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTypeofExpr(LuxParser.TypeofExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code typeofExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTypeofExpr(LuxParser.TypeofExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumNamedVariantExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumNamedVariantExpr(LuxParser.GenericEnumNamedVariantExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumNamedVariantExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumNamedVariantExpr(LuxParser.GenericEnumNamedVariantExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rshiftExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRshiftExpr(LuxParser.RshiftExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rshiftExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRshiftExpr(LuxParser.RshiftExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrowMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrowMethodCallExpr(LuxParser.ArrowMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrowMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrowMethodCallExpr(LuxParser.ArrowMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code octLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterOctLitExpr(LuxParser.OctLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code octLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitOctLitExpr(LuxParser.OctLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitXorExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitXorExpr(LuxParser.BitXorExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitXorExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitXorExpr(LuxParser.BitXorExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalNotExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalNotExpr(LuxParser.LogicalNotExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalNotExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalNotExpr(LuxParser.LogicalNotExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code identExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIdentExpr(LuxParser.IdentExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code identExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIdentExpr(LuxParser.IdentExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code preIncrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPreIncrExpr(LuxParser.PreIncrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code preIncrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPreIncrExpr(LuxParser.PreIncrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code ternaryExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTernaryExpr(LuxParser.TernaryExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code ternaryExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTernaryExpr(LuxParser.TernaryExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code chainedTupleIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterChainedTupleIndexExpr(LuxParser.ChainedTupleIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code chainedTupleIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitChainedTupleIndexExpr(LuxParser.ChainedTupleIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code nullLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNullLitExpr(LuxParser.NullLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code nullLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNullLitExpr(LuxParser.NullLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code mulExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterMulExpr(LuxParser.MulExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code mulExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitMulExpr(LuxParser.MulExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitAndExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitAndExpr(LuxParser.BitAndExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitAndExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitAndExpr(LuxParser.BitAndExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code isExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIsExpr(LuxParser.IsExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code isExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIsExpr(LuxParser.IsExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code lshiftExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLshiftExpr(LuxParser.LshiftExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code lshiftExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLshiftExpr(LuxParser.LshiftExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleLitExpr(LuxParser.TupleLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleLitExpr(LuxParser.TupleLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code propagateExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPropagateExpr(LuxParser.PropagateExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code propagateExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPropagateExpr(LuxParser.PropagateExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code addSubExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAddSubExpr(LuxParser.AddSubExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code addSubExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAddSubExpr(LuxParser.AddSubExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code intLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIntLitExpr(LuxParser.IntLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code intLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIntLitExpr(LuxParser.IntLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code addrOfExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAddrOfExpr(LuxParser.AddrOfExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code addrOfExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAddrOfExpr(LuxParser.AddrOfExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleIndexExpr(LuxParser.TupleIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleIndexExpr(LuxParser.TupleIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code floatLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFloatLitExpr(LuxParser.FloatLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code floatLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFloatLitExpr(LuxParser.FloatLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStructLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStructLitExpr(LuxParser.GenericStructLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStructLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStructLitExpr(LuxParser.GenericStructLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code spawnExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSpawnExpr(LuxParser.SpawnExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code spawnExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSpawnExpr(LuxParser.SpawnExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrowAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrowAccessExpr(LuxParser.ArrowAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrowAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrowAccessExpr(LuxParser.ArrowAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStaticMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStaticMethodCallExpr(LuxParser.GenericStaticMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStaticMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStaticMethodCallExpr(LuxParser.GenericStaticMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code listCompExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterListCompExpr(LuxParser.ListCompExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code listCompExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitListCompExpr(LuxParser.ListCompExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code indexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIndexExpr(LuxParser.IndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code indexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIndexExpr(LuxParser.IndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code negExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNegExpr(LuxParser.NegExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code negExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNegExpr(LuxParser.NegExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code derefExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterDerefExpr(LuxParser.DerefExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code derefExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitDerefExpr(LuxParser.DerefExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code preDecrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPreDecrExpr(LuxParser.PreDecrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code preDecrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPreDecrExpr(LuxParser.PreDecrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code spreadExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSpreadExpr(LuxParser.SpreadExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code spreadExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSpreadExpr(LuxParser.SpreadExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code catchUnwrapExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCatchUnwrapExpr(LuxParser.CatchUnwrapExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code catchUnwrapExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCatchUnwrapExpr(LuxParser.CatchUnwrapExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code staticMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStaticMethodCallExpr(LuxParser.StaticMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code staticMethodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStaticMethodCallExpr(LuxParser.StaticMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code nullCoalExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNullCoalExpr(LuxParser.NullCoalExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code nullCoalExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNullCoalExpr(LuxParser.NullCoalExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStructPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStructPosInitExpr(LuxParser.GenericStructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStructPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStructPosInitExpr(LuxParser.GenericStructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code castExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCastExpr(LuxParser.CastExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code castExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCastExpr(LuxParser.CastExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericFnCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericFnCallExpr(LuxParser.GenericFnCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericFnCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericFnCallExpr(LuxParser.GenericFnCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumAccessExpr(LuxParser.GenericEnumAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumAccessExpr(LuxParser.GenericEnumAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code enumAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterEnumAccessExpr(LuxParser.EnumAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code enumAccessExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitEnumAccessExpr(LuxParser.EnumAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code parenExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterParenExpr(LuxParser.ParenExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code parenExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitParenExpr(LuxParser.ParenExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitNotExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitNotExpr(LuxParser.BitNotExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitNotExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitNotExpr(LuxParser.BitNotExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrayLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrayLitExpr(LuxParser.ArrayLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrayLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrayLitExpr(LuxParser.ArrayLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code methodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterMethodCallExpr(LuxParser.MethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code methodCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitMethodCallExpr(LuxParser.MethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code leadingDotFloatLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLeadingDotFloatLitExpr(LuxParser.LeadingDotFloatLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code leadingDotFloatLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLeadingDotFloatLitExpr(LuxParser.LeadingDotFloatLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code structLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStructLitExpr(LuxParser.StructLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code structLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStructLitExpr(LuxParser.StructLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code postDecrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPostDecrExpr(LuxParser.PostDecrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code postDecrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPostDecrExpr(LuxParser.PostDecrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code relExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRelExpr(LuxParser.RelExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code relExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRelExpr(LuxParser.RelExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code binLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBinLitExpr(LuxParser.BinLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code binLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBinLitExpr(LuxParser.BinLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rangeInclExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRangeInclExpr(LuxParser.RangeInclExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rangeInclExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRangeInclExpr(LuxParser.RangeInclExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleArrowIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleArrowIndexExpr(LuxParser.TupleArrowIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleArrowIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleArrowIndexExpr(LuxParser.TupleArrowIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalAndExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalAndExpr(LuxParser.LogicalAndExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalAndExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalAndExpr(LuxParser.LogicalAndExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code qualifiedStructPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterQualifiedStructPosInitExpr(LuxParser.QualifiedStructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code qualifiedStructPosInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitQualifiedStructPosInitExpr(LuxParser.QualifiedStructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code strLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStrLitExpr(LuxParser.StrLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code strLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStrLitExpr(LuxParser.StrLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code awaitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAwaitExpr(LuxParser.AwaitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code awaitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAwaitExpr(LuxParser.AwaitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code qualifiedStructNamedInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterQualifiedStructNamedInitExpr(LuxParser.QualifiedStructNamedInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code qualifiedStructNamedInitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitQualifiedStructNamedInitExpr(LuxParser.QualifiedStructNamedInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code cStrLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCStrLitExpr(LuxParser.CStrLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code cStrLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCStrLitExpr(LuxParser.CStrLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code fnCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFnCallExpr(LuxParser.FnCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code fnCallExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFnCallExpr(LuxParser.FnCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalOrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalOrExpr(LuxParser.LogicalOrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalOrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalOrExpr(LuxParser.LogicalOrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code sizeofExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSizeofExpr(LuxParser.SizeofExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code sizeofExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSizeofExpr(LuxParser.SizeofExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code eqExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterEqExpr(LuxParser.EqExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code eqExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitEqExpr(LuxParser.EqExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitOrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitOrExpr(LuxParser.BitOrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitOrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitOrExpr(LuxParser.BitOrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code charLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCharLitExpr(LuxParser.CharLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code charLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCharLitExpr(LuxParser.CharLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tryExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTryExpr(LuxParser.TryExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tryExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTryExpr(LuxParser.TryExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code postIncrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPostIncrExpr(LuxParser.PostIncrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code postIncrExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPostIncrExpr(LuxParser.PostIncrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumPosVariantExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumPosVariantExpr(LuxParser.GenericEnumPosVariantExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumPosVariantExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumPosVariantExpr(LuxParser.GenericEnumPosVariantExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code boolLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBoolLitExpr(LuxParser.BoolLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code boolLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBoolLitExpr(LuxParser.BoolLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code hexLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterHexLitExpr(LuxParser.HexLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code hexLitExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitHexLitExpr(LuxParser.HexLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code chainedTupleArrowIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterChainedTupleArrowIndexExpr(LuxParser.ChainedTupleArrowIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code chainedTupleArrowIndexExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitChainedTupleArrowIndexExpr(LuxParser.ChainedTupleArrowIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rangeExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRangeExpr(LuxParser.RangeExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rangeExpr}
	 * labeled alternative in {@link LuxParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRangeExpr(LuxParser.RangeExprContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#typeSpec}.
	 * @param ctx the parse tree
	 */
	void enterTypeSpec(LuxParser.TypeSpecContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#typeSpec}.
	 * @param ctx the parse tree
	 */
	void exitTypeSpec(LuxParser.TypeSpecContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#fnTypeSpec}.
	 * @param ctx the parse tree
	 */
	void enterFnTypeSpec(LuxParser.FnTypeSpecContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#fnTypeSpec}.
	 * @param ctx the parse tree
	 */
	void exitFnTypeSpec(LuxParser.FnTypeSpecContext ctx);
	/**
	 * Enter a parse tree produced by {@link LuxParser#primitiveType}.
	 * @param ctx the parse tree
	 */
	void enterPrimitiveType(LuxParser.PrimitiveTypeContext ctx);
	/**
	 * Exit a parse tree produced by {@link LuxParser#primitiveType}.
	 * @param ctx the parse tree
	 */
	void exitPrimitiveType(LuxParser.PrimitiveTypeContext ctx);
}