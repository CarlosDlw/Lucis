// Generated from /home/carlos/Projects/Cpp/Lucis/grammar/LucisParser.g4 by ANTLR 4.13.1
import org.antlr.v4.runtime.tree.ParseTreeListener;

/**
 * This interface defines a complete listener for a parse tree produced by
 * {@link LucisParser}.
 */
public interface LucisParserListener extends ParseTreeListener {
	/**
	 * Enter a parse tree produced by {@link LucisParser#program}.
	 * @param ctx the parse tree
	 */
	void enterProgram(LucisParser.ProgramContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#program}.
	 * @param ctx the parse tree
	 */
	void exitProgram(LucisParser.ProgramContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#preambleDecl}.
	 * @param ctx the parse tree
	 */
	void enterPreambleDecl(LucisParser.PreambleDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#preambleDecl}.
	 * @param ctx the parse tree
	 */
	void exitPreambleDecl(LucisParser.PreambleDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#namespaceDecl}.
	 * @param ctx the parse tree
	 */
	void enterNamespaceDecl(LucisParser.NamespaceDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#namespaceDecl}.
	 * @param ctx the parse tree
	 */
	void exitNamespaceDecl(LucisParser.NamespaceDeclContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useRoot}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseRoot(LucisParser.UseRootContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useRoot}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseRoot(LucisParser.UseRootContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useItem}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseItem(LucisParser.UseItemContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useItem}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseItem(LucisParser.UseItemContext ctx);
	/**
	 * Enter a parse tree produced by the {@code useGroup}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void enterUseGroup(LucisParser.UseGroupContext ctx);
	/**
	 * Exit a parse tree produced by the {@code useGroup}
	 * labeled alternative in {@link LucisParser#useDecl}.
	 * @param ctx the parse tree
	 */
	void exitUseGroup(LucisParser.UseGroupContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#modulePath}.
	 * @param ctx the parse tree
	 */
	void enterModulePath(LucisParser.ModulePathContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#modulePath}.
	 * @param ctx the parse tree
	 */
	void exitModulePath(LucisParser.ModulePathContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#includeDecl}.
	 * @param ctx the parse tree
	 */
	void enterIncludeDecl(LucisParser.IncludeDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#includeDecl}.
	 * @param ctx the parse tree
	 */
	void exitIncludeDecl(LucisParser.IncludeDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#topLevelDecl}.
	 * @param ctx the parse tree
	 */
	void enterTopLevelDecl(LucisParser.TopLevelDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#topLevelDecl}.
	 * @param ctx the parse tree
	 */
	void exitTopLevelDecl(LucisParser.TopLevelDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#typeAliasDecl}.
	 * @param ctx the parse tree
	 */
	void enterTypeAliasDecl(LucisParser.TypeAliasDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#typeAliasDecl}.
	 * @param ctx the parse tree
	 */
	void exitTypeAliasDecl(LucisParser.TypeAliasDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#enumDecl}.
	 * @param ctx the parse tree
	 */
	void enterEnumDecl(LucisParser.EnumDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#enumDecl}.
	 * @param ctx the parse tree
	 */
	void exitEnumDecl(LucisParser.EnumDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#enumVariant}.
	 * @param ctx the parse tree
	 */
	void enterEnumVariant(LucisParser.EnumVariantContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#enumVariant}.
	 * @param ctx the parse tree
	 */
	void exitEnumVariant(LucisParser.EnumVariantContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#enumPayloadField}.
	 * @param ctx the parse tree
	 */
	void enterEnumPayloadField(LucisParser.EnumPayloadFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#enumPayloadField}.
	 * @param ctx the parse tree
	 */
	void exitEnumPayloadField(LucisParser.EnumPayloadFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#structDecl}.
	 * @param ctx the parse tree
	 */
	void enterStructDecl(LucisParser.StructDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#structDecl}.
	 * @param ctx the parse tree
	 */
	void exitStructDecl(LucisParser.StructDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#structField}.
	 * @param ctx the parse tree
	 */
	void enterStructField(LucisParser.StructFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#structField}.
	 * @param ctx the parse tree
	 */
	void exitStructField(LucisParser.StructFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#unionDecl}.
	 * @param ctx the parse tree
	 */
	void enterUnionDecl(LucisParser.UnionDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#unionDecl}.
	 * @param ctx the parse tree
	 */
	void exitUnionDecl(LucisParser.UnionDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#unionField}.
	 * @param ctx the parse tree
	 */
	void enterUnionField(LucisParser.UnionFieldContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#unionField}.
	 * @param ctx the parse tree
	 */
	void exitUnionField(LucisParser.UnionFieldContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#externDecl}.
	 * @param ctx the parse tree
	 */
	void enterExternDecl(LucisParser.ExternDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#externDecl}.
	 * @param ctx the parse tree
	 */
	void exitExternDecl(LucisParser.ExternDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#externParamList}.
	 * @param ctx the parse tree
	 */
	void enterExternParamList(LucisParser.ExternParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#externParamList}.
	 * @param ctx the parse tree
	 */
	void exitExternParamList(LucisParser.ExternParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#externParam}.
	 * @param ctx the parse tree
	 */
	void enterExternParam(LucisParser.ExternParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#externParam}.
	 * @param ctx the parse tree
	 */
	void exitExternParam(LucisParser.ExternParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#functionDecl}.
	 * @param ctx the parse tree
	 */
	void enterFunctionDecl(LucisParser.FunctionDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#functionDecl}.
	 * @param ctx the parse tree
	 */
	void exitFunctionDecl(LucisParser.FunctionDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#extendDecl}.
	 * @param ctx the parse tree
	 */
	void enterExtendDecl(LucisParser.ExtendDeclContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#extendDecl}.
	 * @param ctx the parse tree
	 */
	void exitExtendDecl(LucisParser.ExtendDeclContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#typeParamList}.
	 * @param ctx the parse tree
	 */
	void enterTypeParamList(LucisParser.TypeParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#typeParamList}.
	 * @param ctx the parse tree
	 */
	void exitTypeParamList(LucisParser.TypeParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#typeParam}.
	 * @param ctx the parse tree
	 */
	void enterTypeParam(LucisParser.TypeParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#typeParam}.
	 * @param ctx the parse tree
	 */
	void exitTypeParam(LucisParser.TypeParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#extendMethod}.
	 * @param ctx the parse tree
	 */
	void enterExtendMethod(LucisParser.ExtendMethodContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#extendMethod}.
	 * @param ctx the parse tree
	 */
	void exitExtendMethod(LucisParser.ExtendMethodContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#paramList}.
	 * @param ctx the parse tree
	 */
	void enterParamList(LucisParser.ParamListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#paramList}.
	 * @param ctx the parse tree
	 */
	void exitParamList(LucisParser.ParamListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#param}.
	 * @param ctx the parse tree
	 */
	void enterParam(LucisParser.ParamContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#param}.
	 * @param ctx the parse tree
	 */
	void exitParam(LucisParser.ParamContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#block}.
	 * @param ctx the parse tree
	 */
	void enterBlock(LucisParser.BlockContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#block}.
	 * @param ctx the parse tree
	 */
	void exitBlock(LucisParser.BlockContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#statement}.
	 * @param ctx the parse tree
	 */
	void enterStatement(LucisParser.StatementContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#statement}.
	 * @param ctx the parse tree
	 */
	void exitStatement(LucisParser.StatementContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#deferStmt}.
	 * @param ctx the parse tree
	 */
	void enterDeferStmt(LucisParser.DeferStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#deferStmt}.
	 * @param ctx the parse tree
	 */
	void exitDeferStmt(LucisParser.DeferStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#nakedBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterNakedBlockStmt(LucisParser.NakedBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#nakedBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitNakedBlockStmt(LucisParser.NakedBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#inlineBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterInlineBlockStmt(LucisParser.InlineBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#inlineBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitInlineBlockStmt(LucisParser.InlineBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#scopeBlockStmt}.
	 * @param ctx the parse tree
	 */
	void enterScopeBlockStmt(LucisParser.ScopeBlockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#scopeBlockStmt}.
	 * @param ctx the parse tree
	 */
	void exitScopeBlockStmt(LucisParser.ScopeBlockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#scopeCallbackList}.
	 * @param ctx the parse tree
	 */
	void enterScopeCallbackList(LucisParser.ScopeCallbackListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#scopeCallbackList}.
	 * @param ctx the parse tree
	 */
	void exitScopeCallbackList(LucisParser.ScopeCallbackListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#scopeCallback}.
	 * @param ctx the parse tree
	 */
	void enterScopeCallback(LucisParser.ScopeCallbackContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#scopeCallback}.
	 * @param ctx the parse tree
	 */
	void exitScopeCallback(LucisParser.ScopeCallbackContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#exprStmt}.
	 * @param ctx the parse tree
	 */
	void enterExprStmt(LucisParser.ExprStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#exprStmt}.
	 * @param ctx the parse tree
	 */
	void exitExprStmt(LucisParser.ExprStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#varDeclStmt}.
	 * @param ctx the parse tree
	 */
	void enterVarDeclStmt(LucisParser.VarDeclStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#varDeclStmt}.
	 * @param ctx the parse tree
	 */
	void exitVarDeclStmt(LucisParser.VarDeclStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#assignStmt}.
	 * @param ctx the parse tree
	 */
	void enterAssignStmt(LucisParser.AssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#assignStmt}.
	 * @param ctx the parse tree
	 */
	void exitAssignStmt(LucisParser.AssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#compoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterCompoundAssignStmt(LucisParser.CompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#compoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitCompoundAssignStmt(LucisParser.CompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#fieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldAssignStmt(LucisParser.FieldAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#fieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldAssignStmt(LucisParser.FieldAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#fieldCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldCompoundAssignStmt(LucisParser.FieldCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#fieldCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldCompoundAssignStmt(LucisParser.FieldCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#indexFieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterIndexFieldAssignStmt(LucisParser.IndexFieldAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#indexFieldAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitIndexFieldAssignStmt(LucisParser.IndexFieldAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#fieldIndexAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterFieldIndexAssignStmt(LucisParser.FieldIndexAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#fieldIndexAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitFieldIndexAssignStmt(LucisParser.FieldIndexAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#derefAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterDerefAssignStmt(LucisParser.DerefAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#derefAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitDerefAssignStmt(LucisParser.DerefAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#derefCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterDerefCompoundAssignStmt(LucisParser.DerefCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#derefCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitDerefCompoundAssignStmt(LucisParser.DerefCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#arrowAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterArrowAssignStmt(LucisParser.ArrowAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#arrowAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitArrowAssignStmt(LucisParser.ArrowAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#arrowCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void enterArrowCompoundAssignStmt(LucisParser.ArrowCompoundAssignStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#arrowCompoundAssignStmt}.
	 * @param ctx the parse tree
	 */
	void exitArrowCompoundAssignStmt(LucisParser.ArrowCompoundAssignStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#callStmt}.
	 * @param ctx the parse tree
	 */
	void enterCallStmt(LucisParser.CallStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#callStmt}.
	 * @param ctx the parse tree
	 */
	void exitCallStmt(LucisParser.CallStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#argList}.
	 * @param ctx the parse tree
	 */
	void enterArgList(LucisParser.ArgListContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#argList}.
	 * @param ctx the parse tree
	 */
	void exitArgList(LucisParser.ArgListContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#returnStmt}.
	 * @param ctx the parse tree
	 */
	void enterReturnStmt(LucisParser.ReturnStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#returnStmt}.
	 * @param ctx the parse tree
	 */
	void exitReturnStmt(LucisParser.ReturnStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#ifStmt}.
	 * @param ctx the parse tree
	 */
	void enterIfStmt(LucisParser.IfStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#ifStmt}.
	 * @param ctx the parse tree
	 */
	void exitIfStmt(LucisParser.IfStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#elseIfClause}.
	 * @param ctx the parse tree
	 */
	void enterElseIfClause(LucisParser.ElseIfClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#elseIfClause}.
	 * @param ctx the parse tree
	 */
	void exitElseIfClause(LucisParser.ElseIfClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#elseClause}.
	 * @param ctx the parse tree
	 */
	void enterElseClause(LucisParser.ElseClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#elseClause}.
	 * @param ctx the parse tree
	 */
	void exitElseClause(LucisParser.ElseClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#ifBody}.
	 * @param ctx the parse tree
	 */
	void enterIfBody(LucisParser.IfBodyContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#ifBody}.
	 * @param ctx the parse tree
	 */
	void exitIfBody(LucisParser.IfBodyContext ctx);
	/**
	 * Enter a parse tree produced by the {@code forInStmt}
	 * labeled alternative in {@link LucisParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void enterForInStmt(LucisParser.ForInStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code forInStmt}
	 * labeled alternative in {@link LucisParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void exitForInStmt(LucisParser.ForInStmtContext ctx);
	/**
	 * Enter a parse tree produced by the {@code forClassicStmt}
	 * labeled alternative in {@link LucisParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void enterForClassicStmt(LucisParser.ForClassicStmtContext ctx);
	/**
	 * Exit a parse tree produced by the {@code forClassicStmt}
	 * labeled alternative in {@link LucisParser#forStmt}.
	 * @param ctx the parse tree
	 */
	void exitForClassicStmt(LucisParser.ForClassicStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#breakStmt}.
	 * @param ctx the parse tree
	 */
	void enterBreakStmt(LucisParser.BreakStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#breakStmt}.
	 * @param ctx the parse tree
	 */
	void exitBreakStmt(LucisParser.BreakStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#continueStmt}.
	 * @param ctx the parse tree
	 */
	void enterContinueStmt(LucisParser.ContinueStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#continueStmt}.
	 * @param ctx the parse tree
	 */
	void exitContinueStmt(LucisParser.ContinueStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#loopStmt}.
	 * @param ctx the parse tree
	 */
	void enterLoopStmt(LucisParser.LoopStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#loopStmt}.
	 * @param ctx the parse tree
	 */
	void exitLoopStmt(LucisParser.LoopStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#whileStmt}.
	 * @param ctx the parse tree
	 */
	void enterWhileStmt(LucisParser.WhileStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#whileStmt}.
	 * @param ctx the parse tree
	 */
	void exitWhileStmt(LucisParser.WhileStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#doWhileStmt}.
	 * @param ctx the parse tree
	 */
	void enterDoWhileStmt(LucisParser.DoWhileStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#doWhileStmt}.
	 * @param ctx the parse tree
	 */
	void exitDoWhileStmt(LucisParser.DoWhileStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#lockStmt}.
	 * @param ctx the parse tree
	 */
	void enterLockStmt(LucisParser.LockStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#lockStmt}.
	 * @param ctx the parse tree
	 */
	void exitLockStmt(LucisParser.LockStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#tryCatchStmt}.
	 * @param ctx the parse tree
	 */
	void enterTryCatchStmt(LucisParser.TryCatchStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#tryCatchStmt}.
	 * @param ctx the parse tree
	 */
	void exitTryCatchStmt(LucisParser.TryCatchStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#catchClause}.
	 * @param ctx the parse tree
	 */
	void enterCatchClause(LucisParser.CatchClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#catchClause}.
	 * @param ctx the parse tree
	 */
	void exitCatchClause(LucisParser.CatchClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#finallyClause}.
	 * @param ctx the parse tree
	 */
	void enterFinallyClause(LucisParser.FinallyClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#finallyClause}.
	 * @param ctx the parse tree
	 */
	void exitFinallyClause(LucisParser.FinallyClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#throwStmt}.
	 * @param ctx the parse tree
	 */
	void enterThrowStmt(LucisParser.ThrowStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#throwStmt}.
	 * @param ctx the parse tree
	 */
	void exitThrowStmt(LucisParser.ThrowStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#switchStmt}.
	 * @param ctx the parse tree
	 */
	void enterSwitchStmt(LucisParser.SwitchStmtContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#switchStmt}.
	 * @param ctx the parse tree
	 */
	void exitSwitchStmt(LucisParser.SwitchStmtContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#caseClause}.
	 * @param ctx the parse tree
	 */
	void enterCaseClause(LucisParser.CaseClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#caseClause}.
	 * @param ctx the parse tree
	 */
	void exitCaseClause(LucisParser.CaseClauseContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#defaultClause}.
	 * @param ctx the parse tree
	 */
	void enterDefaultClause(LucisParser.DefaultClauseContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#defaultClause}.
	 * @param ctx the parse tree
	 */
	void exitDefaultClause(LucisParser.DefaultClauseContext ctx);
	/**
	 * Enter a parse tree produced by the {@code structPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStructPosInitExpr(LucisParser.StructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code structPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStructPosInitExpr(LucisParser.StructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code fieldAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFieldAccessExpr(LucisParser.FieldAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code fieldAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFieldAccessExpr(LucisParser.FieldAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code typeofExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTypeofExpr(LucisParser.TypeofExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code typeofExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTypeofExpr(LucisParser.TypeofExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumNamedVariantExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumNamedVariantExpr(LucisParser.GenericEnumNamedVariantExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumNamedVariantExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumNamedVariantExpr(LucisParser.GenericEnumNamedVariantExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rshiftExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRshiftExpr(LucisParser.RshiftExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rshiftExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRshiftExpr(LucisParser.RshiftExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrowMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrowMethodCallExpr(LucisParser.ArrowMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrowMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrowMethodCallExpr(LucisParser.ArrowMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code octLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterOctLitExpr(LucisParser.OctLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code octLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitOctLitExpr(LucisParser.OctLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitXorExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitXorExpr(LucisParser.BitXorExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitXorExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitXorExpr(LucisParser.BitXorExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalNotExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalNotExpr(LucisParser.LogicalNotExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalNotExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalNotExpr(LucisParser.LogicalNotExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code identExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIdentExpr(LucisParser.IdentExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code identExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIdentExpr(LucisParser.IdentExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code preIncrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPreIncrExpr(LucisParser.PreIncrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code preIncrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPreIncrExpr(LucisParser.PreIncrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code ternaryExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTernaryExpr(LucisParser.TernaryExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code ternaryExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTernaryExpr(LucisParser.TernaryExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code chainedTupleIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterChainedTupleIndexExpr(LucisParser.ChainedTupleIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code chainedTupleIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitChainedTupleIndexExpr(LucisParser.ChainedTupleIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code nullLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNullLitExpr(LucisParser.NullLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code nullLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNullLitExpr(LucisParser.NullLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code mulExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterMulExpr(LucisParser.MulExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code mulExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitMulExpr(LucisParser.MulExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitAndExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitAndExpr(LucisParser.BitAndExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitAndExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitAndExpr(LucisParser.BitAndExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code isExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIsExpr(LucisParser.IsExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code isExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIsExpr(LucisParser.IsExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code lshiftExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLshiftExpr(LucisParser.LshiftExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code lshiftExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLshiftExpr(LucisParser.LshiftExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleLitExpr(LucisParser.TupleLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleLitExpr(LucisParser.TupleLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code propagateExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPropagateExpr(LucisParser.PropagateExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code propagateExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPropagateExpr(LucisParser.PropagateExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code addSubExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAddSubExpr(LucisParser.AddSubExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code addSubExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAddSubExpr(LucisParser.AddSubExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code intLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIntLitExpr(LucisParser.IntLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code intLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIntLitExpr(LucisParser.IntLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code addrOfExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAddrOfExpr(LucisParser.AddrOfExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code addrOfExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAddrOfExpr(LucisParser.AddrOfExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleIndexExpr(LucisParser.TupleIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleIndexExpr(LucisParser.TupleIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code floatLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFloatLitExpr(LucisParser.FloatLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code floatLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFloatLitExpr(LucisParser.FloatLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStructLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStructLitExpr(LucisParser.GenericStructLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStructLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStructLitExpr(LucisParser.GenericStructLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code spawnExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSpawnExpr(LucisParser.SpawnExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code spawnExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSpawnExpr(LucisParser.SpawnExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrowAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrowAccessExpr(LucisParser.ArrowAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrowAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrowAccessExpr(LucisParser.ArrowAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStaticMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStaticMethodCallExpr(LucisParser.GenericStaticMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStaticMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStaticMethodCallExpr(LucisParser.GenericStaticMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code listCompExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterListCompExpr(LucisParser.ListCompExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code listCompExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitListCompExpr(LucisParser.ListCompExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code indexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterIndexExpr(LucisParser.IndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code indexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitIndexExpr(LucisParser.IndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code negExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNegExpr(LucisParser.NegExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code negExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNegExpr(LucisParser.NegExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code derefExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterDerefExpr(LucisParser.DerefExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code derefExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitDerefExpr(LucisParser.DerefExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code preDecrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPreDecrExpr(LucisParser.PreDecrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code preDecrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPreDecrExpr(LucisParser.PreDecrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code spreadExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSpreadExpr(LucisParser.SpreadExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code spreadExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSpreadExpr(LucisParser.SpreadExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code catchUnwrapExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCatchUnwrapExpr(LucisParser.CatchUnwrapExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code catchUnwrapExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCatchUnwrapExpr(LucisParser.CatchUnwrapExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code staticMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStaticMethodCallExpr(LucisParser.StaticMethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code staticMethodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStaticMethodCallExpr(LucisParser.StaticMethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code nullCoalExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterNullCoalExpr(LucisParser.NullCoalExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code nullCoalExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitNullCoalExpr(LucisParser.NullCoalExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericStructPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericStructPosInitExpr(LucisParser.GenericStructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericStructPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericStructPosInitExpr(LucisParser.GenericStructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code castExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCastExpr(LucisParser.CastExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code castExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCastExpr(LucisParser.CastExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericFnCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericFnCallExpr(LucisParser.GenericFnCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericFnCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericFnCallExpr(LucisParser.GenericFnCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumAccessExpr(LucisParser.GenericEnumAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumAccessExpr(LucisParser.GenericEnumAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code enumAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterEnumAccessExpr(LucisParser.EnumAccessExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code enumAccessExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitEnumAccessExpr(LucisParser.EnumAccessExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code parenExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterParenExpr(LucisParser.ParenExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code parenExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitParenExpr(LucisParser.ParenExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitNotExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitNotExpr(LucisParser.BitNotExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitNotExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitNotExpr(LucisParser.BitNotExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code arrayLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterArrayLitExpr(LucisParser.ArrayLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code arrayLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitArrayLitExpr(LucisParser.ArrayLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code methodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterMethodCallExpr(LucisParser.MethodCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code methodCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitMethodCallExpr(LucisParser.MethodCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code leadingDotFloatLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLeadingDotFloatLitExpr(LucisParser.LeadingDotFloatLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code leadingDotFloatLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLeadingDotFloatLitExpr(LucisParser.LeadingDotFloatLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code structLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStructLitExpr(LucisParser.StructLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code structLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStructLitExpr(LucisParser.StructLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code postDecrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPostDecrExpr(LucisParser.PostDecrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code postDecrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPostDecrExpr(LucisParser.PostDecrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code relExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRelExpr(LucisParser.RelExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code relExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRelExpr(LucisParser.RelExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code binLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBinLitExpr(LucisParser.BinLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code binLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBinLitExpr(LucisParser.BinLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rangeInclExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRangeInclExpr(LucisParser.RangeInclExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rangeInclExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRangeInclExpr(LucisParser.RangeInclExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tupleArrowIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTupleArrowIndexExpr(LucisParser.TupleArrowIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tupleArrowIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTupleArrowIndexExpr(LucisParser.TupleArrowIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalAndExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalAndExpr(LucisParser.LogicalAndExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalAndExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalAndExpr(LucisParser.LogicalAndExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code qualifiedStructPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterQualifiedStructPosInitExpr(LucisParser.QualifiedStructPosInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code qualifiedStructPosInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitQualifiedStructPosInitExpr(LucisParser.QualifiedStructPosInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code strLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterStrLitExpr(LucisParser.StrLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code strLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitStrLitExpr(LucisParser.StrLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code awaitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterAwaitExpr(LucisParser.AwaitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code awaitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitAwaitExpr(LucisParser.AwaitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code qualifiedStructNamedInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterQualifiedStructNamedInitExpr(LucisParser.QualifiedStructNamedInitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code qualifiedStructNamedInitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitQualifiedStructNamedInitExpr(LucisParser.QualifiedStructNamedInitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code cStrLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCStrLitExpr(LucisParser.CStrLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code cStrLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCStrLitExpr(LucisParser.CStrLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code fnCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterFnCallExpr(LucisParser.FnCallExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code fnCallExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitFnCallExpr(LucisParser.FnCallExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code logicalOrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterLogicalOrExpr(LucisParser.LogicalOrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code logicalOrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitLogicalOrExpr(LucisParser.LogicalOrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code sizeofExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterSizeofExpr(LucisParser.SizeofExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code sizeofExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitSizeofExpr(LucisParser.SizeofExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code eqExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterEqExpr(LucisParser.EqExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code eqExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitEqExpr(LucisParser.EqExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code bitOrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBitOrExpr(LucisParser.BitOrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code bitOrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBitOrExpr(LucisParser.BitOrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code charLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterCharLitExpr(LucisParser.CharLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code charLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitCharLitExpr(LucisParser.CharLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code tryExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterTryExpr(LucisParser.TryExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code tryExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitTryExpr(LucisParser.TryExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code postIncrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterPostIncrExpr(LucisParser.PostIncrExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code postIncrExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitPostIncrExpr(LucisParser.PostIncrExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code genericEnumPosVariantExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterGenericEnumPosVariantExpr(LucisParser.GenericEnumPosVariantExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code genericEnumPosVariantExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitGenericEnumPosVariantExpr(LucisParser.GenericEnumPosVariantExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code boolLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterBoolLitExpr(LucisParser.BoolLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code boolLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitBoolLitExpr(LucisParser.BoolLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code hexLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterHexLitExpr(LucisParser.HexLitExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code hexLitExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitHexLitExpr(LucisParser.HexLitExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code chainedTupleArrowIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterChainedTupleArrowIndexExpr(LucisParser.ChainedTupleArrowIndexExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code chainedTupleArrowIndexExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitChainedTupleArrowIndexExpr(LucisParser.ChainedTupleArrowIndexExprContext ctx);
	/**
	 * Enter a parse tree produced by the {@code rangeExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void enterRangeExpr(LucisParser.RangeExprContext ctx);
	/**
	 * Exit a parse tree produced by the {@code rangeExpr}
	 * labeled alternative in {@link LucisParser#expression}.
	 * @param ctx the parse tree
	 */
	void exitRangeExpr(LucisParser.RangeExprContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#typeSpec}.
	 * @param ctx the parse tree
	 */
	void enterTypeSpec(LucisParser.TypeSpecContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#typeSpec}.
	 * @param ctx the parse tree
	 */
	void exitTypeSpec(LucisParser.TypeSpecContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#fnTypeSpec}.
	 * @param ctx the parse tree
	 */
	void enterFnTypeSpec(LucisParser.FnTypeSpecContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#fnTypeSpec}.
	 * @param ctx the parse tree
	 */
	void exitFnTypeSpec(LucisParser.FnTypeSpecContext ctx);
	/**
	 * Enter a parse tree produced by {@link LucisParser#primitiveType}.
	 * @param ctx the parse tree
	 */
	void enterPrimitiveType(LucisParser.PrimitiveTypeContext ctx);
	/**
	 * Exit a parse tree produced by {@link LucisParser#primitiveType}.
	 * @param ctx the parse tree
	 */
	void exitPrimitiveType(LucisParser.PrimitiveTypeContext ctx);
}