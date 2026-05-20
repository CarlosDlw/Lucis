// Generated from /home/carlos/Projects/Cpp/Lux/grammar/LuxParser.g4 by ANTLR 4.13.1
import org.antlr.v4.runtime.atn.*;
import org.antlr.v4.runtime.dfa.DFA;
import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.misc.*;
import org.antlr.v4.runtime.tree.*;
import java.util.List;
import java.util.Iterator;
import java.util.ArrayList;

@SuppressWarnings({"all", "warnings", "unchecked", "unused", "cast", "CheckReturnValue"})
public class LuxParser extends Parser {
	static { RuntimeMetaData.checkVersion("4.13.1", RuntimeMetaData.VERSION); }

	protected static final DFA[] _decisionToDFA;
	protected static final PredictionContextCache _sharedContextCache =
		new PredictionContextCache();
	public static final int
		NAMESPACE=1, USE=2, RET=3, STRUCT=4, UNION=5, ENUM=6, NULL_LIT=7, FN=8, 
		TYPE=9, AS=10, IS=11, SIZEOF=12, TYPEOF=13, IF=14, ELSE=15, FOR=16, IN=17, 
		LOOP=18, WHILE=19, DO=20, BREAK=21, CONTINUE=22, SWITCH=23, CASE=24, DEFAULT=25, 
		SPAWN=26, AWAIT=27, LOCK=28, EXTEND=29, TRY=30, CATCH=31, FINALLY=32, 
		THROW=33, DEFER=34, EXTERN=35, AUTO=36, VEC=37, MAP=38, SET=39, TUPLE=40, 
		ARROW=41, INCLUDE_SYS=42, INCLUDE_LOCAL=43, INLINE_BLOCK=44, SCOPE_BLOCK=45, 
		NULLCOAL=46, SPREAD=47, RANGE_INCL=48, RANGE=49, INT1=50, INT8=51, INT16=52, 
		INT32=53, INT64=54, INT128=55, INTINF=56, ISIZE=57, UINT1=58, UINT8=59, 
		UINT16=60, UINT32=61, UINT64=62, UINT128=63, USIZE=64, FLOAT32=65, FLOAT64=66, 
		FLOAT80=67, FLOAT128=68, DOUBLE=69, BOOL=70, CHAR=71, VOID=72, STRING=73, 
		CSTRING=74, HEX_LIT=75, OCT_LIT=76, BIN_LIT=77, INT_LIT=78, FLOAT_LIT=79, 
		BOOL_LIT=80, C_STR_LIT=81, STR_LIT=82, CHAR_LIT=83, IDENTIFIER=84, PLUS_ASSIGN=85, 
		MINUS_ASSIGN=86, STAR_ASSIGN=87, SLASH_ASSIGN=88, PERCENT_ASSIGN=89, AMP_ASSIGN=90, 
		PIPE_ASSIGN=91, CARET_ASSIGN=92, LSHIFT_ASSIGN=93, RSHIFT_ASSIGN=94, SEMI=95, 
		COLON=96, SCOPE=97, COMMA=98, DOT=99, ASSIGN=100, LPAREN=101, RPAREN=102, 
		LBRACE=103, RBRACE=104, LBRACKET=105, RBRACKET=106, STAR=107, AMPERSAND=108, 
		MINUS=109, PLUS=110, SLASH=111, PERCENT=112, EQ=113, NEQ=114, LTE=115, 
		GTE=116, LT=117, GT=118, LAND=119, LOR=120, NOT=121, INCR=122, DECR=123, 
		LSHIFT=124, PIPE=125, CARET=126, TILDE=127, QUESTION=128, WS=129, LINE_COMMENT=130, 
		BLOCK_COMMENT=131;
	public static final int
		RULE_program = 0, RULE_preambleDecl = 1, RULE_namespaceDecl = 2, RULE_useDecl = 3, 
		RULE_modulePath = 4, RULE_includeDecl = 5, RULE_topLevelDecl = 6, RULE_typeAliasDecl = 7, 
		RULE_enumDecl = 8, RULE_enumVariant = 9, RULE_enumPayloadField = 10, RULE_structDecl = 11, 
		RULE_structField = 12, RULE_unionDecl = 13, RULE_unionField = 14, RULE_externDecl = 15, 
		RULE_externParamList = 16, RULE_externParam = 17, RULE_functionDecl = 18, 
		RULE_extendDecl = 19, RULE_typeParamList = 20, RULE_typeParam = 21, RULE_extendMethod = 22, 
		RULE_paramList = 23, RULE_param = 24, RULE_block = 25, RULE_statement = 26, 
		RULE_deferStmt = 27, RULE_nakedBlockStmt = 28, RULE_inlineBlockStmt = 29, 
		RULE_scopeBlockStmt = 30, RULE_scopeCallbackList = 31, RULE_scopeCallback = 32, 
		RULE_exprStmt = 33, RULE_varDeclStmt = 34, RULE_assignStmt = 35, RULE_compoundAssignStmt = 36, 
		RULE_fieldAssignStmt = 37, RULE_fieldCompoundAssignStmt = 38, RULE_indexFieldAssignStmt = 39, 
		RULE_fieldIndexAssignStmt = 40, RULE_derefAssignStmt = 41, RULE_arrowAssignStmt = 42, 
		RULE_arrowCompoundAssignStmt = 43, RULE_callStmt = 44, RULE_argList = 45, 
		RULE_returnStmt = 46, RULE_ifStmt = 47, RULE_elseIfClause = 48, RULE_elseClause = 49, 
		RULE_ifBody = 50, RULE_forStmt = 51, RULE_breakStmt = 52, RULE_continueStmt = 53, 
		RULE_loopStmt = 54, RULE_whileStmt = 55, RULE_doWhileStmt = 56, RULE_lockStmt = 57, 
		RULE_tryCatchStmt = 58, RULE_catchClause = 59, RULE_finallyClause = 60, 
		RULE_throwStmt = 61, RULE_switchStmt = 62, RULE_caseClause = 63, RULE_defaultClause = 64, 
		RULE_expression = 65, RULE_typeSpec = 66, RULE_fnTypeSpec = 67, RULE_primitiveType = 68;
	private static String[] makeRuleNames() {
		return new String[] {
			"program", "preambleDecl", "namespaceDecl", "useDecl", "modulePath", 
			"includeDecl", "topLevelDecl", "typeAliasDecl", "enumDecl", "enumVariant", 
			"enumPayloadField", "structDecl", "structField", "unionDecl", "unionField", 
			"externDecl", "externParamList", "externParam", "functionDecl", "extendDecl", 
			"typeParamList", "typeParam", "extendMethod", "paramList", "param", "block", 
			"statement", "deferStmt", "nakedBlockStmt", "inlineBlockStmt", "scopeBlockStmt", 
			"scopeCallbackList", "scopeCallback", "exprStmt", "varDeclStmt", "assignStmt", 
			"compoundAssignStmt", "fieldAssignStmt", "fieldCompoundAssignStmt", "indexFieldAssignStmt", 
			"fieldIndexAssignStmt", "derefAssignStmt", "arrowAssignStmt", "arrowCompoundAssignStmt", 
			"callStmt", "argList", "returnStmt", "ifStmt", "elseIfClause", "elseClause", 
			"ifBody", "forStmt", "breakStmt", "continueStmt", "loopStmt", "whileStmt", 
			"doWhileStmt", "lockStmt", "tryCatchStmt", "catchClause", "finallyClause", 
			"throwStmt", "switchStmt", "caseClause", "defaultClause", "expression", 
			"typeSpec", "fnTypeSpec", "primitiveType"
		};
	}
	public static final String[] ruleNames = makeRuleNames();

	private static String[] makeLiteralNames() {
		return new String[] {
			null, "'namespace'", "'use'", "'ret'", "'struct'", "'union'", "'enum'", 
			"'null'", "'fn'", "'type'", "'as'", "'is'", "'sizeof'", "'typeof'", "'if'", 
			"'else'", "'for'", "'in'", "'loop'", "'while'", "'do'", "'break'", "'continue'", 
			"'switch'", "'case'", "'default'", "'spawn'", "'await'", "'lock'", "'extend'", 
			"'try'", "'catch'", "'finally'", "'throw'", "'defer'", "'extern'", "'auto'", 
			"'vec'", "'map'", "'set'", "'tuple'", "'->'", null, null, "'#inline'", 
			"'#scope'", "'??'", "'...'", "'..='", "'..'", "'int1'", "'int8'", "'int16'", 
			"'int32'", "'int64'", "'int128'", "'intinf'", "'isize'", "'uint1'", "'uint8'", 
			"'uint16'", "'uint32'", "'uint64'", "'uint128'", "'usize'", "'float32'", 
			"'float64'", "'float80'", "'float128'", "'double'", "'bool'", "'char'", 
			"'void'", "'string'", "'cstring'", null, null, null, null, null, null, 
			null, null, null, null, "'+='", "'-='", "'*='", "'/='", "'%='", "'&='", 
			"'|='", "'^='", "'<<='", "'>>='", "';'", "':'", "'::'", "','", "'.'", 
			"'='", "'('", "')'", "'{'", "'}'", "'['", "']'", "'*'", "'&'", "'-'", 
			"'+'", "'/'", "'%'", "'=='", "'!='", "'<='", "'>='", "'<'", "'>'", "'&&'", 
			"'||'", "'!'", "'++'", "'--'", "'<<'", "'|'", "'^'", "'~'", "'?'"
		};
	}
	private static final String[] _LITERAL_NAMES = makeLiteralNames();
	private static String[] makeSymbolicNames() {
		return new String[] {
			null, "NAMESPACE", "USE", "RET", "STRUCT", "UNION", "ENUM", "NULL_LIT", 
			"FN", "TYPE", "AS", "IS", "SIZEOF", "TYPEOF", "IF", "ELSE", "FOR", "IN", 
			"LOOP", "WHILE", "DO", "BREAK", "CONTINUE", "SWITCH", "CASE", "DEFAULT", 
			"SPAWN", "AWAIT", "LOCK", "EXTEND", "TRY", "CATCH", "FINALLY", "THROW", 
			"DEFER", "EXTERN", "AUTO", "VEC", "MAP", "SET", "TUPLE", "ARROW", "INCLUDE_SYS", 
			"INCLUDE_LOCAL", "INLINE_BLOCK", "SCOPE_BLOCK", "NULLCOAL", "SPREAD", 
			"RANGE_INCL", "RANGE", "INT1", "INT8", "INT16", "INT32", "INT64", "INT128", 
			"INTINF", "ISIZE", "UINT1", "UINT8", "UINT16", "UINT32", "UINT64", "UINT128", 
			"USIZE", "FLOAT32", "FLOAT64", "FLOAT80", "FLOAT128", "DOUBLE", "BOOL", 
			"CHAR", "VOID", "STRING", "CSTRING", "HEX_LIT", "OCT_LIT", "BIN_LIT", 
			"INT_LIT", "FLOAT_LIT", "BOOL_LIT", "C_STR_LIT", "STR_LIT", "CHAR_LIT", 
			"IDENTIFIER", "PLUS_ASSIGN", "MINUS_ASSIGN", "STAR_ASSIGN", "SLASH_ASSIGN", 
			"PERCENT_ASSIGN", "AMP_ASSIGN", "PIPE_ASSIGN", "CARET_ASSIGN", "LSHIFT_ASSIGN", 
			"RSHIFT_ASSIGN", "SEMI", "COLON", "SCOPE", "COMMA", "DOT", "ASSIGN", 
			"LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET", "STAR", 
			"AMPERSAND", "MINUS", "PLUS", "SLASH", "PERCENT", "EQ", "NEQ", "LTE", 
			"GTE", "LT", "GT", "LAND", "LOR", "NOT", "INCR", "DECR", "LSHIFT", "PIPE", 
			"CARET", "TILDE", "QUESTION", "WS", "LINE_COMMENT", "BLOCK_COMMENT"
		};
	}
	private static final String[] _SYMBOLIC_NAMES = makeSymbolicNames();
	public static final Vocabulary VOCABULARY = new VocabularyImpl(_LITERAL_NAMES, _SYMBOLIC_NAMES);

	/**
	 * @deprecated Use {@link #VOCABULARY} instead.
	 */
	@Deprecated
	public static final String[] tokenNames;
	static {
		tokenNames = new String[_SYMBOLIC_NAMES.length];
		for (int i = 0; i < tokenNames.length; i++) {
			tokenNames[i] = VOCABULARY.getLiteralName(i);
			if (tokenNames[i] == null) {
				tokenNames[i] = VOCABULARY.getSymbolicName(i);
			}

			if (tokenNames[i] == null) {
				tokenNames[i] = "<INVALID>";
			}
		}
	}

	@Override
	@Deprecated
	public String[] getTokenNames() {
		return tokenNames;
	}

	@Override

	public Vocabulary getVocabulary() {
		return VOCABULARY;
	}

	@Override
	public String getGrammarFileName() { return "LuxParser.g4"; }

	@Override
	public String[] getRuleNames() { return ruleNames; }

	@Override
	public String getSerializedATN() { return _serializedATN; }

	@Override
	public ATN getATN() { return _ATN; }

	public LuxParser(TokenStream input) {
		super(input);
		_interp = new ParserATNSimulator(this,_ATN,_decisionToDFA,_sharedContextCache);
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ProgramContext extends ParserRuleContext {
		public TerminalNode EOF() { return getToken(LuxParser.EOF, 0); }
		public NamespaceDeclContext namespaceDecl() {
			return getRuleContext(NamespaceDeclContext.class,0);
		}
		public List<PreambleDeclContext> preambleDecl() {
			return getRuleContexts(PreambleDeclContext.class);
		}
		public PreambleDeclContext preambleDecl(int i) {
			return getRuleContext(PreambleDeclContext.class,i);
		}
		public List<TopLevelDeclContext> topLevelDecl() {
			return getRuleContexts(TopLevelDeclContext.class);
		}
		public TopLevelDeclContext topLevelDecl(int i) {
			return getRuleContext(TopLevelDeclContext.class,i);
		}
		public ProgramContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_program; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterProgram(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitProgram(this);
		}
	}

	public final ProgramContext program() throws RecognitionException {
		ProgramContext _localctx = new ProgramContext(_ctx, getState());
		enterRule(_localctx, 0, RULE_program);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(139);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==NAMESPACE) {
				{
				setState(138);
				namespaceDecl();
				}
			}

			setState(144);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & 13194139533316L) != 0)) {
				{
				{
				setState(141);
				preambleDecl();
				}
				}
				setState(146);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(150);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123734706453648L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				{
				setState(147);
				topLevelDecl();
				}
				}
				setState(152);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(153);
			match(EOF);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class PreambleDeclContext extends ParserRuleContext {
		public UseDeclContext useDecl() {
			return getRuleContext(UseDeclContext.class,0);
		}
		public IncludeDeclContext includeDecl() {
			return getRuleContext(IncludeDeclContext.class,0);
		}
		public PreambleDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_preambleDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPreambleDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPreambleDecl(this);
		}
	}

	public final PreambleDeclContext preambleDecl() throws RecognitionException {
		PreambleDeclContext _localctx = new PreambleDeclContext(_ctx, getState());
		enterRule(_localctx, 2, RULE_preambleDecl);
		try {
			setState(157);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case USE:
				enterOuterAlt(_localctx, 1);
				{
				setState(155);
				useDecl();
				}
				break;
			case INCLUDE_SYS:
			case INCLUDE_LOCAL:
				enterOuterAlt(_localctx, 2);
				{
				setState(156);
				includeDecl();
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class NamespaceDeclContext extends ParserRuleContext {
		public TerminalNode NAMESPACE() { return getToken(LuxParser.NAMESPACE, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public NamespaceDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_namespaceDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterNamespaceDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitNamespaceDecl(this);
		}
	}

	public final NamespaceDeclContext namespaceDecl() throws RecognitionException {
		NamespaceDeclContext _localctx = new NamespaceDeclContext(_ctx, getState());
		enterRule(_localctx, 4, RULE_namespaceDecl);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(159);
			match(NAMESPACE);
			setState(160);
			match(IDENTIFIER);
			setState(161);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class UseDeclContext extends ParserRuleContext {
		public UseDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_useDecl; }
	 
		public UseDeclContext() { }
		public void copyFrom(UseDeclContext ctx) {
			super.copyFrom(ctx);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class UseItemContext extends UseDeclContext {
		public TerminalNode USE() { return getToken(LuxParser.USE, 0); }
		public ModulePathContext modulePath() {
			return getRuleContext(ModulePathContext.class,0);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public UseItemContext(UseDeclContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterUseItem(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitUseItem(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class UseGroupContext extends UseDeclContext {
		public TerminalNode USE() { return getToken(LuxParser.USE, 0); }
		public ModulePathContext modulePath() {
			return getRuleContext(ModulePathContext.class,0);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public UseGroupContext(UseDeclContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterUseGroup(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitUseGroup(this);
		}
	}

	public final UseDeclContext useDecl() throws RecognitionException {
		UseDeclContext _localctx = new UseDeclContext(_ctx, getState());
		enterRule(_localctx, 6, RULE_useDecl);
		int _la;
		try {
			setState(184);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,5,_ctx) ) {
			case 1:
				_localctx = new UseItemContext(_localctx);
				enterOuterAlt(_localctx, 1);
				{
				setState(163);
				match(USE);
				setState(164);
				modulePath();
				setState(165);
				match(SCOPE);
				setState(166);
				match(IDENTIFIER);
				setState(167);
				match(SEMI);
				}
				break;
			case 2:
				_localctx = new UseGroupContext(_localctx);
				enterOuterAlt(_localctx, 2);
				{
				setState(169);
				match(USE);
				setState(170);
				modulePath();
				setState(171);
				match(SCOPE);
				setState(172);
				match(LBRACE);
				setState(173);
				match(IDENTIFIER);
				setState(178);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(174);
					match(COMMA);
					setState(175);
					match(IDENTIFIER);
					}
					}
					setState(180);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(181);
				match(RBRACE);
				setState(182);
				match(SEMI);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ModulePathContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public List<TerminalNode> SCOPE() { return getTokens(LuxParser.SCOPE); }
		public TerminalNode SCOPE(int i) {
			return getToken(LuxParser.SCOPE, i);
		}
		public ModulePathContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_modulePath; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterModulePath(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitModulePath(this);
		}
	}

	public final ModulePathContext modulePath() throws RecognitionException {
		ModulePathContext _localctx = new ModulePathContext(_ctx, getState());
		enterRule(_localctx, 8, RULE_modulePath);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(186);
			match(IDENTIFIER);
			setState(191);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,6,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(187);
					match(SCOPE);
					setState(188);
					match(IDENTIFIER);
					}
					} 
				}
				setState(193);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,6,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class IncludeDeclContext extends ParserRuleContext {
		public TerminalNode INCLUDE_SYS() { return getToken(LuxParser.INCLUDE_SYS, 0); }
		public TerminalNode INCLUDE_LOCAL() { return getToken(LuxParser.INCLUDE_LOCAL, 0); }
		public IncludeDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_includeDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIncludeDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIncludeDecl(this);
		}
	}

	public final IncludeDeclContext includeDecl() throws RecognitionException {
		IncludeDeclContext _localctx = new IncludeDeclContext(_ctx, getState());
		enterRule(_localctx, 10, RULE_includeDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(194);
			_la = _input.LA(1);
			if ( !(_la==INCLUDE_SYS || _la==INCLUDE_LOCAL) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TopLevelDeclContext extends ParserRuleContext {
		public TypeAliasDeclContext typeAliasDecl() {
			return getRuleContext(TypeAliasDeclContext.class,0);
		}
		public StructDeclContext structDecl() {
			return getRuleContext(StructDeclContext.class,0);
		}
		public UnionDeclContext unionDecl() {
			return getRuleContext(UnionDeclContext.class,0);
		}
		public EnumDeclContext enumDecl() {
			return getRuleContext(EnumDeclContext.class,0);
		}
		public ExtendDeclContext extendDecl() {
			return getRuleContext(ExtendDeclContext.class,0);
		}
		public ExternDeclContext externDecl() {
			return getRuleContext(ExternDeclContext.class,0);
		}
		public FunctionDeclContext functionDecl() {
			return getRuleContext(FunctionDeclContext.class,0);
		}
		public TopLevelDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_topLevelDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTopLevelDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTopLevelDecl(this);
		}
	}

	public final TopLevelDeclContext topLevelDecl() throws RecognitionException {
		TopLevelDeclContext _localctx = new TopLevelDeclContext(_ctx, getState());
		enterRule(_localctx, 12, RULE_topLevelDecl);
		try {
			setState(203);
			_errHandler.sync(this);
			switch (_input.LA(1)) {
			case TYPE:
				enterOuterAlt(_localctx, 1);
				{
				setState(196);
				typeAliasDecl();
				}
				break;
			case STRUCT:
				enterOuterAlt(_localctx, 2);
				{
				setState(197);
				structDecl();
				}
				break;
			case UNION:
				enterOuterAlt(_localctx, 3);
				{
				setState(198);
				unionDecl();
				}
				break;
			case ENUM:
				enterOuterAlt(_localctx, 4);
				{
				setState(199);
				enumDecl();
				}
				break;
			case EXTEND:
				enterOuterAlt(_localctx, 5);
				{
				setState(200);
				extendDecl();
				}
				break;
			case EXTERN:
				enterOuterAlt(_localctx, 6);
				{
				setState(201);
				externDecl();
				}
				break;
			case FN:
			case AUTO:
			case VEC:
			case MAP:
			case SET:
			case TUPLE:
			case INT1:
			case INT8:
			case INT16:
			case INT32:
			case INT64:
			case INT128:
			case INTINF:
			case ISIZE:
			case UINT1:
			case UINT8:
			case UINT16:
			case UINT32:
			case UINT64:
			case UINT128:
			case USIZE:
			case FLOAT32:
			case FLOAT64:
			case FLOAT80:
			case FLOAT128:
			case DOUBLE:
			case BOOL:
			case CHAR:
			case VOID:
			case STRING:
			case CSTRING:
			case IDENTIFIER:
			case LBRACKET:
			case STAR:
				enterOuterAlt(_localctx, 7);
				{
				setState(202);
				functionDecl();
				}
				break;
			default:
				throw new NoViableAltException(this);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TypeAliasDeclContext extends ParserRuleContext {
		public TerminalNode TYPE() { return getToken(LuxParser.TYPE, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TypeAliasDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_typeAliasDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTypeAliasDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTypeAliasDecl(this);
		}
	}

	public final TypeAliasDeclContext typeAliasDecl() throws RecognitionException {
		TypeAliasDeclContext _localctx = new TypeAliasDeclContext(_ctx, getState());
		enterRule(_localctx, 14, RULE_typeAliasDecl);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(205);
			match(TYPE);
			setState(206);
			match(IDENTIFIER);
			setState(207);
			match(ASSIGN);
			setState(208);
			typeSpec(0);
			setState(209);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class EnumDeclContext extends ParserRuleContext {
		public TerminalNode ENUM() { return getToken(LuxParser.ENUM, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public List<EnumVariantContext> enumVariant() {
			return getRuleContexts(EnumVariantContext.class);
		}
		public EnumVariantContext enumVariant(int i) {
			return getRuleContext(EnumVariantContext.class,i);
		}
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public TypeParamListContext typeParamList() {
			return getRuleContext(TypeParamListContext.class,0);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public EnumDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_enumDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEnumDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEnumDecl(this);
		}
	}

	public final EnumDeclContext enumDecl() throws RecognitionException {
		EnumDeclContext _localctx = new EnumDeclContext(_ctx, getState());
		enterRule(_localctx, 16, RULE_enumDecl);
		int _la;
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(211);
			match(ENUM);
			setState(212);
			match(IDENTIFIER);
			setState(214);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(213);
				typeParamList();
				}
			}

			setState(216);
			match(LBRACE);
			setState(217);
			enumVariant();
			setState(222);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,9,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(218);
					match(COMMA);
					setState(219);
					enumVariant();
					}
					} 
				}
				setState(224);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,9,_ctx);
			}
			setState(226);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COMMA) {
				{
				setState(225);
				match(COMMA);
				}
			}

			setState(228);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class EnumVariantContext extends ParserRuleContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public List<EnumPayloadFieldContext> enumPayloadField() {
			return getRuleContexts(EnumPayloadFieldContext.class);
		}
		public EnumPayloadFieldContext enumPayloadField(int i) {
			return getRuleContext(EnumPayloadFieldContext.class,i);
		}
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public EnumVariantContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_enumVariant; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEnumVariant(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEnumVariant(this);
		}
	}

	public final EnumVariantContext enumVariant() throws RecognitionException {
		EnumVariantContext _localctx = new EnumVariantContext(_ctx, getState());
		enterRule(_localctx, 18, RULE_enumVariant);
		int _la;
		try {
			int _alt;
			setState(258);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,14,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(230);
				match(IDENTIFIER);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(231);
				match(IDENTIFIER);
				setState(232);
				match(LPAREN);
				setState(233);
				typeSpec(0);
				setState(238);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(234);
					match(COMMA);
					setState(235);
					typeSpec(0);
					}
					}
					setState(240);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(241);
				match(RPAREN);
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(243);
				match(IDENTIFIER);
				setState(244);
				match(LBRACE);
				setState(245);
				enumPayloadField();
				setState(250);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,12,_ctx);
				while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
					if ( _alt==1 ) {
						{
						{
						setState(246);
						match(COMMA);
						setState(247);
						enumPayloadField();
						}
						} 
					}
					setState(252);
					_errHandler.sync(this);
					_alt = getInterpreter().adaptivePredict(_input,12,_ctx);
				}
				setState(254);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==COMMA) {
					{
					setState(253);
					match(COMMA);
					}
				}

				setState(256);
				match(RBRACE);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class EnumPayloadFieldContext extends ParserRuleContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode COLON() { return getToken(LuxParser.COLON, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public EnumPayloadFieldContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_enumPayloadField; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEnumPayloadField(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEnumPayloadField(this);
		}
	}

	public final EnumPayloadFieldContext enumPayloadField() throws RecognitionException {
		EnumPayloadFieldContext _localctx = new EnumPayloadFieldContext(_ctx, getState());
		enterRule(_localctx, 20, RULE_enumPayloadField);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(260);
			match(IDENTIFIER);
			setState(261);
			match(COLON);
			setState(262);
			typeSpec(0);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class StructDeclContext extends ParserRuleContext {
		public TerminalNode STRUCT() { return getToken(LuxParser.STRUCT, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public TypeParamListContext typeParamList() {
			return getRuleContext(TypeParamListContext.class,0);
		}
		public List<StructFieldContext> structField() {
			return getRuleContexts(StructFieldContext.class);
		}
		public StructFieldContext structField(int i) {
			return getRuleContext(StructFieldContext.class,i);
		}
		public StructDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_structDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStructDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStructDecl(this);
		}
	}

	public final StructDeclContext structDecl() throws RecognitionException {
		StructDeclContext _localctx = new StructDeclContext(_ctx, getState());
		enterRule(_localctx, 22, RULE_structDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(264);
			match(STRUCT);
			setState(265);
			match(IDENTIFIER);
			setState(267);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(266);
				typeParamList();
				}
			}

			setState(269);
			match(LBRACE);
			setState(273);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				{
				setState(270);
				structField();
				}
				}
				setState(275);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(276);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class StructFieldContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public StructFieldContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_structField; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStructField(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStructField(this);
		}
	}

	public final StructFieldContext structField() throws RecognitionException {
		StructFieldContext _localctx = new StructFieldContext(_ctx, getState());
		enterRule(_localctx, 24, RULE_structField);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(278);
			typeSpec(0);
			setState(279);
			match(IDENTIFIER);
			setState(280);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class UnionDeclContext extends ParserRuleContext {
		public TerminalNode UNION() { return getToken(LuxParser.UNION, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public TypeParamListContext typeParamList() {
			return getRuleContext(TypeParamListContext.class,0);
		}
		public List<UnionFieldContext> unionField() {
			return getRuleContexts(UnionFieldContext.class);
		}
		public UnionFieldContext unionField(int i) {
			return getRuleContext(UnionFieldContext.class,i);
		}
		public UnionDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_unionDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterUnionDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitUnionDecl(this);
		}
	}

	public final UnionDeclContext unionDecl() throws RecognitionException {
		UnionDeclContext _localctx = new UnionDeclContext(_ctx, getState());
		enterRule(_localctx, 26, RULE_unionDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(282);
			match(UNION);
			setState(283);
			match(IDENTIFIER);
			setState(285);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(284);
				typeParamList();
				}
			}

			setState(287);
			match(LBRACE);
			setState(291);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				{
				setState(288);
				unionField();
				}
				}
				setState(293);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(294);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class UnionFieldContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public UnionFieldContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_unionField; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterUnionField(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitUnionField(this);
		}
	}

	public final UnionFieldContext unionField() throws RecognitionException {
		UnionFieldContext _localctx = new UnionFieldContext(_ctx, getState());
		enterRule(_localctx, 28, RULE_unionField);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(296);
			typeSpec(0);
			setState(297);
			match(IDENTIFIER);
			setState(298);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExternDeclContext extends ParserRuleContext {
		public TerminalNode EXTERN() { return getToken(LuxParser.EXTERN, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ExternParamListContext externParamList() {
			return getRuleContext(ExternParamListContext.class,0);
		}
		public TerminalNode COMMA() { return getToken(LuxParser.COMMA, 0); }
		public TerminalNode SPREAD() { return getToken(LuxParser.SPREAD, 0); }
		public ExternDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_externDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExternDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExternDecl(this);
		}
	}

	public final ExternDeclContext externDecl() throws RecognitionException {
		ExternDeclContext _localctx = new ExternDeclContext(_ctx, getState());
		enterRule(_localctx, 30, RULE_externDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(300);
			match(EXTERN);
			setState(301);
			typeSpec(0);
			setState(302);
			match(IDENTIFIER);
			setState(303);
			match(LPAREN);
			setState(305);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				setState(304);
				externParamList();
				}
			}

			setState(309);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COMMA) {
				{
				setState(307);
				match(COMMA);
				setState(308);
				match(SPREAD);
				}
			}

			setState(311);
			match(RPAREN);
			setState(312);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExternParamListContext extends ParserRuleContext {
		public List<ExternParamContext> externParam() {
			return getRuleContexts(ExternParamContext.class);
		}
		public ExternParamContext externParam(int i) {
			return getRuleContext(ExternParamContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ExternParamListContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_externParamList; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExternParamList(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExternParamList(this);
		}
	}

	public final ExternParamListContext externParamList() throws RecognitionException {
		ExternParamListContext _localctx = new ExternParamListContext(_ctx, getState());
		enterRule(_localctx, 32, RULE_externParamList);
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(314);
			externParam();
			setState(319);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,21,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					{
					{
					setState(315);
					match(COMMA);
					setState(316);
					externParam();
					}
					} 
				}
				setState(321);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,21,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExternParamContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public ExternParamContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_externParam; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExternParam(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExternParam(this);
		}
	}

	public final ExternParamContext externParam() throws RecognitionException {
		ExternParamContext _localctx = new ExternParamContext(_ctx, getState());
		enterRule(_localctx, 34, RULE_externParam);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(322);
			typeSpec(0);
			setState(324);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==IDENTIFIER) {
				{
				setState(323);
				match(IDENTIFIER);
				}
			}

			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FunctionDeclContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TypeParamListContext typeParamList() {
			return getRuleContext(TypeParamListContext.class,0);
		}
		public ParamListContext paramList() {
			return getRuleContext(ParamListContext.class,0);
		}
		public FunctionDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_functionDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFunctionDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFunctionDecl(this);
		}
	}

	public final FunctionDeclContext functionDecl() throws RecognitionException {
		FunctionDeclContext _localctx = new FunctionDeclContext(_ctx, getState());
		enterRule(_localctx, 36, RULE_functionDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(326);
			typeSpec(0);
			setState(327);
			match(IDENTIFIER);
			setState(329);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(328);
				typeParamList();
				}
			}

			setState(331);
			match(LPAREN);
			setState(333);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				setState(332);
				paramList();
				}
			}

			setState(335);
			match(RPAREN);
			setState(336);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExtendDeclContext extends ParserRuleContext {
		public TerminalNode EXTEND() { return getToken(LuxParser.EXTEND, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public TypeParamListContext typeParamList() {
			return getRuleContext(TypeParamListContext.class,0);
		}
		public List<ExtendMethodContext> extendMethod() {
			return getRuleContexts(ExtendMethodContext.class);
		}
		public ExtendMethodContext extendMethod(int i) {
			return getRuleContext(ExtendMethodContext.class,i);
		}
		public ExtendDeclContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_extendDecl; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExtendDecl(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExtendDecl(this);
		}
	}

	public final ExtendDeclContext extendDecl() throws RecognitionException {
		ExtendDeclContext _localctx = new ExtendDeclContext(_ctx, getState());
		enterRule(_localctx, 38, RULE_extendDecl);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(338);
			match(EXTEND);
			setState(339);
			match(IDENTIFIER);
			setState(341);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==LT) {
				{
				setState(340);
				typeParamList();
				}
			}

			setState(343);
			match(LBRACE);
			setState(347);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				{
				setState(344);
				extendMethod();
				}
				}
				setState(349);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(350);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TypeParamListContext extends ParserRuleContext {
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeParamContext> typeParam() {
			return getRuleContexts(TypeParamContext.class);
		}
		public TypeParamContext typeParam(int i) {
			return getRuleContext(TypeParamContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public TypeParamListContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_typeParamList; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTypeParamList(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTypeParamList(this);
		}
	}

	public final TypeParamListContext typeParamList() throws RecognitionException {
		TypeParamListContext _localctx = new TypeParamListContext(_ctx, getState());
		enterRule(_localctx, 40, RULE_typeParamList);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(352);
			match(LT);
			setState(353);
			typeParam();
			setState(358);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(354);
				match(COMMA);
				setState(355);
				typeParam();
				}
				}
				setState(360);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(361);
			match(GT);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TypeParamContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode COLON() { return getToken(LuxParser.COLON, 0); }
		public TypeParamContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_typeParam; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTypeParam(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTypeParam(this);
		}
	}

	public final TypeParamContext typeParam() throws RecognitionException {
		TypeParamContext _localctx = new TypeParamContext(_ctx, getState());
		enterRule(_localctx, 42, RULE_typeParam);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(363);
			match(IDENTIFIER);
			setState(366);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==COLON) {
				{
				setState(364);
				match(COLON);
				setState(365);
				match(IDENTIFIER);
				}
			}

			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExtendMethodContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode AMPERSAND() { return getToken(LuxParser.AMPERSAND, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public List<ParamContext> param() {
			return getRuleContexts(ParamContext.class);
		}
		public ParamContext param(int i) {
			return getRuleContext(ParamContext.class,i);
		}
		public ParamListContext paramList() {
			return getRuleContext(ParamListContext.class,0);
		}
		public ExtendMethodContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_extendMethod; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExtendMethod(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExtendMethod(this);
		}
	}

	public final ExtendMethodContext extendMethod() throws RecognitionException {
		ExtendMethodContext _localctx = new ExtendMethodContext(_ctx, getState());
		enterRule(_localctx, 44, RULE_extendMethod);
		int _la;
		try {
			setState(392);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,31,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(368);
				typeSpec(0);
				setState(369);
				match(IDENTIFIER);
				setState(370);
				match(LPAREN);
				setState(371);
				match(AMPERSAND);
				setState(372);
				match(IDENTIFIER);
				setState(377);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(373);
					match(COMMA);
					setState(374);
					param();
					}
					}
					setState(379);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(380);
				match(RPAREN);
				setState(381);
				block();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(383);
				typeSpec(0);
				setState(384);
				match(IDENTIFIER);
				setState(385);
				match(LPAREN);
				setState(387);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
					{
					setState(386);
					paramList();
					}
				}

				setState(389);
				match(RPAREN);
				setState(390);
				block();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ParamListContext extends ParserRuleContext {
		public List<ParamContext> param() {
			return getRuleContexts(ParamContext.class);
		}
		public ParamContext param(int i) {
			return getRuleContext(ParamContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ParamListContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_paramList; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterParamList(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitParamList(this);
		}
	}

	public final ParamListContext paramList() throws RecognitionException {
		ParamListContext _localctx = new ParamListContext(_ctx, getState());
		enterRule(_localctx, 46, RULE_paramList);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(394);
			param();
			setState(399);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(395);
				match(COMMA);
				setState(396);
				param();
				}
				}
				setState(401);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ParamContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode SPREAD() { return getToken(LuxParser.SPREAD, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public ParamContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_param; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterParam(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitParam(this);
		}
	}

	public final ParamContext param() throws RecognitionException {
		ParamContext _localctx = new ParamContext(_ctx, getState());
		enterRule(_localctx, 48, RULE_param);
		try {
			setState(409);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,33,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(402);
				typeSpec(0);
				setState(403);
				match(SPREAD);
				setState(404);
				match(IDENTIFIER);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(406);
				typeSpec(0);
				setState(407);
				match(IDENTIFIER);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class BlockContext extends ParserRuleContext {
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<StatementContext> statement() {
			return getRuleContexts(StatementContext.class);
		}
		public StatementContext statement(int i) {
			return getRuleContext(StatementContext.class,i);
		}
		public BlockContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_block; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBlock(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBlock(this);
		}
	}

	public final BlockContext block() throws RecognitionException {
		BlockContext _localctx = new BlockContext(_ctx, getState());
		enterRule(_localctx, 50, RULE_block);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(411);
			match(LBRACE);
			setState(415);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -930228226657912L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & -8214501261452509185L) != 0)) {
				{
				{
				setState(412);
				statement();
				}
				}
				setState(417);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(418);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class StatementContext extends ParserRuleContext {
		public VarDeclStmtContext varDeclStmt() {
			return getRuleContext(VarDeclStmtContext.class,0);
		}
		public AssignStmtContext assignStmt() {
			return getRuleContext(AssignStmtContext.class,0);
		}
		public CompoundAssignStmtContext compoundAssignStmt() {
			return getRuleContext(CompoundAssignStmtContext.class,0);
		}
		public DerefAssignStmtContext derefAssignStmt() {
			return getRuleContext(DerefAssignStmtContext.class,0);
		}
		public FieldIndexAssignStmtContext fieldIndexAssignStmt() {
			return getRuleContext(FieldIndexAssignStmtContext.class,0);
		}
		public IndexFieldAssignStmtContext indexFieldAssignStmt() {
			return getRuleContext(IndexFieldAssignStmtContext.class,0);
		}
		public FieldAssignStmtContext fieldAssignStmt() {
			return getRuleContext(FieldAssignStmtContext.class,0);
		}
		public FieldCompoundAssignStmtContext fieldCompoundAssignStmt() {
			return getRuleContext(FieldCompoundAssignStmtContext.class,0);
		}
		public ArrowAssignStmtContext arrowAssignStmt() {
			return getRuleContext(ArrowAssignStmtContext.class,0);
		}
		public ArrowCompoundAssignStmtContext arrowCompoundAssignStmt() {
			return getRuleContext(ArrowCompoundAssignStmtContext.class,0);
		}
		public CallStmtContext callStmt() {
			return getRuleContext(CallStmtContext.class,0);
		}
		public ExprStmtContext exprStmt() {
			return getRuleContext(ExprStmtContext.class,0);
		}
		public ReturnStmtContext returnStmt() {
			return getRuleContext(ReturnStmtContext.class,0);
		}
		public IfStmtContext ifStmt() {
			return getRuleContext(IfStmtContext.class,0);
		}
		public ForStmtContext forStmt() {
			return getRuleContext(ForStmtContext.class,0);
		}
		public LoopStmtContext loopStmt() {
			return getRuleContext(LoopStmtContext.class,0);
		}
		public WhileStmtContext whileStmt() {
			return getRuleContext(WhileStmtContext.class,0);
		}
		public DoWhileStmtContext doWhileStmt() {
			return getRuleContext(DoWhileStmtContext.class,0);
		}
		public BreakStmtContext breakStmt() {
			return getRuleContext(BreakStmtContext.class,0);
		}
		public ContinueStmtContext continueStmt() {
			return getRuleContext(ContinueStmtContext.class,0);
		}
		public SwitchStmtContext switchStmt() {
			return getRuleContext(SwitchStmtContext.class,0);
		}
		public LockStmtContext lockStmt() {
			return getRuleContext(LockStmtContext.class,0);
		}
		public TryCatchStmtContext tryCatchStmt() {
			return getRuleContext(TryCatchStmtContext.class,0);
		}
		public ThrowStmtContext throwStmt() {
			return getRuleContext(ThrowStmtContext.class,0);
		}
		public DeferStmtContext deferStmt() {
			return getRuleContext(DeferStmtContext.class,0);
		}
		public NakedBlockStmtContext nakedBlockStmt() {
			return getRuleContext(NakedBlockStmtContext.class,0);
		}
		public InlineBlockStmtContext inlineBlockStmt() {
			return getRuleContext(InlineBlockStmtContext.class,0);
		}
		public ScopeBlockStmtContext scopeBlockStmt() {
			return getRuleContext(ScopeBlockStmtContext.class,0);
		}
		public StatementContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_statement; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStatement(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStatement(this);
		}
	}

	public final StatementContext statement() throws RecognitionException {
		StatementContext _localctx = new StatementContext(_ctx, getState());
		enterRule(_localctx, 52, RULE_statement);
		try {
			setState(448);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,35,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(420);
				varDeclStmt();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(421);
				assignStmt();
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(422);
				compoundAssignStmt();
				}
				break;
			case 4:
				enterOuterAlt(_localctx, 4);
				{
				setState(423);
				derefAssignStmt();
				}
				break;
			case 5:
				enterOuterAlt(_localctx, 5);
				{
				setState(424);
				fieldIndexAssignStmt();
				}
				break;
			case 6:
				enterOuterAlt(_localctx, 6);
				{
				setState(425);
				indexFieldAssignStmt();
				}
				break;
			case 7:
				enterOuterAlt(_localctx, 7);
				{
				setState(426);
				fieldAssignStmt();
				}
				break;
			case 8:
				enterOuterAlt(_localctx, 8);
				{
				setState(427);
				fieldCompoundAssignStmt();
				}
				break;
			case 9:
				enterOuterAlt(_localctx, 9);
				{
				setState(428);
				arrowAssignStmt();
				}
				break;
			case 10:
				enterOuterAlt(_localctx, 10);
				{
				setState(429);
				arrowCompoundAssignStmt();
				}
				break;
			case 11:
				enterOuterAlt(_localctx, 11);
				{
				setState(430);
				callStmt();
				}
				break;
			case 12:
				enterOuterAlt(_localctx, 12);
				{
				setState(431);
				exprStmt();
				}
				break;
			case 13:
				enterOuterAlt(_localctx, 13);
				{
				setState(432);
				returnStmt();
				}
				break;
			case 14:
				enterOuterAlt(_localctx, 14);
				{
				setState(433);
				ifStmt();
				}
				break;
			case 15:
				enterOuterAlt(_localctx, 15);
				{
				setState(434);
				forStmt();
				}
				break;
			case 16:
				enterOuterAlt(_localctx, 16);
				{
				setState(435);
				loopStmt();
				}
				break;
			case 17:
				enterOuterAlt(_localctx, 17);
				{
				setState(436);
				whileStmt();
				}
				break;
			case 18:
				enterOuterAlt(_localctx, 18);
				{
				setState(437);
				doWhileStmt();
				}
				break;
			case 19:
				enterOuterAlt(_localctx, 19);
				{
				setState(438);
				breakStmt();
				}
				break;
			case 20:
				enterOuterAlt(_localctx, 20);
				{
				setState(439);
				continueStmt();
				}
				break;
			case 21:
				enterOuterAlt(_localctx, 21);
				{
				setState(440);
				switchStmt();
				}
				break;
			case 22:
				enterOuterAlt(_localctx, 22);
				{
				setState(441);
				lockStmt();
				}
				break;
			case 23:
				enterOuterAlt(_localctx, 23);
				{
				setState(442);
				tryCatchStmt();
				}
				break;
			case 24:
				enterOuterAlt(_localctx, 24);
				{
				setState(443);
				throwStmt();
				}
				break;
			case 25:
				enterOuterAlt(_localctx, 25);
				{
				setState(444);
				deferStmt();
				}
				break;
			case 26:
				enterOuterAlt(_localctx, 26);
				{
				setState(445);
				nakedBlockStmt();
				}
				break;
			case 27:
				enterOuterAlt(_localctx, 27);
				{
				setState(446);
				inlineBlockStmt();
				}
				break;
			case 28:
				enterOuterAlt(_localctx, 28);
				{
				setState(447);
				scopeBlockStmt();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class DeferStmtContext extends ParserRuleContext {
		public TerminalNode DEFER() { return getToken(LuxParser.DEFER, 0); }
		public CallStmtContext callStmt() {
			return getRuleContext(CallStmtContext.class,0);
		}
		public ExprStmtContext exprStmt() {
			return getRuleContext(ExprStmtContext.class,0);
		}
		public DeferStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_deferStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterDeferStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitDeferStmt(this);
		}
	}

	public final DeferStmtContext deferStmt() throws RecognitionException {
		DeferStmtContext _localctx = new DeferStmtContext(_ctx, getState());
		enterRule(_localctx, 54, RULE_deferStmt);
		try {
			setState(454);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,36,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(450);
				match(DEFER);
				setState(451);
				callStmt();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(452);
				match(DEFER);
				setState(453);
				exprStmt();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class NakedBlockStmtContext extends ParserRuleContext {
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<StatementContext> statement() {
			return getRuleContexts(StatementContext.class);
		}
		public StatementContext statement(int i) {
			return getRuleContext(StatementContext.class,i);
		}
		public NakedBlockStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_nakedBlockStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterNakedBlockStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitNakedBlockStmt(this);
		}
	}

	public final NakedBlockStmtContext nakedBlockStmt() throws RecognitionException {
		NakedBlockStmtContext _localctx = new NakedBlockStmtContext(_ctx, getState());
		enterRule(_localctx, 56, RULE_nakedBlockStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(456);
			match(LBRACE);
			setState(460);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -930228226657912L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & -8214501261452509185L) != 0)) {
				{
				{
				setState(457);
				statement();
				}
				}
				setState(462);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(463);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class InlineBlockStmtContext extends ParserRuleContext {
		public TerminalNode INLINE_BLOCK() { return getToken(LuxParser.INLINE_BLOCK, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<StatementContext> statement() {
			return getRuleContexts(StatementContext.class);
		}
		public StatementContext statement(int i) {
			return getRuleContext(StatementContext.class,i);
		}
		public InlineBlockStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_inlineBlockStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterInlineBlockStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitInlineBlockStmt(this);
		}
	}

	public final InlineBlockStmtContext inlineBlockStmt() throws RecognitionException {
		InlineBlockStmtContext _localctx = new InlineBlockStmtContext(_ctx, getState());
		enterRule(_localctx, 58, RULE_inlineBlockStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(465);
			match(INLINE_BLOCK);
			setState(466);
			match(LBRACE);
			setState(470);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -930228226657912L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & -8214501261452509185L) != 0)) {
				{
				{
				setState(467);
				statement();
				}
				}
				setState(472);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(473);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ScopeBlockStmtContext extends ParserRuleContext {
		public TerminalNode SCOPE_BLOCK() { return getToken(LuxParser.SCOPE_BLOCK, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public ScopeCallbackListContext scopeCallbackList() {
			return getRuleContext(ScopeCallbackListContext.class,0);
		}
		public List<StatementContext> statement() {
			return getRuleContexts(StatementContext.class);
		}
		public StatementContext statement(int i) {
			return getRuleContext(StatementContext.class,i);
		}
		public ScopeBlockStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_scopeBlockStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterScopeBlockStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitScopeBlockStmt(this);
		}
	}

	public final ScopeBlockStmtContext scopeBlockStmt() throws RecognitionException {
		ScopeBlockStmtContext _localctx = new ScopeBlockStmtContext(_ctx, getState());
		enterRule(_localctx, 60, RULE_scopeBlockStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(475);
			match(SCOPE_BLOCK);
			setState(476);
			match(LPAREN);
			setState(478);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==IDENTIFIER) {
				{
				setState(477);
				scopeCallbackList();
				}
			}

			setState(480);
			match(RPAREN);
			setState(481);
			match(LBRACE);
			setState(485);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while ((((_la) & ~0x3f) == 0 && ((1L << _la) & -930228226657912L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & -8214501261452509185L) != 0)) {
				{
				{
				setState(482);
				statement();
				}
				}
				setState(487);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(488);
			match(RBRACE);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ScopeCallbackListContext extends ParserRuleContext {
		public List<ScopeCallbackContext> scopeCallback() {
			return getRuleContexts(ScopeCallbackContext.class);
		}
		public ScopeCallbackContext scopeCallback(int i) {
			return getRuleContext(ScopeCallbackContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ScopeCallbackListContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_scopeCallbackList; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterScopeCallbackList(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitScopeCallbackList(this);
		}
	}

	public final ScopeCallbackListContext scopeCallbackList() throws RecognitionException {
		ScopeCallbackListContext _localctx = new ScopeCallbackListContext(_ctx, getState());
		enterRule(_localctx, 62, RULE_scopeCallbackList);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(490);
			scopeCallback();
			setState(495);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(491);
				match(COMMA);
				setState(492);
				scopeCallback();
				}
				}
				setState(497);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ScopeCallbackContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode DOT() { return getToken(LuxParser.DOT, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public ScopeCallbackContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_scopeCallback; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterScopeCallback(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitScopeCallback(this);
		}
	}

	public final ScopeCallbackContext scopeCallback() throws RecognitionException {
		ScopeCallbackContext _localctx = new ScopeCallbackContext(_ctx, getState());
		enterRule(_localctx, 64, RULE_scopeCallback);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(498);
			match(IDENTIFIER);
			setState(501);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if (_la==DOT) {
				{
				setState(499);
				match(DOT);
				setState(500);
				match(IDENTIFIER);
				}
			}

			setState(503);
			match(LPAREN);
			setState(505);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
				{
				setState(504);
				argList();
				}
			}

			setState(507);
			match(RPAREN);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExprStmtContext extends ParserRuleContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ExprStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_exprStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterExprStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitExprStmt(this);
		}
	}

	public final ExprStmtContext exprStmt() throws RecognitionException {
		ExprStmtContext _localctx = new ExprStmtContext(_ctx, getState());
		enterRule(_localctx, 66, RULE_exprStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(509);
			expression(0);
			setState(510);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class VarDeclStmtContext extends ParserRuleContext {
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public VarDeclStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_varDeclStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterVarDeclStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitVarDeclStmt(this);
		}
	}

	public final VarDeclStmtContext varDeclStmt() throws RecognitionException {
		VarDeclStmtContext _localctx = new VarDeclStmtContext(_ctx, getState());
		enterRule(_localctx, 68, RULE_varDeclStmt);
		int _la;
		try {
			setState(537);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,45,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(512);
				typeSpec(0);
				setState(513);
				match(LPAREN);
				setState(514);
				match(IDENTIFIER);
				setState(519);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(515);
					match(COMMA);
					setState(516);
					match(IDENTIFIER);
					}
					}
					setState(521);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(522);
				match(RPAREN);
				setState(523);
				match(ASSIGN);
				setState(524);
				expression(0);
				setState(525);
				match(SEMI);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(527);
				typeSpec(0);
				setState(528);
				match(IDENTIFIER);
				setState(529);
				match(ASSIGN);
				setState(530);
				expression(0);
				setState(531);
				match(SEMI);
				}
				break;
			case 3:
				enterOuterAlt(_localctx, 3);
				{
				setState(533);
				typeSpec(0);
				setState(534);
				match(IDENTIFIER);
				setState(535);
				match(SEMI);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class AssignStmtContext extends ParserRuleContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> LBRACKET() { return getTokens(LuxParser.LBRACKET); }
		public TerminalNode LBRACKET(int i) {
			return getToken(LuxParser.LBRACKET, i);
		}
		public List<TerminalNode> RBRACKET() { return getTokens(LuxParser.RBRACKET); }
		public TerminalNode RBRACKET(int i) {
			return getToken(LuxParser.RBRACKET, i);
		}
		public AssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_assignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitAssignStmt(this);
		}
	}

	public final AssignStmtContext assignStmt() throws RecognitionException {
		AssignStmtContext _localctx = new AssignStmtContext(_ctx, getState());
		enterRule(_localctx, 70, RULE_assignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(539);
			match(IDENTIFIER);
			setState(546);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==LBRACKET) {
				{
				{
				setState(540);
				match(LBRACKET);
				setState(541);
				expression(0);
				setState(542);
				match(RBRACKET);
				}
				}
				setState(548);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(549);
			match(ASSIGN);
			setState(550);
			expression(0);
			setState(551);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class CompoundAssignStmtContext extends ParserRuleContext {
		public Token op;
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TerminalNode PLUS_ASSIGN() { return getToken(LuxParser.PLUS_ASSIGN, 0); }
		public TerminalNode MINUS_ASSIGN() { return getToken(LuxParser.MINUS_ASSIGN, 0); }
		public TerminalNode STAR_ASSIGN() { return getToken(LuxParser.STAR_ASSIGN, 0); }
		public TerminalNode SLASH_ASSIGN() { return getToken(LuxParser.SLASH_ASSIGN, 0); }
		public TerminalNode PERCENT_ASSIGN() { return getToken(LuxParser.PERCENT_ASSIGN, 0); }
		public TerminalNode AMP_ASSIGN() { return getToken(LuxParser.AMP_ASSIGN, 0); }
		public TerminalNode PIPE_ASSIGN() { return getToken(LuxParser.PIPE_ASSIGN, 0); }
		public TerminalNode CARET_ASSIGN() { return getToken(LuxParser.CARET_ASSIGN, 0); }
		public TerminalNode LSHIFT_ASSIGN() { return getToken(LuxParser.LSHIFT_ASSIGN, 0); }
		public TerminalNode RSHIFT_ASSIGN() { return getToken(LuxParser.RSHIFT_ASSIGN, 0); }
		public CompoundAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_compoundAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCompoundAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCompoundAssignStmt(this);
		}
	}

	public final CompoundAssignStmtContext compoundAssignStmt() throws RecognitionException {
		CompoundAssignStmtContext _localctx = new CompoundAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 72, RULE_compoundAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(553);
			match(IDENTIFIER);
			setState(554);
			((CompoundAssignStmtContext)_localctx).op = _input.LT(1);
			_la = _input.LA(1);
			if ( !(((((_la - 85)) & ~0x3f) == 0 && ((1L << (_la - 85)) & 1023L) != 0)) ) {
				((CompoundAssignStmtContext)_localctx).op = (Token)_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
			}
			setState(555);
			expression(0);
			setState(556);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FieldAssignStmtContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> DOT() { return getTokens(LuxParser.DOT); }
		public TerminalNode DOT(int i) {
			return getToken(LuxParser.DOT, i);
		}
		public FieldAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fieldAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFieldAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFieldAssignStmt(this);
		}
	}

	public final FieldAssignStmtContext fieldAssignStmt() throws RecognitionException {
		FieldAssignStmtContext _localctx = new FieldAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 74, RULE_fieldAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(558);
			match(IDENTIFIER);
			setState(561); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(559);
				match(DOT);
				setState(560);
				match(IDENTIFIER);
				}
				}
				setState(563); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==DOT );
			setState(565);
			match(ASSIGN);
			setState(566);
			expression(0);
			setState(567);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FieldCompoundAssignStmtContext extends ParserRuleContext {
		public Token op;
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TerminalNode PLUS_ASSIGN() { return getToken(LuxParser.PLUS_ASSIGN, 0); }
		public TerminalNode MINUS_ASSIGN() { return getToken(LuxParser.MINUS_ASSIGN, 0); }
		public TerminalNode STAR_ASSIGN() { return getToken(LuxParser.STAR_ASSIGN, 0); }
		public TerminalNode SLASH_ASSIGN() { return getToken(LuxParser.SLASH_ASSIGN, 0); }
		public TerminalNode PERCENT_ASSIGN() { return getToken(LuxParser.PERCENT_ASSIGN, 0); }
		public TerminalNode AMP_ASSIGN() { return getToken(LuxParser.AMP_ASSIGN, 0); }
		public TerminalNode PIPE_ASSIGN() { return getToken(LuxParser.PIPE_ASSIGN, 0); }
		public TerminalNode CARET_ASSIGN() { return getToken(LuxParser.CARET_ASSIGN, 0); }
		public TerminalNode LSHIFT_ASSIGN() { return getToken(LuxParser.LSHIFT_ASSIGN, 0); }
		public TerminalNode RSHIFT_ASSIGN() { return getToken(LuxParser.RSHIFT_ASSIGN, 0); }
		public List<TerminalNode> DOT() { return getTokens(LuxParser.DOT); }
		public TerminalNode DOT(int i) {
			return getToken(LuxParser.DOT, i);
		}
		public FieldCompoundAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fieldCompoundAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFieldCompoundAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFieldCompoundAssignStmt(this);
		}
	}

	public final FieldCompoundAssignStmtContext fieldCompoundAssignStmt() throws RecognitionException {
		FieldCompoundAssignStmtContext _localctx = new FieldCompoundAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 76, RULE_fieldCompoundAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(569);
			match(IDENTIFIER);
			setState(572); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(570);
				match(DOT);
				setState(571);
				match(IDENTIFIER);
				}
				}
				setState(574); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==DOT );
			setState(576);
			((FieldCompoundAssignStmtContext)_localctx).op = _input.LT(1);
			_la = _input.LA(1);
			if ( !(((((_la - 85)) & ~0x3f) == 0 && ((1L << (_la - 85)) & 1023L) != 0)) ) {
				((FieldCompoundAssignStmtContext)_localctx).op = (Token)_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
			}
			setState(577);
			expression(0);
			setState(578);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class IndexFieldAssignStmtContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> LBRACKET() { return getTokens(LuxParser.LBRACKET); }
		public TerminalNode LBRACKET(int i) {
			return getToken(LuxParser.LBRACKET, i);
		}
		public List<TerminalNode> RBRACKET() { return getTokens(LuxParser.RBRACKET); }
		public TerminalNode RBRACKET(int i) {
			return getToken(LuxParser.RBRACKET, i);
		}
		public List<TerminalNode> DOT() { return getTokens(LuxParser.DOT); }
		public TerminalNode DOT(int i) {
			return getToken(LuxParser.DOT, i);
		}
		public IndexFieldAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_indexFieldAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIndexFieldAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIndexFieldAssignStmt(this);
		}
	}

	public final IndexFieldAssignStmtContext indexFieldAssignStmt() throws RecognitionException {
		IndexFieldAssignStmtContext _localctx = new IndexFieldAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 78, RULE_indexFieldAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(580);
			match(IDENTIFIER);
			setState(585); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(581);
				match(LBRACKET);
				setState(582);
				expression(0);
				setState(583);
				match(RBRACKET);
				}
				}
				setState(587); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==LBRACKET );
			setState(591); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(589);
				match(DOT);
				setState(590);
				match(IDENTIFIER);
				}
				}
				setState(593); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==DOT );
			setState(595);
			match(ASSIGN);
			setState(596);
			expression(0);
			setState(597);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FieldIndexAssignStmtContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public List<TerminalNode> DOT() { return getTokens(LuxParser.DOT); }
		public TerminalNode DOT(int i) {
			return getToken(LuxParser.DOT, i);
		}
		public List<TerminalNode> LBRACKET() { return getTokens(LuxParser.LBRACKET); }
		public TerminalNode LBRACKET(int i) {
			return getToken(LuxParser.LBRACKET, i);
		}
		public List<TerminalNode> RBRACKET() { return getTokens(LuxParser.RBRACKET); }
		public TerminalNode RBRACKET(int i) {
			return getToken(LuxParser.RBRACKET, i);
		}
		public FieldIndexAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fieldIndexAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFieldIndexAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFieldIndexAssignStmt(this);
		}
	}

	public final FieldIndexAssignStmtContext fieldIndexAssignStmt() throws RecognitionException {
		FieldIndexAssignStmtContext _localctx = new FieldIndexAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 80, RULE_fieldIndexAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(599);
			match(IDENTIFIER);
			setState(602); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(600);
				match(DOT);
				setState(601);
				match(IDENTIFIER);
				}
				}
				setState(604); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==DOT );
			setState(610); 
			_errHandler.sync(this);
			_la = _input.LA(1);
			do {
				{
				{
				setState(606);
				match(LBRACKET);
				setState(607);
				expression(0);
				setState(608);
				match(RBRACKET);
				}
				}
				setState(612); 
				_errHandler.sync(this);
				_la = _input.LA(1);
			} while ( _la==LBRACKET );
			setState(614);
			match(ASSIGN);
			setState(615);
			expression(0);
			setState(616);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class DerefAssignStmtContext extends ParserRuleContext {
		public TerminalNode STAR() { return getToken(LuxParser.STAR, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public DerefAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_derefAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterDerefAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitDerefAssignStmt(this);
		}
	}

	public final DerefAssignStmtContext derefAssignStmt() throws RecognitionException {
		DerefAssignStmtContext _localctx = new DerefAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 82, RULE_derefAssignStmt);
		try {
			setState(632);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,53,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(618);
				match(STAR);
				setState(619);
				match(IDENTIFIER);
				setState(620);
				match(ASSIGN);
				setState(621);
				expression(0);
				setState(622);
				match(SEMI);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(624);
				match(STAR);
				setState(625);
				match(LPAREN);
				setState(626);
				expression(0);
				setState(627);
				match(RPAREN);
				setState(628);
				match(ASSIGN);
				setState(629);
				expression(0);
				setState(630);
				match(SEMI);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ArrowAssignStmtContext extends ParserRuleContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ArrowAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_arrowAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArrowAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArrowAssignStmt(this);
		}
	}

	public final ArrowAssignStmtContext arrowAssignStmt() throws RecognitionException {
		ArrowAssignStmtContext _localctx = new ArrowAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 84, RULE_arrowAssignStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(634);
			match(IDENTIFIER);
			setState(635);
			match(ARROW);
			setState(636);
			match(IDENTIFIER);
			setState(637);
			match(ASSIGN);
			setState(638);
			expression(0);
			setState(639);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ArrowCompoundAssignStmtContext extends ParserRuleContext {
		public Token op;
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TerminalNode PLUS_ASSIGN() { return getToken(LuxParser.PLUS_ASSIGN, 0); }
		public TerminalNode MINUS_ASSIGN() { return getToken(LuxParser.MINUS_ASSIGN, 0); }
		public TerminalNode STAR_ASSIGN() { return getToken(LuxParser.STAR_ASSIGN, 0); }
		public TerminalNode SLASH_ASSIGN() { return getToken(LuxParser.SLASH_ASSIGN, 0); }
		public TerminalNode PERCENT_ASSIGN() { return getToken(LuxParser.PERCENT_ASSIGN, 0); }
		public TerminalNode AMP_ASSIGN() { return getToken(LuxParser.AMP_ASSIGN, 0); }
		public TerminalNode PIPE_ASSIGN() { return getToken(LuxParser.PIPE_ASSIGN, 0); }
		public TerminalNode CARET_ASSIGN() { return getToken(LuxParser.CARET_ASSIGN, 0); }
		public TerminalNode LSHIFT_ASSIGN() { return getToken(LuxParser.LSHIFT_ASSIGN, 0); }
		public TerminalNode RSHIFT_ASSIGN() { return getToken(LuxParser.RSHIFT_ASSIGN, 0); }
		public ArrowCompoundAssignStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_arrowCompoundAssignStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArrowCompoundAssignStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArrowCompoundAssignStmt(this);
		}
	}

	public final ArrowCompoundAssignStmtContext arrowCompoundAssignStmt() throws RecognitionException {
		ArrowCompoundAssignStmtContext _localctx = new ArrowCompoundAssignStmtContext(_ctx, getState());
		enterRule(_localctx, 86, RULE_arrowCompoundAssignStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(641);
			match(IDENTIFIER);
			setState(642);
			match(ARROW);
			setState(643);
			match(IDENTIFIER);
			setState(644);
			((ArrowCompoundAssignStmtContext)_localctx).op = _input.LT(1);
			_la = _input.LA(1);
			if ( !(((((_la - 85)) & ~0x3f) == 0 && ((1L << (_la - 85)) & 1023L) != 0)) ) {
				((ArrowCompoundAssignStmtContext)_localctx).op = (Token)_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
			}
			setState(645);
			expression(0);
			setState(646);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class CallStmtContext extends ParserRuleContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public CallStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_callStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCallStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCallStmt(this);
		}
	}

	public final CallStmtContext callStmt() throws RecognitionException {
		CallStmtContext _localctx = new CallStmtContext(_ctx, getState());
		enterRule(_localctx, 88, RULE_callStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(648);
			match(IDENTIFIER);
			setState(649);
			match(LPAREN);
			setState(651);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
				{
				setState(650);
				argList();
				}
			}

			setState(653);
			match(RPAREN);
			setState(654);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ArgListContext extends ParserRuleContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ArgListContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_argList; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArgList(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArgList(this);
		}
	}

	public final ArgListContext argList() throws RecognitionException {
		ArgListContext _localctx = new ArgListContext(_ctx, getState());
		enterRule(_localctx, 90, RULE_argList);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(656);
			expression(0);
			setState(661);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(657);
				match(COMMA);
				setState(658);
				expression(0);
				}
				}
				setState(663);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ReturnStmtContext extends ParserRuleContext {
		public TerminalNode RET() { return getToken(LuxParser.RET, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public ReturnStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_returnStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterReturnStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitReturnStmt(this);
		}
	}

	public final ReturnStmtContext returnStmt() throws RecognitionException {
		ReturnStmtContext _localctx = new ReturnStmtContext(_ctx, getState());
		enterRule(_localctx, 92, RULE_returnStmt);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(664);
			match(RET);
			setState(666);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
				{
				setState(665);
				expression(0);
				}
			}

			setState(668);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class IfStmtContext extends ParserRuleContext {
		public TerminalNode IF() { return getToken(LuxParser.IF, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public IfBodyContext ifBody() {
			return getRuleContext(IfBodyContext.class,0);
		}
		public List<ElseIfClauseContext> elseIfClause() {
			return getRuleContexts(ElseIfClauseContext.class);
		}
		public ElseIfClauseContext elseIfClause(int i) {
			return getRuleContext(ElseIfClauseContext.class,i);
		}
		public ElseClauseContext elseClause() {
			return getRuleContext(ElseClauseContext.class,0);
		}
		public IfStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_ifStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIfStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIfStmt(this);
		}
	}

	public final IfStmtContext ifStmt() throws RecognitionException {
		IfStmtContext _localctx = new IfStmtContext(_ctx, getState());
		enterRule(_localctx, 94, RULE_ifStmt);
		try {
			int _alt;
			setState(696);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,61,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(670);
				match(IF);
				setState(671);
				match(LPAREN);
				setState(672);
				expression(0);
				setState(673);
				match(RPAREN);
				setState(674);
				ifBody();
				setState(678);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,57,_ctx);
				while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
					if ( _alt==1 ) {
						{
						{
						setState(675);
						elseIfClause();
						}
						} 
					}
					setState(680);
					_errHandler.sync(this);
					_alt = getInterpreter().adaptivePredict(_input,57,_ctx);
				}
				setState(682);
				_errHandler.sync(this);
				switch ( getInterpreter().adaptivePredict(_input,58,_ctx) ) {
				case 1:
					{
					setState(681);
					elseClause();
					}
					break;
				}
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(684);
				match(IF);
				setState(685);
				expression(0);
				setState(686);
				ifBody();
				setState(690);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,59,_ctx);
				while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
					if ( _alt==1 ) {
						{
						{
						setState(687);
						elseIfClause();
						}
						} 
					}
					setState(692);
					_errHandler.sync(this);
					_alt = getInterpreter().adaptivePredict(_input,59,_ctx);
				}
				setState(694);
				_errHandler.sync(this);
				switch ( getInterpreter().adaptivePredict(_input,60,_ctx) ) {
				case 1:
					{
					setState(693);
					elseClause();
					}
					break;
				}
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ElseIfClauseContext extends ParserRuleContext {
		public TerminalNode ELSE() { return getToken(LuxParser.ELSE, 0); }
		public TerminalNode IF() { return getToken(LuxParser.IF, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public IfBodyContext ifBody() {
			return getRuleContext(IfBodyContext.class,0);
		}
		public ElseIfClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_elseIfClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterElseIfClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitElseIfClause(this);
		}
	}

	public final ElseIfClauseContext elseIfClause() throws RecognitionException {
		ElseIfClauseContext _localctx = new ElseIfClauseContext(_ctx, getState());
		enterRule(_localctx, 96, RULE_elseIfClause);
		try {
			setState(710);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,62,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(698);
				match(ELSE);
				setState(699);
				match(IF);
				setState(700);
				match(LPAREN);
				setState(701);
				expression(0);
				setState(702);
				match(RPAREN);
				setState(703);
				ifBody();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(705);
				match(ELSE);
				setState(706);
				match(IF);
				setState(707);
				expression(0);
				setState(708);
				ifBody();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ElseClauseContext extends ParserRuleContext {
		public TerminalNode ELSE() { return getToken(LuxParser.ELSE, 0); }
		public IfBodyContext ifBody() {
			return getRuleContext(IfBodyContext.class,0);
		}
		public ElseClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_elseClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterElseClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitElseClause(this);
		}
	}

	public final ElseClauseContext elseClause() throws RecognitionException {
		ElseClauseContext _localctx = new ElseClauseContext(_ctx, getState());
		enterRule(_localctx, 98, RULE_elseClause);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(712);
			match(ELSE);
			setState(713);
			ifBody();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class IfBodyContext extends ParserRuleContext {
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public StatementContext statement() {
			return getRuleContext(StatementContext.class,0);
		}
		public IfBodyContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_ifBody; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIfBody(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIfBody(this);
		}
	}

	public final IfBodyContext ifBody() throws RecognitionException {
		IfBodyContext _localctx = new IfBodyContext(_ctx, getState());
		enterRule(_localctx, 100, RULE_ifBody);
		try {
			setState(717);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,63,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(715);
				block();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(716);
				statement();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ForStmtContext extends ParserRuleContext {
		public ForStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_forStmt; }
	 
		public ForStmtContext() { }
		public void copyFrom(ForStmtContext ctx) {
			super.copyFrom(ctx);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ForInStmtContext extends ForStmtContext {
		public TerminalNode FOR() { return getToken(LuxParser.FOR, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode IN() { return getToken(LuxParser.IN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public ForInStmtContext(ForStmtContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterForInStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitForInStmt(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ForClassicStmtContext extends ForStmtContext {
		public TerminalNode FOR() { return getToken(LuxParser.FOR, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode ASSIGN() { return getToken(LuxParser.ASSIGN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> SEMI() { return getTokens(LuxParser.SEMI); }
		public TerminalNode SEMI(int i) {
			return getToken(LuxParser.SEMI, i);
		}
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public ForClassicStmtContext(ForStmtContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterForClassicStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitForClassicStmt(this);
		}
	}

	public final ForStmtContext forStmt() throws RecognitionException {
		ForStmtContext _localctx = new ForStmtContext(_ctx, getState());
		enterRule(_localctx, 102, RULE_forStmt);
		try {
			setState(737);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,64,_ctx) ) {
			case 1:
				_localctx = new ForInStmtContext(_localctx);
				enterOuterAlt(_localctx, 1);
				{
				setState(719);
				match(FOR);
				setState(720);
				typeSpec(0);
				setState(721);
				match(IDENTIFIER);
				setState(722);
				match(IN);
				setState(723);
				expression(0);
				setState(724);
				block();
				}
				break;
			case 2:
				_localctx = new ForClassicStmtContext(_localctx);
				enterOuterAlt(_localctx, 2);
				{
				setState(726);
				match(FOR);
				setState(727);
				typeSpec(0);
				setState(728);
				match(IDENTIFIER);
				setState(729);
				match(ASSIGN);
				setState(730);
				expression(0);
				setState(731);
				match(SEMI);
				setState(732);
				expression(0);
				setState(733);
				match(SEMI);
				setState(734);
				expression(0);
				setState(735);
				block();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class BreakStmtContext extends ParserRuleContext {
		public TerminalNode BREAK() { return getToken(LuxParser.BREAK, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public BreakStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_breakStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBreakStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBreakStmt(this);
		}
	}

	public final BreakStmtContext breakStmt() throws RecognitionException {
		BreakStmtContext _localctx = new BreakStmtContext(_ctx, getState());
		enterRule(_localctx, 104, RULE_breakStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(739);
			match(BREAK);
			setState(740);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ContinueStmtContext extends ParserRuleContext {
		public TerminalNode CONTINUE() { return getToken(LuxParser.CONTINUE, 0); }
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ContinueStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_continueStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterContinueStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitContinueStmt(this);
		}
	}

	public final ContinueStmtContext continueStmt() throws RecognitionException {
		ContinueStmtContext _localctx = new ContinueStmtContext(_ctx, getState());
		enterRule(_localctx, 106, RULE_continueStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(742);
			match(CONTINUE);
			setState(743);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class LoopStmtContext extends ParserRuleContext {
		public TerminalNode LOOP() { return getToken(LuxParser.LOOP, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public LoopStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_loopStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLoopStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLoopStmt(this);
		}
	}

	public final LoopStmtContext loopStmt() throws RecognitionException {
		LoopStmtContext _localctx = new LoopStmtContext(_ctx, getState());
		enterRule(_localctx, 108, RULE_loopStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(745);
			match(LOOP);
			setState(746);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class WhileStmtContext extends ParserRuleContext {
		public TerminalNode WHILE() { return getToken(LuxParser.WHILE, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public WhileStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_whileStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterWhileStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitWhileStmt(this);
		}
	}

	public final WhileStmtContext whileStmt() throws RecognitionException {
		WhileStmtContext _localctx = new WhileStmtContext(_ctx, getState());
		enterRule(_localctx, 110, RULE_whileStmt);
		try {
			setState(758);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,65,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(748);
				match(WHILE);
				setState(749);
				expression(0);
				setState(750);
				block();
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(752);
				match(WHILE);
				setState(753);
				match(LPAREN);
				setState(754);
				expression(0);
				setState(755);
				match(RPAREN);
				setState(756);
				block();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class DoWhileStmtContext extends ParserRuleContext {
		public TerminalNode DO() { return getToken(LuxParser.DO, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public TerminalNode WHILE() { return getToken(LuxParser.WHILE, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public DoWhileStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_doWhileStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterDoWhileStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitDoWhileStmt(this);
		}
	}

	public final DoWhileStmtContext doWhileStmt() throws RecognitionException {
		DoWhileStmtContext _localctx = new DoWhileStmtContext(_ctx, getState());
		enterRule(_localctx, 112, RULE_doWhileStmt);
		try {
			setState(774);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,66,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(760);
				match(DO);
				setState(761);
				block();
				setState(762);
				match(WHILE);
				setState(763);
				expression(0);
				setState(764);
				match(SEMI);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(766);
				match(DO);
				setState(767);
				block();
				setState(768);
				match(WHILE);
				setState(769);
				match(LPAREN);
				setState(770);
				expression(0);
				setState(771);
				match(RPAREN);
				setState(772);
				match(SEMI);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class LockStmtContext extends ParserRuleContext {
		public TerminalNode LOCK() { return getToken(LuxParser.LOCK, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public LockStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_lockStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLockStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLockStmt(this);
		}
	}

	public final LockStmtContext lockStmt() throws RecognitionException {
		LockStmtContext _localctx = new LockStmtContext(_ctx, getState());
		enterRule(_localctx, 114, RULE_lockStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(776);
			match(LOCK);
			setState(777);
			match(LPAREN);
			setState(778);
			expression(0);
			setState(779);
			match(RPAREN);
			setState(780);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TryCatchStmtContext extends ParserRuleContext {
		public TerminalNode TRY() { return getToken(LuxParser.TRY, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public List<CatchClauseContext> catchClause() {
			return getRuleContexts(CatchClauseContext.class);
		}
		public CatchClauseContext catchClause(int i) {
			return getRuleContext(CatchClauseContext.class,i);
		}
		public FinallyClauseContext finallyClause() {
			return getRuleContext(FinallyClauseContext.class,0);
		}
		public TryCatchStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_tryCatchStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTryCatchStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTryCatchStmt(this);
		}
	}

	public final TryCatchStmtContext tryCatchStmt() throws RecognitionException {
		TryCatchStmtContext _localctx = new TryCatchStmtContext(_ctx, getState());
		enterRule(_localctx, 116, RULE_tryCatchStmt);
		int _la;
		try {
			setState(796);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,69,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(782);
				match(TRY);
				setState(783);
				block();
				setState(785); 
				_errHandler.sync(this);
				_la = _input.LA(1);
				do {
					{
					{
					setState(784);
					catchClause();
					}
					}
					setState(787); 
					_errHandler.sync(this);
					_la = _input.LA(1);
				} while ( _la==CATCH );
				setState(790);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==FINALLY) {
					{
					setState(789);
					finallyClause();
					}
				}

				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(792);
				match(TRY);
				setState(793);
				block();
				setState(794);
				finallyClause();
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class CatchClauseContext extends ParserRuleContext {
		public TerminalNode CATCH() { return getToken(LuxParser.CATCH, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public CatchClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_catchClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCatchClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCatchClause(this);
		}
	}

	public final CatchClauseContext catchClause() throws RecognitionException {
		CatchClauseContext _localctx = new CatchClauseContext(_ctx, getState());
		enterRule(_localctx, 118, RULE_catchClause);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(798);
			match(CATCH);
			setState(799);
			match(LPAREN);
			setState(800);
			typeSpec(0);
			setState(801);
			match(IDENTIFIER);
			setState(802);
			match(RPAREN);
			setState(803);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FinallyClauseContext extends ParserRuleContext {
		public TerminalNode FINALLY() { return getToken(LuxParser.FINALLY, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public FinallyClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_finallyClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFinallyClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFinallyClause(this);
		}
	}

	public final FinallyClauseContext finallyClause() throws RecognitionException {
		FinallyClauseContext _localctx = new FinallyClauseContext(_ctx, getState());
		enterRule(_localctx, 120, RULE_finallyClause);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(805);
			match(FINALLY);
			setState(806);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ThrowStmtContext extends ParserRuleContext {
		public TerminalNode THROW() { return getToken(LuxParser.THROW, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode SEMI() { return getToken(LuxParser.SEMI, 0); }
		public ThrowStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_throwStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterThrowStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitThrowStmt(this);
		}
	}

	public final ThrowStmtContext throwStmt() throws RecognitionException {
		ThrowStmtContext _localctx = new ThrowStmtContext(_ctx, getState());
		enterRule(_localctx, 122, RULE_throwStmt);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(808);
			match(THROW);
			setState(809);
			expression(0);
			setState(810);
			match(SEMI);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class SwitchStmtContext extends ParserRuleContext {
		public TerminalNode SWITCH() { return getToken(LuxParser.SWITCH, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<CaseClauseContext> caseClause() {
			return getRuleContexts(CaseClauseContext.class);
		}
		public CaseClauseContext caseClause(int i) {
			return getRuleContext(CaseClauseContext.class,i);
		}
		public DefaultClauseContext defaultClause() {
			return getRuleContext(DefaultClauseContext.class,0);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public SwitchStmtContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_switchStmt; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterSwitchStmt(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitSwitchStmt(this);
		}
	}

	public final SwitchStmtContext switchStmt() throws RecognitionException {
		SwitchStmtContext _localctx = new SwitchStmtContext(_ctx, getState());
		enterRule(_localctx, 124, RULE_switchStmt);
		int _la;
		try {
			setState(842);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,74,_ctx) ) {
			case 1:
				enterOuterAlt(_localctx, 1);
				{
				setState(812);
				match(SWITCH);
				setState(813);
				expression(0);
				setState(814);
				match(LBRACE);
				setState(818);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==CASE) {
					{
					{
					setState(815);
					caseClause();
					}
					}
					setState(820);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(822);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==DEFAULT) {
					{
					setState(821);
					defaultClause();
					}
				}

				setState(824);
				match(RBRACE);
				}
				break;
			case 2:
				enterOuterAlt(_localctx, 2);
				{
				setState(826);
				match(SWITCH);
				setState(827);
				match(LPAREN);
				setState(828);
				expression(0);
				setState(829);
				match(RPAREN);
				setState(830);
				match(LBRACE);
				setState(834);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==CASE) {
					{
					{
					setState(831);
					caseClause();
					}
					}
					setState(836);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(838);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==DEFAULT) {
					{
					setState(837);
					defaultClause();
					}
				}

				setState(840);
				match(RBRACE);
				}
				break;
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class CaseClauseContext extends ParserRuleContext {
		public TerminalNode CASE() { return getToken(LuxParser.CASE, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public CaseClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_caseClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCaseClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCaseClause(this);
		}
	}

	public final CaseClauseContext caseClause() throws RecognitionException {
		CaseClauseContext _localctx = new CaseClauseContext(_ctx, getState());
		enterRule(_localctx, 126, RULE_caseClause);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(844);
			match(CASE);
			setState(845);
			expression(0);
			setState(850);
			_errHandler.sync(this);
			_la = _input.LA(1);
			while (_la==COMMA) {
				{
				{
				setState(846);
				match(COMMA);
				setState(847);
				expression(0);
				}
				}
				setState(852);
				_errHandler.sync(this);
				_la = _input.LA(1);
			}
			setState(853);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class DefaultClauseContext extends ParserRuleContext {
		public TerminalNode DEFAULT() { return getToken(LuxParser.DEFAULT, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public DefaultClauseContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_defaultClause; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterDefaultClause(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitDefaultClause(this);
		}
	}

	public final DefaultClauseContext defaultClause() throws RecognitionException {
		DefaultClauseContext _localctx = new DefaultClauseContext(_ctx, getState());
		enterRule(_localctx, 128, RULE_defaultClause);
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(855);
			match(DEFAULT);
			setState(856);
			block();
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class ExpressionContext extends ParserRuleContext {
		public ExpressionContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_expression; }
	 
		public ExpressionContext() { }
		public void copyFrom(ExpressionContext ctx) {
			super.copyFrom(ctx);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class FieldAccessExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode DOT() { return getToken(LuxParser.DOT, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public FieldAccessExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFieldAccessExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFieldAccessExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TypeofExprContext extends ExpressionContext {
		public TerminalNode TYPEOF() { return getToken(LuxParser.TYPEOF, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TypeofExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTypeofExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTypeofExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class GenericEnumNamedVariantExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public List<TerminalNode> COLON() { return getTokens(LuxParser.COLON); }
		public TerminalNode COLON(int i) {
			return getToken(LuxParser.COLON, i);
		}
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public GenericEnumNamedVariantExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterGenericEnumNamedVariantExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitGenericEnumNamedVariantExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class RshiftExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> GT() { return getTokens(LuxParser.GT); }
		public TerminalNode GT(int i) {
			return getToken(LuxParser.GT, i);
		}
		public RshiftExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterRshiftExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitRshiftExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ArrowMethodCallExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public ArrowMethodCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArrowMethodCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArrowMethodCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class OctLitExprContext extends ExpressionContext {
		public TerminalNode OCT_LIT() { return getToken(LuxParser.OCT_LIT, 0); }
		public OctLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterOctLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitOctLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BitXorExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode CARET() { return getToken(LuxParser.CARET, 0); }
		public BitXorExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBitXorExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBitXorExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class LogicalNotExprContext extends ExpressionContext {
		public TerminalNode NOT() { return getToken(LuxParser.NOT, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public LogicalNotExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLogicalNotExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLogicalNotExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class IdentExprContext extends ExpressionContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public IdentExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIdentExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIdentExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class PreIncrExprContext extends ExpressionContext {
		public TerminalNode INCR() { return getToken(LuxParser.INCR, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public PreIncrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPreIncrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPreIncrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TernaryExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode QUESTION() { return getToken(LuxParser.QUESTION, 0); }
		public TerminalNode COLON() { return getToken(LuxParser.COLON, 0); }
		public TernaryExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTernaryExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTernaryExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ChainedTupleIndexExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode DOT() { return getToken(LuxParser.DOT, 0); }
		public TerminalNode FLOAT_LIT() { return getToken(LuxParser.FLOAT_LIT, 0); }
		public ChainedTupleIndexExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterChainedTupleIndexExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitChainedTupleIndexExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class NullLitExprContext extends ExpressionContext {
		public TerminalNode NULL_LIT() { return getToken(LuxParser.NULL_LIT, 0); }
		public NullLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterNullLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitNullLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class MulExprContext extends ExpressionContext {
		public Token op;
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode STAR() { return getToken(LuxParser.STAR, 0); }
		public TerminalNode SLASH() { return getToken(LuxParser.SLASH, 0); }
		public TerminalNode PERCENT() { return getToken(LuxParser.PERCENT, 0); }
		public MulExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterMulExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitMulExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BitAndExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode AMPERSAND() { return getToken(LuxParser.AMPERSAND, 0); }
		public BitAndExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBitAndExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBitAndExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class IsExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode IS() { return getToken(LuxParser.IS, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public IsExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIsExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIsExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class LshiftExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode LSHIFT() { return getToken(LuxParser.LSHIFT, 0); }
		public LshiftExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLshiftExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLshiftExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TupleLitExprContext extends ExpressionContext {
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TupleLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTupleLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTupleLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class AddSubExprContext extends ExpressionContext {
		public Token op;
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode PLUS() { return getToken(LuxParser.PLUS, 0); }
		public TerminalNode MINUS() { return getToken(LuxParser.MINUS, 0); }
		public AddSubExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterAddSubExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitAddSubExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class IntLitExprContext extends ExpressionContext {
		public TerminalNode INT_LIT() { return getToken(LuxParser.INT_LIT, 0); }
		public IntLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIntLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIntLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class AddrOfExprContext extends ExpressionContext {
		public TerminalNode AMPERSAND() { return getToken(LuxParser.AMPERSAND, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public AddrOfExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterAddrOfExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitAddrOfExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TupleIndexExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode DOT() { return getToken(LuxParser.DOT, 0); }
		public TerminalNode INT_LIT() { return getToken(LuxParser.INT_LIT, 0); }
		public TupleIndexExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTupleIndexExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTupleIndexExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class FloatLitExprContext extends ExpressionContext {
		public TerminalNode FLOAT_LIT() { return getToken(LuxParser.FLOAT_LIT, 0); }
		public FloatLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFloatLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFloatLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class GenericStructLitExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public List<TerminalNode> COLON() { return getTokens(LuxParser.COLON); }
		public TerminalNode COLON(int i) {
			return getToken(LuxParser.COLON, i);
		}
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public GenericStructLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterGenericStructLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitGenericStructLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class SpawnExprContext extends ExpressionContext {
		public TerminalNode SPAWN() { return getToken(LuxParser.SPAWN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public SpawnExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterSpawnExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitSpawnExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ArrowAccessExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public ArrowAccessExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArrowAccessExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArrowAccessExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class GenericStaticMethodCallExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public GenericStaticMethodCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterGenericStaticMethodCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitGenericStaticMethodCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ListCompExprContext extends ExpressionContext {
		public TerminalNode LBRACKET() { return getToken(LuxParser.LBRACKET, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode PIPE() { return getToken(LuxParser.PIPE, 0); }
		public TerminalNode FOR() { return getToken(LuxParser.FOR, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode IN() { return getToken(LuxParser.IN, 0); }
		public TerminalNode RBRACKET() { return getToken(LuxParser.RBRACKET, 0); }
		public TerminalNode IF() { return getToken(LuxParser.IF, 0); }
		public ListCompExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterListCompExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitListCompExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class IndexExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode LBRACKET() { return getToken(LuxParser.LBRACKET, 0); }
		public TerminalNode RBRACKET() { return getToken(LuxParser.RBRACKET, 0); }
		public IndexExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterIndexExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitIndexExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class NegExprContext extends ExpressionContext {
		public TerminalNode MINUS() { return getToken(LuxParser.MINUS, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public NegExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterNegExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitNegExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class DerefExprContext extends ExpressionContext {
		public TerminalNode STAR() { return getToken(LuxParser.STAR, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public DerefExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterDerefExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitDerefExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class PreDecrExprContext extends ExpressionContext {
		public TerminalNode DECR() { return getToken(LuxParser.DECR, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public PreDecrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPreDecrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPreDecrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class SpreadExprContext extends ExpressionContext {
		public TerminalNode SPREAD() { return getToken(LuxParser.SPREAD, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public SpreadExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterSpreadExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitSpreadExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class CatchUnwrapExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode CATCH() { return getToken(LuxParser.CATCH, 0); }
		public BlockContext block() {
			return getRuleContext(BlockContext.class,0);
		}
		public CatchUnwrapExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCatchUnwrapExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCatchUnwrapExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class StaticMethodCallExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public StaticMethodCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStaticMethodCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStaticMethodCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class NullCoalExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode NULLCOAL() { return getToken(LuxParser.NULLCOAL, 0); }
		public NullCoalExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterNullCoalExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitNullCoalExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class CastExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode AS() { return getToken(LuxParser.AS, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public CastExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCastExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCastExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class GenericFnCallExprContext extends ExpressionContext {
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public GenericFnCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterGenericFnCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitGenericFnCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class GenericEnumAccessExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public GenericEnumAccessExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterGenericEnumAccessExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitGenericEnumAccessExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class EnumAccessExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public EnumAccessExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEnumAccessExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEnumAccessExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ParenExprContext extends ExpressionContext {
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public ParenExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterParenExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitParenExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BitNotExprContext extends ExpressionContext {
		public TerminalNode TILDE() { return getToken(LuxParser.TILDE, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public BitNotExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBitNotExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBitNotExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ArrayLitExprContext extends ExpressionContext {
		public TerminalNode LBRACKET() { return getToken(LuxParser.LBRACKET, 0); }
		public TerminalNode RBRACKET() { return getToken(LuxParser.RBRACKET, 0); }
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public ArrayLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterArrayLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitArrayLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class MethodCallExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode DOT() { return getToken(LuxParser.DOT, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public MethodCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterMethodCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitMethodCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class EnumNamedVariantExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode SCOPE() { return getToken(LuxParser.SCOPE, 0); }
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<TerminalNode> COLON() { return getTokens(LuxParser.COLON); }
		public TerminalNode COLON(int i) {
			return getToken(LuxParser.COLON, i);
		}
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public EnumNamedVariantExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEnumNamedVariantExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEnumNamedVariantExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class StructLitExprContext extends ExpressionContext {
		public List<TerminalNode> IDENTIFIER() { return getTokens(LuxParser.IDENTIFIER); }
		public TerminalNode IDENTIFIER(int i) {
			return getToken(LuxParser.IDENTIFIER, i);
		}
		public TerminalNode LBRACE() { return getToken(LuxParser.LBRACE, 0); }
		public TerminalNode RBRACE() { return getToken(LuxParser.RBRACE, 0); }
		public List<TerminalNode> COLON() { return getTokens(LuxParser.COLON); }
		public TerminalNode COLON(int i) {
			return getToken(LuxParser.COLON, i);
		}
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public StructLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStructLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStructLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class PostDecrExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode DECR() { return getToken(LuxParser.DECR, 0); }
		public PostDecrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPostDecrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPostDecrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class RelExprContext extends ExpressionContext {
		public Token op;
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode LTE() { return getToken(LuxParser.LTE, 0); }
		public TerminalNode GTE() { return getToken(LuxParser.GTE, 0); }
		public RelExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterRelExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitRelExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BinLitExprContext extends ExpressionContext {
		public TerminalNode BIN_LIT() { return getToken(LuxParser.BIN_LIT, 0); }
		public BinLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBinLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBinLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class RangeInclExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode RANGE_INCL() { return getToken(LuxParser.RANGE_INCL, 0); }
		public RangeInclExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterRangeInclExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitRangeInclExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TupleArrowIndexExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public TerminalNode INT_LIT() { return getToken(LuxParser.INT_LIT, 0); }
		public TupleArrowIndexExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTupleArrowIndexExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTupleArrowIndexExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class LogicalAndExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode LAND() { return getToken(LuxParser.LAND, 0); }
		public LogicalAndExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLogicalAndExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLogicalAndExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class StrLitExprContext extends ExpressionContext {
		public TerminalNode STR_LIT() { return getToken(LuxParser.STR_LIT, 0); }
		public StrLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterStrLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitStrLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class AwaitExprContext extends ExpressionContext {
		public TerminalNode AWAIT() { return getToken(LuxParser.AWAIT, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public AwaitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterAwaitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitAwaitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class CStrLitExprContext extends ExpressionContext {
		public TerminalNode C_STR_LIT() { return getToken(LuxParser.C_STR_LIT, 0); }
		public CStrLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCStrLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCStrLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class FnCallExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public ArgListContext argList() {
			return getRuleContext(ArgListContext.class,0);
		}
		public FnCallExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFnCallExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFnCallExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class LogicalOrExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode LOR() { return getToken(LuxParser.LOR, 0); }
		public LogicalOrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterLogicalOrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitLogicalOrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class SizeofExprContext extends ExpressionContext {
		public TerminalNode SIZEOF() { return getToken(LuxParser.SIZEOF, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TypeSpecContext typeSpec() {
			return getRuleContext(TypeSpecContext.class,0);
		}
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public SizeofExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterSizeofExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitSizeofExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class EqExprContext extends ExpressionContext {
		public Token op;
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode EQ() { return getToken(LuxParser.EQ, 0); }
		public TerminalNode NEQ() { return getToken(LuxParser.NEQ, 0); }
		public EqExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterEqExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitEqExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BitOrExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode PIPE() { return getToken(LuxParser.PIPE, 0); }
		public BitOrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBitOrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBitOrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class CharLitExprContext extends ExpressionContext {
		public TerminalNode CHAR_LIT() { return getToken(LuxParser.CHAR_LIT, 0); }
		public CharLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterCharLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitCharLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class TryExprContext extends ExpressionContext {
		public TerminalNode TRY() { return getToken(LuxParser.TRY, 0); }
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TryExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTryExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTryExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class PostIncrExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode INCR() { return getToken(LuxParser.INCR, 0); }
		public PostIncrExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPostIncrExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPostIncrExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class BoolLitExprContext extends ExpressionContext {
		public TerminalNode BOOL_LIT() { return getToken(LuxParser.BOOL_LIT, 0); }
		public BoolLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterBoolLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitBoolLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class HexLitExprContext extends ExpressionContext {
		public TerminalNode HEX_LIT() { return getToken(LuxParser.HEX_LIT, 0); }
		public HexLitExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterHexLitExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitHexLitExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class ChainedTupleArrowIndexExprContext extends ExpressionContext {
		public ExpressionContext expression() {
			return getRuleContext(ExpressionContext.class,0);
		}
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public TerminalNode FLOAT_LIT() { return getToken(LuxParser.FLOAT_LIT, 0); }
		public ChainedTupleArrowIndexExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterChainedTupleArrowIndexExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitChainedTupleArrowIndexExpr(this);
		}
	}
	@SuppressWarnings("CheckReturnValue")
	public static class RangeExprContext extends ExpressionContext {
		public List<ExpressionContext> expression() {
			return getRuleContexts(ExpressionContext.class);
		}
		public ExpressionContext expression(int i) {
			return getRuleContext(ExpressionContext.class,i);
		}
		public TerminalNode RANGE() { return getToken(LuxParser.RANGE, 0); }
		public RangeExprContext(ExpressionContext ctx) { copyFrom(ctx); }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterRangeExpr(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitRangeExpr(this);
		}
	}

	public final ExpressionContext expression() throws RecognitionException {
		return expression(0);
	}

	private ExpressionContext expression(int _p) throws RecognitionException {
		ParserRuleContext _parentctx = _ctx;
		int _parentState = getState();
		ExpressionContext _localctx = new ExpressionContext(_ctx, _parentState);
		ExpressionContext _prevctx = _localctx;
		int _startState = 130;
		enterRecursionRule(_localctx, 130, RULE_expression, _p);
		int _la;
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(1100);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,96,_ctx) ) {
			case 1:
				{
				_localctx = new GenericFnCallExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;

				setState(859);
				match(IDENTIFIER);
				setState(860);
				match(LT);
				setState(861);
				typeSpec(0);
				setState(866);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(862);
					match(COMMA);
					setState(863);
					typeSpec(0);
					}
					}
					setState(868);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(869);
				match(GT);
				setState(870);
				match(LPAREN);
				setState(872);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
					{
					setState(871);
					argList();
					}
				}

				setState(874);
				match(RPAREN);
				}
				break;
			case 2:
				{
				_localctx = new GenericStaticMethodCallExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(876);
				match(IDENTIFIER);
				setState(877);
				match(LT);
				setState(878);
				typeSpec(0);
				setState(883);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(879);
					match(COMMA);
					setState(880);
					typeSpec(0);
					}
					}
					setState(885);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(886);
				match(GT);
				setState(887);
				match(SCOPE);
				setState(888);
				match(IDENTIFIER);
				setState(889);
				match(LPAREN);
				setState(891);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
					{
					setState(890);
					argList();
					}
				}

				setState(893);
				match(RPAREN);
				}
				break;
			case 3:
				{
				_localctx = new StructLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(895);
				match(IDENTIFIER);
				setState(896);
				match(LBRACE);
				setState(909);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==IDENTIFIER) {
					{
					setState(897);
					match(IDENTIFIER);
					setState(898);
					match(COLON);
					setState(899);
					expression(0);
					setState(906);
					_errHandler.sync(this);
					_la = _input.LA(1);
					while (_la==COMMA) {
						{
						{
						setState(900);
						match(COMMA);
						setState(901);
						match(IDENTIFIER);
						setState(902);
						match(COLON);
						setState(903);
						expression(0);
						}
						}
						setState(908);
						_errHandler.sync(this);
						_la = _input.LA(1);
					}
					}
				}

				setState(911);
				match(RBRACE);
				}
				break;
			case 4:
				{
				_localctx = new GenericStructLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(912);
				match(IDENTIFIER);
				setState(913);
				match(LT);
				setState(914);
				typeSpec(0);
				setState(919);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(915);
					match(COMMA);
					setState(916);
					typeSpec(0);
					}
					}
					setState(921);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(922);
				match(GT);
				setState(923);
				match(LBRACE);
				setState(936);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==IDENTIFIER) {
					{
					setState(924);
					match(IDENTIFIER);
					setState(925);
					match(COLON);
					setState(926);
					expression(0);
					setState(933);
					_errHandler.sync(this);
					_la = _input.LA(1);
					while (_la==COMMA) {
						{
						{
						setState(927);
						match(COMMA);
						setState(928);
						match(IDENTIFIER);
						setState(929);
						match(COLON);
						setState(930);
						expression(0);
						}
						}
						setState(935);
						_errHandler.sync(this);
						_la = _input.LA(1);
					}
					}
				}

				setState(938);
				match(RBRACE);
				}
				break;
			case 5:
				{
				_localctx = new StaticMethodCallExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(940);
				match(IDENTIFIER);
				setState(941);
				match(SCOPE);
				setState(942);
				match(IDENTIFIER);
				setState(943);
				match(LPAREN);
				setState(945);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
					{
					setState(944);
					argList();
					}
				}

				setState(947);
				match(RPAREN);
				}
				break;
			case 6:
				{
				_localctx = new GenericEnumAccessExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(948);
				match(IDENTIFIER);
				setState(949);
				match(LT);
				setState(950);
				typeSpec(0);
				setState(955);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(951);
					match(COMMA);
					setState(952);
					typeSpec(0);
					}
					}
					setState(957);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(958);
				match(GT);
				setState(959);
				match(SCOPE);
				setState(960);
				match(IDENTIFIER);
				}
				break;
			case 7:
				{
				_localctx = new EnumNamedVariantExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(962);
				match(IDENTIFIER);
				setState(963);
				match(SCOPE);
				setState(964);
				match(IDENTIFIER);
				setState(965);
				match(LBRACE);
				setState(978);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==IDENTIFIER) {
					{
					setState(966);
					match(IDENTIFIER);
					setState(967);
					match(COLON);
					setState(968);
					expression(0);
					setState(975);
					_errHandler.sync(this);
					_la = _input.LA(1);
					while (_la==COMMA) {
						{
						{
						setState(969);
						match(COMMA);
						setState(970);
						match(IDENTIFIER);
						setState(971);
						match(COLON);
						setState(972);
						expression(0);
						}
						}
						setState(977);
						_errHandler.sync(this);
						_la = _input.LA(1);
					}
					}
				}

				setState(980);
				match(RBRACE);
				}
				break;
			case 8:
				{
				_localctx = new GenericEnumNamedVariantExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(981);
				match(IDENTIFIER);
				setState(982);
				match(LT);
				setState(983);
				typeSpec(0);
				setState(988);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(984);
					match(COMMA);
					setState(985);
					typeSpec(0);
					}
					}
					setState(990);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(991);
				match(GT);
				setState(992);
				match(SCOPE);
				setState(993);
				match(IDENTIFIER);
				setState(994);
				match(LBRACE);
				setState(1007);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==IDENTIFIER) {
					{
					setState(995);
					match(IDENTIFIER);
					setState(996);
					match(COLON);
					setState(997);
					expression(0);
					setState(1004);
					_errHandler.sync(this);
					_la = _input.LA(1);
					while (_la==COMMA) {
						{
						{
						setState(998);
						match(COMMA);
						setState(999);
						match(IDENTIFIER);
						setState(1000);
						match(COLON);
						setState(1001);
						expression(0);
						}
						}
						setState(1006);
						_errHandler.sync(this);
						_la = _input.LA(1);
					}
					}
				}

				setState(1009);
				match(RBRACE);
				}
				break;
			case 9:
				{
				_localctx = new EnumAccessExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1011);
				match(IDENTIFIER);
				setState(1012);
				match(SCOPE);
				setState(1013);
				match(IDENTIFIER);
				}
				break;
			case 10:
				{
				_localctx = new DerefExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1014);
				match(STAR);
				setState(1015);
				expression(44);
				}
				break;
			case 11:
				{
				_localctx = new AddrOfExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1016);
				match(AMPERSAND);
				setState(1017);
				expression(43);
				}
				break;
			case 12:
				{
				_localctx = new NegExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1018);
				match(MINUS);
				setState(1019);
				expression(42);
				}
				break;
			case 13:
				{
				_localctx = new SpawnExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1020);
				match(SPAWN);
				setState(1021);
				expression(41);
				}
				break;
			case 14:
				{
				_localctx = new AwaitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1022);
				match(AWAIT);
				setState(1023);
				expression(40);
				}
				break;
			case 15:
				{
				_localctx = new LogicalNotExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1024);
				match(NOT);
				setState(1025);
				expression(39);
				}
				break;
			case 16:
				{
				_localctx = new BitNotExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1026);
				match(TILDE);
				setState(1027);
				expression(38);
				}
				break;
			case 17:
				{
				_localctx = new PreIncrExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1028);
				match(INCR);
				setState(1029);
				expression(37);
				}
				break;
			case 18:
				{
				_localctx = new PreDecrExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1030);
				match(DECR);
				setState(1031);
				expression(36);
				}
				break;
			case 19:
				{
				_localctx = new SizeofExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1032);
				match(SIZEOF);
				setState(1033);
				match(LPAREN);
				setState(1034);
				typeSpec(0);
				setState(1035);
				match(RPAREN);
				}
				break;
			case 20:
				{
				_localctx = new TypeofExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1037);
				match(TYPEOF);
				setState(1038);
				match(LPAREN);
				setState(1039);
				expression(0);
				setState(1040);
				match(RPAREN);
				}
				break;
			case 21:
				{
				_localctx = new TryExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1042);
				match(TRY);
				setState(1043);
				expression(17);
				}
				break;
			case 22:
				{
				_localctx = new TupleLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1044);
				match(LPAREN);
				setState(1045);
				expression(0);
				setState(1046);
				match(COMMA);
				setState(1047);
				expression(0);
				setState(1052);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(1048);
					match(COMMA);
					setState(1049);
					expression(0);
					}
					}
					setState(1054);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(1055);
				match(RPAREN);
				}
				break;
			case 23:
				{
				_localctx = new ParenExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1057);
				match(LPAREN);
				setState(1058);
				expression(0);
				setState(1059);
				match(RPAREN);
				}
				break;
			case 24:
				{
				_localctx = new SpreadExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1061);
				match(SPREAD);
				setState(1062);
				expression(14);
				}
				break;
			case 25:
				{
				_localctx = new ListCompExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1063);
				match(LBRACKET);
				setState(1064);
				expression(0);
				setState(1065);
				match(PIPE);
				setState(1066);
				match(FOR);
				setState(1067);
				typeSpec(0);
				setState(1068);
				match(IDENTIFIER);
				setState(1069);
				match(IN);
				setState(1070);
				expression(0);
				setState(1073);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if (_la==IF) {
					{
					setState(1071);
					match(IF);
					setState(1072);
					expression(0);
					}
				}

				setState(1075);
				match(RBRACKET);
				}
				break;
			case 26:
				{
				_localctx = new ArrayLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1077);
				match(LBRACKET);
				setState(1086);
				_errHandler.sync(this);
				_la = _input.LA(1);
				if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
					{
					setState(1078);
					expression(0);
					setState(1083);
					_errHandler.sync(this);
					_la = _input.LA(1);
					while (_la==COMMA) {
						{
						{
						setState(1079);
						match(COMMA);
						setState(1080);
						expression(0);
						}
						}
						setState(1085);
						_errHandler.sync(this);
						_la = _input.LA(1);
					}
					}
				}

				setState(1088);
				match(RBRACKET);
				}
				break;
			case 27:
				{
				_localctx = new NullLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1089);
				match(NULL_LIT);
				}
				break;
			case 28:
				{
				_localctx = new IntLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1090);
				match(INT_LIT);
				}
				break;
			case 29:
				{
				_localctx = new HexLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1091);
				match(HEX_LIT);
				}
				break;
			case 30:
				{
				_localctx = new OctLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1092);
				match(OCT_LIT);
				}
				break;
			case 31:
				{
				_localctx = new BinLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1093);
				match(BIN_LIT);
				}
				break;
			case 32:
				{
				_localctx = new FloatLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1094);
				match(FLOAT_LIT);
				}
				break;
			case 33:
				{
				_localctx = new BoolLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1095);
				match(BOOL_LIT);
				}
				break;
			case 34:
				{
				_localctx = new CharLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1096);
				match(CHAR_LIT);
				}
				break;
			case 35:
				{
				_localctx = new StrLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1097);
				match(STR_LIT);
				}
				break;
			case 36:
				{
				_localctx = new CStrLitExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1098);
				match(C_STR_LIT);
				}
				break;
			case 37:
				{
				_localctx = new IdentExprContext(_localctx);
				_ctx = _localctx;
				_prevctx = _localctx;
				setState(1099);
				match(IDENTIFIER);
				}
				break;
			}
			_ctx.stop = _input.LT(-1);
			setState(1220);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,103,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					if ( _parseListeners!=null ) triggerExitRuleEvent();
					_prevctx = _localctx;
					{
					setState(1218);
					_errHandler.sync(this);
					switch ( getInterpreter().adaptivePredict(_input,102,_ctx) ) {
					case 1:
						{
						_localctx = new MulExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1102);
						if (!(precpred(_ctx, 33))) throw new FailedPredicateException(this, "precpred(_ctx, 33)");
						setState(1103);
						((MulExprContext)_localctx).op = _input.LT(1);
						_la = _input.LA(1);
						if ( !(((((_la - 107)) & ~0x3f) == 0 && ((1L << (_la - 107)) & 49L) != 0)) ) {
							((MulExprContext)_localctx).op = (Token)_errHandler.recoverInline(this);
						}
						else {
							if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
							_errHandler.reportMatch(this);
							consume();
						}
						setState(1104);
						expression(34);
						}
						break;
					case 2:
						{
						_localctx = new AddSubExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1105);
						if (!(precpred(_ctx, 32))) throw new FailedPredicateException(this, "precpred(_ctx, 32)");
						setState(1106);
						((AddSubExprContext)_localctx).op = _input.LT(1);
						_la = _input.LA(1);
						if ( !(_la==MINUS || _la==PLUS) ) {
							((AddSubExprContext)_localctx).op = (Token)_errHandler.recoverInline(this);
						}
						else {
							if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
							_errHandler.reportMatch(this);
							consume();
						}
						setState(1107);
						expression(33);
						}
						break;
					case 3:
						{
						_localctx = new LshiftExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1108);
						if (!(precpred(_ctx, 31))) throw new FailedPredicateException(this, "precpred(_ctx, 31)");
						setState(1109);
						match(LSHIFT);
						setState(1110);
						expression(32);
						}
						break;
					case 4:
						{
						_localctx = new RshiftExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1111);
						if (!(precpred(_ctx, 30))) throw new FailedPredicateException(this, "precpred(_ctx, 30)");
						setState(1112);
						match(GT);
						setState(1113);
						match(GT);
						setState(1114);
						expression(31);
						}
						break;
					case 5:
						{
						_localctx = new RelExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1115);
						if (!(precpred(_ctx, 29))) throw new FailedPredicateException(this, "precpred(_ctx, 29)");
						setState(1116);
						((RelExprContext)_localctx).op = _input.LT(1);
						_la = _input.LA(1);
						if ( !(((((_la - 115)) & ~0x3f) == 0 && ((1L << (_la - 115)) & 15L) != 0)) ) {
							((RelExprContext)_localctx).op = (Token)_errHandler.recoverInline(this);
						}
						else {
							if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
							_errHandler.reportMatch(this);
							consume();
						}
						setState(1117);
						expression(30);
						}
						break;
					case 6:
						{
						_localctx = new EqExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1118);
						if (!(precpred(_ctx, 28))) throw new FailedPredicateException(this, "precpred(_ctx, 28)");
						setState(1119);
						((EqExprContext)_localctx).op = _input.LT(1);
						_la = _input.LA(1);
						if ( !(_la==EQ || _la==NEQ) ) {
							((EqExprContext)_localctx).op = (Token)_errHandler.recoverInline(this);
						}
						else {
							if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
							_errHandler.reportMatch(this);
							consume();
						}
						setState(1120);
						expression(29);
						}
						break;
					case 7:
						{
						_localctx = new BitAndExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1121);
						if (!(precpred(_ctx, 27))) throw new FailedPredicateException(this, "precpred(_ctx, 27)");
						setState(1122);
						match(AMPERSAND);
						setState(1123);
						expression(28);
						}
						break;
					case 8:
						{
						_localctx = new BitXorExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1124);
						if (!(precpred(_ctx, 26))) throw new FailedPredicateException(this, "precpred(_ctx, 26)");
						setState(1125);
						match(CARET);
						setState(1126);
						expression(27);
						}
						break;
					case 9:
						{
						_localctx = new BitOrExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1127);
						if (!(precpred(_ctx, 25))) throw new FailedPredicateException(this, "precpred(_ctx, 25)");
						setState(1128);
						match(PIPE);
						setState(1129);
						expression(26);
						}
						break;
					case 10:
						{
						_localctx = new LogicalAndExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1130);
						if (!(precpred(_ctx, 24))) throw new FailedPredicateException(this, "precpred(_ctx, 24)");
						setState(1131);
						match(LAND);
						setState(1132);
						expression(25);
						}
						break;
					case 11:
						{
						_localctx = new LogicalOrExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1133);
						if (!(precpred(_ctx, 23))) throw new FailedPredicateException(this, "precpred(_ctx, 23)");
						setState(1134);
						match(LOR);
						setState(1135);
						expression(24);
						}
						break;
					case 12:
						{
						_localctx = new NullCoalExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1136);
						if (!(precpred(_ctx, 22))) throw new FailedPredicateException(this, "precpred(_ctx, 22)");
						setState(1137);
						match(NULLCOAL);
						setState(1138);
						expression(23);
						}
						break;
					case 13:
						{
						_localctx = new RangeExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1139);
						if (!(precpred(_ctx, 21))) throw new FailedPredicateException(this, "precpred(_ctx, 21)");
						setState(1140);
						match(RANGE);
						setState(1141);
						expression(22);
						}
						break;
					case 14:
						{
						_localctx = new RangeInclExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1142);
						if (!(precpred(_ctx, 20))) throw new FailedPredicateException(this, "precpred(_ctx, 20)");
						setState(1143);
						match(RANGE_INCL);
						setState(1144);
						expression(21);
						}
						break;
					case 15:
						{
						_localctx = new TernaryExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1145);
						if (!(precpred(_ctx, 19))) throw new FailedPredicateException(this, "precpred(_ctx, 19)");
						setState(1146);
						match(QUESTION);
						setState(1147);
						expression(0);
						setState(1148);
						match(COLON);
						setState(1149);
						expression(19);
						}
						break;
					case 16:
						{
						_localctx = new MethodCallExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1151);
						if (!(precpred(_ctx, 67))) throw new FailedPredicateException(this, "precpred(_ctx, 67)");
						setState(1152);
						match(DOT);
						setState(1153);
						match(IDENTIFIER);
						setState(1154);
						match(LPAREN);
						setState(1156);
						_errHandler.sync(this);
						_la = _input.LA(1);
						if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
							{
							setState(1155);
							argList();
							}
						}

						setState(1158);
						match(RPAREN);
						}
						break;
					case 17:
						{
						_localctx = new FnCallExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1159);
						if (!(precpred(_ctx, 66))) throw new FailedPredicateException(this, "precpred(_ctx, 66)");
						setState(1160);
						match(LPAREN);
						setState(1162);
						_errHandler.sync(this);
						_la = _input.LA(1);
						if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
							{
							setState(1161);
							argList();
							}
						}

						setState(1164);
						match(RPAREN);
						}
						break;
					case 18:
						{
						_localctx = new FieldAccessExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1165);
						if (!(precpred(_ctx, 63))) throw new FailedPredicateException(this, "precpred(_ctx, 63)");
						setState(1166);
						match(DOT);
						setState(1167);
						match(IDENTIFIER);
						}
						break;
					case 19:
						{
						_localctx = new TupleIndexExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1168);
						if (!(precpred(_ctx, 62))) throw new FailedPredicateException(this, "precpred(_ctx, 62)");
						setState(1169);
						match(DOT);
						setState(1170);
						match(INT_LIT);
						}
						break;
					case 20:
						{
						_localctx = new ChainedTupleIndexExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1171);
						if (!(precpred(_ctx, 61))) throw new FailedPredicateException(this, "precpred(_ctx, 61)");
						setState(1172);
						match(DOT);
						setState(1173);
						match(FLOAT_LIT);
						}
						break;
					case 21:
						{
						_localctx = new ArrowMethodCallExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1174);
						if (!(precpred(_ctx, 60))) throw new FailedPredicateException(this, "precpred(_ctx, 60)");
						setState(1175);
						match(ARROW);
						setState(1176);
						match(IDENTIFIER);
						setState(1177);
						match(LPAREN);
						setState(1179);
						_errHandler.sync(this);
						_la = _input.LA(1);
						if ((((_la) & ~0x3f) == 0 && ((1L << _la) & 140738763436160L) != 0) || ((((_la - 75)) & ~0x3f) == 0 && ((1L << (_la - 75)) & 4996212042236927L) != 0)) {
							{
							setState(1178);
							argList();
							}
						}

						setState(1181);
						match(RPAREN);
						}
						break;
					case 22:
						{
						_localctx = new ArrowAccessExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1182);
						if (!(precpred(_ctx, 59))) throw new FailedPredicateException(this, "precpred(_ctx, 59)");
						setState(1183);
						match(ARROW);
						setState(1184);
						match(IDENTIFIER);
						}
						break;
					case 23:
						{
						_localctx = new TupleArrowIndexExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1185);
						if (!(precpred(_ctx, 58))) throw new FailedPredicateException(this, "precpred(_ctx, 58)");
						setState(1186);
						match(ARROW);
						setState(1187);
						match(INT_LIT);
						}
						break;
					case 24:
						{
						_localctx = new ChainedTupleArrowIndexExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1188);
						if (!(precpred(_ctx, 57))) throw new FailedPredicateException(this, "precpred(_ctx, 57)");
						setState(1189);
						match(ARROW);
						setState(1190);
						match(FLOAT_LIT);
						}
						break;
					case 25:
						{
						_localctx = new IndexExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1191);
						if (!(precpred(_ctx, 56))) throw new FailedPredicateException(this, "precpred(_ctx, 56)");
						setState(1192);
						match(LBRACKET);
						setState(1193);
						expression(0);
						setState(1194);
						match(RBRACKET);
						}
						break;
					case 26:
						{
						_localctx = new CastExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1196);
						if (!(precpred(_ctx, 55))) throw new FailedPredicateException(this, "precpred(_ctx, 55)");
						setState(1197);
						match(AS);
						setState(1198);
						typeSpec(0);
						}
						break;
					case 27:
						{
						_localctx = new IsExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1199);
						if (!(precpred(_ctx, 54))) throw new FailedPredicateException(this, "precpred(_ctx, 54)");
						setState(1200);
						match(IS);
						setState(1201);
						typeSpec(0);
						setState(1209);
						_errHandler.sync(this);
						switch ( getInterpreter().adaptivePredict(_input,101,_ctx) ) {
						case 1:
							{
							setState(1202);
							match(SCOPE);
							setState(1203);
							match(IDENTIFIER);
							setState(1207);
							_errHandler.sync(this);
							switch ( getInterpreter().adaptivePredict(_input,100,_ctx) ) {
							case 1:
								{
								setState(1204);
								match(LPAREN);
								setState(1205);
								match(IDENTIFIER);
								setState(1206);
								match(RPAREN);
								}
								break;
							}
							}
							break;
						}
						}
						break;
					case 28:
						{
						_localctx = new PostIncrExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1211);
						if (!(precpred(_ctx, 53))) throw new FailedPredicateException(this, "precpred(_ctx, 53)");
						setState(1212);
						match(INCR);
						}
						break;
					case 29:
						{
						_localctx = new PostDecrExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1213);
						if (!(precpred(_ctx, 52))) throw new FailedPredicateException(this, "precpred(_ctx, 52)");
						setState(1214);
						match(DECR);
						}
						break;
					case 30:
						{
						_localctx = new CatchUnwrapExprContext(new ExpressionContext(_parentctx, _parentState));
						pushNewRecursionContext(_localctx, _startState, RULE_expression);
						setState(1215);
						if (!(precpred(_ctx, 18))) throw new FailedPredicateException(this, "precpred(_ctx, 18)");
						setState(1216);
						match(CATCH);
						setState(1217);
						block();
						}
						break;
					}
					} 
				}
				setState(1222);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,103,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			unrollRecursionContexts(_parentctx);
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class TypeSpecContext extends ParserRuleContext {
		public TerminalNode STAR() { return getToken(LuxParser.STAR, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public TerminalNode LBRACKET() { return getToken(LuxParser.LBRACKET, 0); }
		public TerminalNode INT_LIT() { return getToken(LuxParser.INT_LIT, 0); }
		public TerminalNode RBRACKET() { return getToken(LuxParser.RBRACKET, 0); }
		public FnTypeSpecContext fnTypeSpec() {
			return getRuleContext(FnTypeSpecContext.class,0);
		}
		public TerminalNode VEC() { return getToken(LuxParser.VEC, 0); }
		public TerminalNode LT() { return getToken(LuxParser.LT, 0); }
		public TerminalNode GT() { return getToken(LuxParser.GT, 0); }
		public TerminalNode MAP() { return getToken(LuxParser.MAP, 0); }
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public TerminalNode SET() { return getToken(LuxParser.SET, 0); }
		public TerminalNode TUPLE() { return getToken(LuxParser.TUPLE, 0); }
		public TerminalNode IDENTIFIER() { return getToken(LuxParser.IDENTIFIER, 0); }
		public PrimitiveTypeContext primitiveType() {
			return getRuleContext(PrimitiveTypeContext.class,0);
		}
		public TerminalNode AUTO() { return getToken(LuxParser.AUTO, 0); }
		public TypeSpecContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_typeSpec; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterTypeSpec(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitTypeSpec(this);
		}
	}

	public final TypeSpecContext typeSpec() throws RecognitionException {
		return typeSpec(0);
	}

	private TypeSpecContext typeSpec(int _p) throws RecognitionException {
		ParserRuleContext _parentctx = _ctx;
		int _parentState = getState();
		TypeSpecContext _localctx = new TypeSpecContext(_ctx, _parentState);
		TypeSpecContext _prevctx = _localctx;
		int _startState = 132;
		enterRecursionRule(_localctx, 132, RULE_typeSpec, _p);
		int _la;
		try {
			int _alt;
			enterOuterAlt(_localctx, 1);
			{
			setState(1277);
			_errHandler.sync(this);
			switch ( getInterpreter().adaptivePredict(_input,106,_ctx) ) {
			case 1:
				{
				setState(1224);
				match(STAR);
				setState(1225);
				typeSpec(13);
				}
				break;
			case 2:
				{
				setState(1226);
				match(LBRACKET);
				setState(1227);
				match(INT_LIT);
				setState(1228);
				match(RBRACKET);
				setState(1229);
				typeSpec(11);
				}
				break;
			case 3:
				{
				setState(1230);
				match(LBRACKET);
				setState(1231);
				match(RBRACKET);
				setState(1232);
				typeSpec(10);
				}
				break;
			case 4:
				{
				setState(1233);
				fnTypeSpec();
				}
				break;
			case 5:
				{
				setState(1234);
				match(VEC);
				setState(1235);
				match(LT);
				setState(1236);
				typeSpec(0);
				setState(1237);
				match(GT);
				}
				break;
			case 6:
				{
				setState(1239);
				match(MAP);
				setState(1240);
				match(LT);
				setState(1241);
				typeSpec(0);
				setState(1242);
				match(COMMA);
				setState(1243);
				typeSpec(0);
				setState(1244);
				match(GT);
				}
				break;
			case 7:
				{
				setState(1246);
				match(SET);
				setState(1247);
				match(LT);
				setState(1248);
				typeSpec(0);
				setState(1249);
				match(GT);
				}
				break;
			case 8:
				{
				setState(1251);
				match(TUPLE);
				setState(1252);
				match(LT);
				setState(1253);
				typeSpec(0);
				setState(1256); 
				_errHandler.sync(this);
				_la = _input.LA(1);
				do {
					{
					{
					setState(1254);
					match(COMMA);
					setState(1255);
					typeSpec(0);
					}
					}
					setState(1258); 
					_errHandler.sync(this);
					_la = _input.LA(1);
				} while ( _la==COMMA );
				setState(1260);
				match(GT);
				}
				break;
			case 9:
				{
				setState(1262);
				match(IDENTIFIER);
				setState(1263);
				match(LT);
				setState(1264);
				typeSpec(0);
				setState(1269);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(1265);
					match(COMMA);
					setState(1266);
					typeSpec(0);
					}
					}
					setState(1271);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				setState(1272);
				match(GT);
				}
				break;
			case 10:
				{
				setState(1274);
				primitiveType();
				}
				break;
			case 11:
				{
				setState(1275);
				match(AUTO);
				}
				break;
			case 12:
				{
				setState(1276);
				match(IDENTIFIER);
				}
				break;
			}
			_ctx.stop = _input.LT(-1);
			setState(1283);
			_errHandler.sync(this);
			_alt = getInterpreter().adaptivePredict(_input,107,_ctx);
			while ( _alt!=2 && _alt!=org.antlr.v4.runtime.atn.ATN.INVALID_ALT_NUMBER ) {
				if ( _alt==1 ) {
					if ( _parseListeners!=null ) triggerExitRuleEvent();
					_prevctx = _localctx;
					{
					{
					_localctx = new TypeSpecContext(_parentctx, _parentState);
					pushNewRecursionContext(_localctx, _startState, RULE_typeSpec);
					setState(1279);
					if (!(precpred(_ctx, 12))) throw new FailedPredicateException(this, "precpred(_ctx, 12)");
					setState(1280);
					match(STAR);
					}
					} 
				}
				setState(1285);
				_errHandler.sync(this);
				_alt = getInterpreter().adaptivePredict(_input,107,_ctx);
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			unrollRecursionContexts(_parentctx);
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class FnTypeSpecContext extends ParserRuleContext {
		public TerminalNode FN() { return getToken(LuxParser.FN, 0); }
		public TerminalNode LPAREN() { return getToken(LuxParser.LPAREN, 0); }
		public TerminalNode RPAREN() { return getToken(LuxParser.RPAREN, 0); }
		public TerminalNode ARROW() { return getToken(LuxParser.ARROW, 0); }
		public List<TypeSpecContext> typeSpec() {
			return getRuleContexts(TypeSpecContext.class);
		}
		public TypeSpecContext typeSpec(int i) {
			return getRuleContext(TypeSpecContext.class,i);
		}
		public List<TerminalNode> COMMA() { return getTokens(LuxParser.COMMA); }
		public TerminalNode COMMA(int i) {
			return getToken(LuxParser.COMMA, i);
		}
		public FnTypeSpecContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_fnTypeSpec; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterFnTypeSpec(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitFnTypeSpec(this);
		}
	}

	public final FnTypeSpecContext fnTypeSpec() throws RecognitionException {
		FnTypeSpecContext _localctx = new FnTypeSpecContext(_ctx, getState());
		enterRule(_localctx, 134, RULE_fnTypeSpec);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(1286);
			match(FN);
			setState(1287);
			match(LPAREN);
			setState(1296);
			_errHandler.sync(this);
			_la = _input.LA(1);
			if ((((_la) & ~0x3f) == 0 && ((1L << _la) & -1123769603063552L) != 0) || ((((_la - 64)) & ~0x3f) == 0 && ((1L << (_la - 64)) & 10995117328383L) != 0)) {
				{
				setState(1288);
				typeSpec(0);
				setState(1293);
				_errHandler.sync(this);
				_la = _input.LA(1);
				while (_la==COMMA) {
					{
					{
					setState(1289);
					match(COMMA);
					setState(1290);
					typeSpec(0);
					}
					}
					setState(1295);
					_errHandler.sync(this);
					_la = _input.LA(1);
				}
				}
			}

			setState(1298);
			match(RPAREN);
			setState(1299);
			match(ARROW);
			setState(1300);
			typeSpec(0);
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	@SuppressWarnings("CheckReturnValue")
	public static class PrimitiveTypeContext extends ParserRuleContext {
		public TerminalNode INT1() { return getToken(LuxParser.INT1, 0); }
		public TerminalNode INT8() { return getToken(LuxParser.INT8, 0); }
		public TerminalNode INT16() { return getToken(LuxParser.INT16, 0); }
		public TerminalNode INT32() { return getToken(LuxParser.INT32, 0); }
		public TerminalNode INT64() { return getToken(LuxParser.INT64, 0); }
		public TerminalNode INT128() { return getToken(LuxParser.INT128, 0); }
		public TerminalNode INTINF() { return getToken(LuxParser.INTINF, 0); }
		public TerminalNode ISIZE() { return getToken(LuxParser.ISIZE, 0); }
		public TerminalNode UINT1() { return getToken(LuxParser.UINT1, 0); }
		public TerminalNode UINT8() { return getToken(LuxParser.UINT8, 0); }
		public TerminalNode UINT16() { return getToken(LuxParser.UINT16, 0); }
		public TerminalNode UINT32() { return getToken(LuxParser.UINT32, 0); }
		public TerminalNode UINT64() { return getToken(LuxParser.UINT64, 0); }
		public TerminalNode UINT128() { return getToken(LuxParser.UINT128, 0); }
		public TerminalNode USIZE() { return getToken(LuxParser.USIZE, 0); }
		public TerminalNode FLOAT32() { return getToken(LuxParser.FLOAT32, 0); }
		public TerminalNode FLOAT64() { return getToken(LuxParser.FLOAT64, 0); }
		public TerminalNode FLOAT80() { return getToken(LuxParser.FLOAT80, 0); }
		public TerminalNode FLOAT128() { return getToken(LuxParser.FLOAT128, 0); }
		public TerminalNode DOUBLE() { return getToken(LuxParser.DOUBLE, 0); }
		public TerminalNode BOOL() { return getToken(LuxParser.BOOL, 0); }
		public TerminalNode CHAR() { return getToken(LuxParser.CHAR, 0); }
		public TerminalNode VOID() { return getToken(LuxParser.VOID, 0); }
		public TerminalNode STRING() { return getToken(LuxParser.STRING, 0); }
		public TerminalNode CSTRING() { return getToken(LuxParser.CSTRING, 0); }
		public PrimitiveTypeContext(ParserRuleContext parent, int invokingState) {
			super(parent, invokingState);
		}
		@Override public int getRuleIndex() { return RULE_primitiveType; }
		@Override
		public void enterRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).enterPrimitiveType(this);
		}
		@Override
		public void exitRule(ParseTreeListener listener) {
			if ( listener instanceof LuxParserListener ) ((LuxParserListener)listener).exitPrimitiveType(this);
		}
	}

	public final PrimitiveTypeContext primitiveType() throws RecognitionException {
		PrimitiveTypeContext _localctx = new PrimitiveTypeContext(_ctx, getState());
		enterRule(_localctx, 136, RULE_primitiveType);
		int _la;
		try {
			enterOuterAlt(_localctx, 1);
			{
			setState(1302);
			_la = _input.LA(1);
			if ( !(((((_la - 50)) & ~0x3f) == 0 && ((1L << (_la - 50)) & 33554431L) != 0)) ) {
			_errHandler.recoverInline(this);
			}
			else {
				if ( _input.LA(1)==Token.EOF ) matchedEOF = true;
				_errHandler.reportMatch(this);
				consume();
			}
			}
		}
		catch (RecognitionException re) {
			_localctx.exception = re;
			_errHandler.reportError(this, re);
			_errHandler.recover(this, re);
		}
		finally {
			exitRule();
		}
		return _localctx;
	}

	public boolean sempred(RuleContext _localctx, int ruleIndex, int predIndex) {
		switch (ruleIndex) {
		case 65:
			return expression_sempred((ExpressionContext)_localctx, predIndex);
		case 66:
			return typeSpec_sempred((TypeSpecContext)_localctx, predIndex);
		}
		return true;
	}
	private boolean expression_sempred(ExpressionContext _localctx, int predIndex) {
		switch (predIndex) {
		case 0:
			return precpred(_ctx, 33);
		case 1:
			return precpred(_ctx, 32);
		case 2:
			return precpred(_ctx, 31);
		case 3:
			return precpred(_ctx, 30);
		case 4:
			return precpred(_ctx, 29);
		case 5:
			return precpred(_ctx, 28);
		case 6:
			return precpred(_ctx, 27);
		case 7:
			return precpred(_ctx, 26);
		case 8:
			return precpred(_ctx, 25);
		case 9:
			return precpred(_ctx, 24);
		case 10:
			return precpred(_ctx, 23);
		case 11:
			return precpred(_ctx, 22);
		case 12:
			return precpred(_ctx, 21);
		case 13:
			return precpred(_ctx, 20);
		case 14:
			return precpred(_ctx, 19);
		case 15:
			return precpred(_ctx, 67);
		case 16:
			return precpred(_ctx, 66);
		case 17:
			return precpred(_ctx, 63);
		case 18:
			return precpred(_ctx, 62);
		case 19:
			return precpred(_ctx, 61);
		case 20:
			return precpred(_ctx, 60);
		case 21:
			return precpred(_ctx, 59);
		case 22:
			return precpred(_ctx, 58);
		case 23:
			return precpred(_ctx, 57);
		case 24:
			return precpred(_ctx, 56);
		case 25:
			return precpred(_ctx, 55);
		case 26:
			return precpred(_ctx, 54);
		case 27:
			return precpred(_ctx, 53);
		case 28:
			return precpred(_ctx, 52);
		case 29:
			return precpred(_ctx, 18);
		}
		return true;
	}
	private boolean typeSpec_sempred(TypeSpecContext _localctx, int predIndex) {
		switch (predIndex) {
		case 30:
			return precpred(_ctx, 12);
		}
		return true;
	}

	public static final String _serializedATN =
		"\u0004\u0001\u0083\u0519\u0002\u0000\u0007\u0000\u0002\u0001\u0007\u0001"+
		"\u0002\u0002\u0007\u0002\u0002\u0003\u0007\u0003\u0002\u0004\u0007\u0004"+
		"\u0002\u0005\u0007\u0005\u0002\u0006\u0007\u0006\u0002\u0007\u0007\u0007"+
		"\u0002\b\u0007\b\u0002\t\u0007\t\u0002\n\u0007\n\u0002\u000b\u0007\u000b"+
		"\u0002\f\u0007\f\u0002\r\u0007\r\u0002\u000e\u0007\u000e\u0002\u000f\u0007"+
		"\u000f\u0002\u0010\u0007\u0010\u0002\u0011\u0007\u0011\u0002\u0012\u0007"+
		"\u0012\u0002\u0013\u0007\u0013\u0002\u0014\u0007\u0014\u0002\u0015\u0007"+
		"\u0015\u0002\u0016\u0007\u0016\u0002\u0017\u0007\u0017\u0002\u0018\u0007"+
		"\u0018\u0002\u0019\u0007\u0019\u0002\u001a\u0007\u001a\u0002\u001b\u0007"+
		"\u001b\u0002\u001c\u0007\u001c\u0002\u001d\u0007\u001d\u0002\u001e\u0007"+
		"\u001e\u0002\u001f\u0007\u001f\u0002 \u0007 \u0002!\u0007!\u0002\"\u0007"+
		"\"\u0002#\u0007#\u0002$\u0007$\u0002%\u0007%\u0002&\u0007&\u0002\'\u0007"+
		"\'\u0002(\u0007(\u0002)\u0007)\u0002*\u0007*\u0002+\u0007+\u0002,\u0007"+
		",\u0002-\u0007-\u0002.\u0007.\u0002/\u0007/\u00020\u00070\u00021\u0007"+
		"1\u00022\u00072\u00023\u00073\u00024\u00074\u00025\u00075\u00026\u0007"+
		"6\u00027\u00077\u00028\u00078\u00029\u00079\u0002:\u0007:\u0002;\u0007"+
		";\u0002<\u0007<\u0002=\u0007=\u0002>\u0007>\u0002?\u0007?\u0002@\u0007"+
		"@\u0002A\u0007A\u0002B\u0007B\u0002C\u0007C\u0002D\u0007D\u0001\u0000"+
		"\u0003\u0000\u008c\b\u0000\u0001\u0000\u0005\u0000\u008f\b\u0000\n\u0000"+
		"\f\u0000\u0092\t\u0000\u0001\u0000\u0005\u0000\u0095\b\u0000\n\u0000\f"+
		"\u0000\u0098\t\u0000\u0001\u0000\u0001\u0000\u0001\u0001\u0001\u0001\u0003"+
		"\u0001\u009e\b\u0001\u0001\u0002\u0001\u0002\u0001\u0002\u0001\u0002\u0001"+
		"\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001"+
		"\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001\u0003\u0001"+
		"\u0003\u0005\u0003\u00b1\b\u0003\n\u0003\f\u0003\u00b4\t\u0003\u0001\u0003"+
		"\u0001\u0003\u0001\u0003\u0003\u0003\u00b9\b\u0003\u0001\u0004\u0001\u0004"+
		"\u0001\u0004\u0005\u0004\u00be\b\u0004\n\u0004\f\u0004\u00c1\t\u0004\u0001"+
		"\u0005\u0001\u0005\u0001\u0006\u0001\u0006\u0001\u0006\u0001\u0006\u0001"+
		"\u0006\u0001\u0006\u0001\u0006\u0003\u0006\u00cc\b\u0006\u0001\u0007\u0001"+
		"\u0007\u0001\u0007\u0001\u0007\u0001\u0007\u0001\u0007\u0001\b\u0001\b"+
		"\u0001\b\u0003\b\u00d7\b\b\u0001\b\u0001\b\u0001\b\u0001\b\u0005\b\u00dd"+
		"\b\b\n\b\f\b\u00e0\t\b\u0001\b\u0003\b\u00e3\b\b\u0001\b\u0001\b\u0001"+
		"\t\u0001\t\u0001\t\u0001\t\u0001\t\u0001\t\u0005\t\u00ed\b\t\n\t\f\t\u00f0"+
		"\t\t\u0001\t\u0001\t\u0001\t\u0001\t\u0001\t\u0001\t\u0001\t\u0005\t\u00f9"+
		"\b\t\n\t\f\t\u00fc\t\t\u0001\t\u0003\t\u00ff\b\t\u0001\t\u0001\t\u0003"+
		"\t\u0103\b\t\u0001\n\u0001\n\u0001\n\u0001\n\u0001\u000b\u0001\u000b\u0001"+
		"\u000b\u0003\u000b\u010c\b\u000b\u0001\u000b\u0001\u000b\u0005\u000b\u0110"+
		"\b\u000b\n\u000b\f\u000b\u0113\t\u000b\u0001\u000b\u0001\u000b\u0001\f"+
		"\u0001\f\u0001\f\u0001\f\u0001\r\u0001\r\u0001\r\u0003\r\u011e\b\r\u0001"+
		"\r\u0001\r\u0005\r\u0122\b\r\n\r\f\r\u0125\t\r\u0001\r\u0001\r\u0001\u000e"+
		"\u0001\u000e\u0001\u000e\u0001\u000e\u0001\u000f\u0001\u000f\u0001\u000f"+
		"\u0001\u000f\u0001\u000f\u0003\u000f\u0132\b\u000f\u0001\u000f\u0001\u000f"+
		"\u0003\u000f\u0136\b\u000f\u0001\u000f\u0001\u000f\u0001\u000f\u0001\u0010"+
		"\u0001\u0010\u0001\u0010\u0005\u0010\u013e\b\u0010\n\u0010\f\u0010\u0141"+
		"\t\u0010\u0001\u0011\u0001\u0011\u0003\u0011\u0145\b\u0011\u0001\u0012"+
		"\u0001\u0012\u0001\u0012\u0003\u0012\u014a\b\u0012\u0001\u0012\u0001\u0012"+
		"\u0003\u0012\u014e\b\u0012\u0001\u0012\u0001\u0012\u0001\u0012\u0001\u0013"+
		"\u0001\u0013\u0001\u0013\u0003\u0013\u0156\b\u0013\u0001\u0013\u0001\u0013"+
		"\u0005\u0013\u015a\b\u0013\n\u0013\f\u0013\u015d\t\u0013\u0001\u0013\u0001"+
		"\u0013\u0001\u0014\u0001\u0014\u0001\u0014\u0001\u0014\u0005\u0014\u0165"+
		"\b\u0014\n\u0014\f\u0014\u0168\t\u0014\u0001\u0014\u0001\u0014\u0001\u0015"+
		"\u0001\u0015\u0001\u0015\u0003\u0015\u016f\b\u0015\u0001\u0016\u0001\u0016"+
		"\u0001\u0016\u0001\u0016\u0001\u0016\u0001\u0016\u0001\u0016\u0005\u0016"+
		"\u0178\b\u0016\n\u0016\f\u0016\u017b\t\u0016\u0001\u0016\u0001\u0016\u0001"+
		"\u0016\u0001\u0016\u0001\u0016\u0001\u0016\u0001\u0016\u0003\u0016\u0184"+
		"\b\u0016\u0001\u0016\u0001\u0016\u0001\u0016\u0003\u0016\u0189\b\u0016"+
		"\u0001\u0017\u0001\u0017\u0001\u0017\u0005\u0017\u018e\b\u0017\n\u0017"+
		"\f\u0017\u0191\t\u0017\u0001\u0018\u0001\u0018\u0001\u0018\u0001\u0018"+
		"\u0001\u0018\u0001\u0018\u0001\u0018\u0003\u0018\u019a\b\u0018\u0001\u0019"+
		"\u0001\u0019\u0005\u0019\u019e\b\u0019\n\u0019\f\u0019\u01a1\t\u0019\u0001"+
		"\u0019\u0001\u0019\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001"+
		"\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001"+
		"\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001"+
		"\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001"+
		"\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0001\u001a\u0003"+
		"\u001a\u01c1\b\u001a\u0001\u001b\u0001\u001b\u0001\u001b\u0001\u001b\u0003"+
		"\u001b\u01c7\b\u001b\u0001\u001c\u0001\u001c\u0005\u001c\u01cb\b\u001c"+
		"\n\u001c\f\u001c\u01ce\t\u001c\u0001\u001c\u0001\u001c\u0001\u001d\u0001"+
		"\u001d\u0001\u001d\u0005\u001d\u01d5\b\u001d\n\u001d\f\u001d\u01d8\t\u001d"+
		"\u0001\u001d\u0001\u001d\u0001\u001e\u0001\u001e\u0001\u001e\u0003\u001e"+
		"\u01df\b\u001e\u0001\u001e\u0001\u001e\u0001\u001e\u0005\u001e\u01e4\b"+
		"\u001e\n\u001e\f\u001e\u01e7\t\u001e\u0001\u001e\u0001\u001e\u0001\u001f"+
		"\u0001\u001f\u0001\u001f\u0005\u001f\u01ee\b\u001f\n\u001f\f\u001f\u01f1"+
		"\t\u001f\u0001 \u0001 \u0001 \u0003 \u01f6\b \u0001 \u0001 \u0003 \u01fa"+
		"\b \u0001 \u0001 \u0001!\u0001!\u0001!\u0001\"\u0001\"\u0001\"\u0001\""+
		"\u0001\"\u0005\"\u0206\b\"\n\"\f\"\u0209\t\"\u0001\"\u0001\"\u0001\"\u0001"+
		"\"\u0001\"\u0001\"\u0001\"\u0001\"\u0001\"\u0001\"\u0001\"\u0001\"\u0001"+
		"\"\u0001\"\u0001\"\u0003\"\u021a\b\"\u0001#\u0001#\u0001#\u0001#\u0001"+
		"#\u0005#\u0221\b#\n#\f#\u0224\t#\u0001#\u0001#\u0001#\u0001#\u0001$\u0001"+
		"$\u0001$\u0001$\u0001$\u0001%\u0001%\u0001%\u0004%\u0232\b%\u000b%\f%"+
		"\u0233\u0001%\u0001%\u0001%\u0001%\u0001&\u0001&\u0001&\u0004&\u023d\b"+
		"&\u000b&\f&\u023e\u0001&\u0001&\u0001&\u0001&\u0001\'\u0001\'\u0001\'"+
		"\u0001\'\u0001\'\u0004\'\u024a\b\'\u000b\'\f\'\u024b\u0001\'\u0001\'\u0004"+
		"\'\u0250\b\'\u000b\'\f\'\u0251\u0001\'\u0001\'\u0001\'\u0001\'\u0001("+
		"\u0001(\u0001(\u0004(\u025b\b(\u000b(\f(\u025c\u0001(\u0001(\u0001(\u0001"+
		"(\u0004(\u0263\b(\u000b(\f(\u0264\u0001(\u0001(\u0001(\u0001(\u0001)\u0001"+
		")\u0001)\u0001)\u0001)\u0001)\u0001)\u0001)\u0001)\u0001)\u0001)\u0001"+
		")\u0001)\u0001)\u0003)\u0279\b)\u0001*\u0001*\u0001*\u0001*\u0001*\u0001"+
		"*\u0001*\u0001+\u0001+\u0001+\u0001+\u0001+\u0001+\u0001+\u0001,\u0001"+
		",\u0001,\u0003,\u028c\b,\u0001,\u0001,\u0001,\u0001-\u0001-\u0001-\u0005"+
		"-\u0294\b-\n-\f-\u0297\t-\u0001.\u0001.\u0003.\u029b\b.\u0001.\u0001."+
		"\u0001/\u0001/\u0001/\u0001/\u0001/\u0001/\u0005/\u02a5\b/\n/\f/\u02a8"+
		"\t/\u0001/\u0003/\u02ab\b/\u0001/\u0001/\u0001/\u0001/\u0005/\u02b1\b"+
		"/\n/\f/\u02b4\t/\u0001/\u0003/\u02b7\b/\u0003/\u02b9\b/\u00010\u00010"+
		"\u00010\u00010\u00010\u00010\u00010\u00010\u00010\u00010\u00010\u0001"+
		"0\u00030\u02c7\b0\u00011\u00011\u00011\u00012\u00012\u00032\u02ce\b2\u0001"+
		"3\u00013\u00013\u00013\u00013\u00013\u00013\u00013\u00013\u00013\u0001"+
		"3\u00013\u00013\u00013\u00013\u00013\u00013\u00013\u00033\u02e2\b3\u0001"+
		"4\u00014\u00014\u00015\u00015\u00015\u00016\u00016\u00016\u00017\u0001"+
		"7\u00017\u00017\u00017\u00017\u00017\u00017\u00017\u00017\u00037\u02f7"+
		"\b7\u00018\u00018\u00018\u00018\u00018\u00018\u00018\u00018\u00018\u0001"+
		"8\u00018\u00018\u00018\u00018\u00038\u0307\b8\u00019\u00019\u00019\u0001"+
		"9\u00019\u00019\u0001:\u0001:\u0001:\u0004:\u0312\b:\u000b:\f:\u0313\u0001"+
		":\u0003:\u0317\b:\u0001:\u0001:\u0001:\u0001:\u0003:\u031d\b:\u0001;\u0001"+
		";\u0001;\u0001;\u0001;\u0001;\u0001;\u0001<\u0001<\u0001<\u0001=\u0001"+
		"=\u0001=\u0001=\u0001>\u0001>\u0001>\u0001>\u0005>\u0331\b>\n>\f>\u0334"+
		"\t>\u0001>\u0003>\u0337\b>\u0001>\u0001>\u0001>\u0001>\u0001>\u0001>\u0001"+
		">\u0001>\u0005>\u0341\b>\n>\f>\u0344\t>\u0001>\u0003>\u0347\b>\u0001>"+
		"\u0001>\u0003>\u034b\b>\u0001?\u0001?\u0001?\u0001?\u0005?\u0351\b?\n"+
		"?\f?\u0354\t?\u0001?\u0001?\u0001@\u0001@\u0001@\u0001A\u0001A\u0001A"+
		"\u0001A\u0001A\u0001A\u0005A\u0361\bA\nA\fA\u0364\tA\u0001A\u0001A\u0001"+
		"A\u0003A\u0369\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0005"+
		"A\u0372\bA\nA\fA\u0375\tA\u0001A\u0001A\u0001A\u0001A\u0001A\u0003A\u037c"+
		"\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0005A\u0389\bA\nA\fA\u038c\tA\u0003A\u038e\bA\u0001A\u0001A"+
		"\u0001A\u0001A\u0001A\u0001A\u0005A\u0396\bA\nA\fA\u0399\tA\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0005A\u03a4\bA\nA"+
		"\fA\u03a7\tA\u0003A\u03a9\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0003A\u03b2\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0005"+
		"A\u03ba\bA\nA\fA\u03bd\tA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0005A\u03ce"+
		"\bA\nA\fA\u03d1\tA\u0003A\u03d3\bA\u0001A\u0001A\u0001A\u0001A\u0001A"+
		"\u0001A\u0005A\u03db\bA\nA\fA\u03de\tA\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0005A\u03eb\bA\nA\fA\u03ee"+
		"\tA\u0003A\u03f0\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0005A\u041b\bA\nA\fA\u041e\tA\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0003A\u0432\bA\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0005A\u043a\bA\nA\fA\u043d\tA\u0003A\u043f\bA"+
		"\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0003A\u044d\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0003A\u0485"+
		"\bA\u0001A\u0001A\u0001A\u0001A\u0003A\u048b\bA\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0003A\u049c\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0003"+
		"A\u04b8\bA\u0003A\u04ba\bA\u0001A\u0001A\u0001A\u0001A\u0001A\u0001A\u0001"+
		"A\u0005A\u04c3\bA\nA\fA\u04c6\tA\u0001B\u0001B\u0001B\u0001B\u0001B\u0001"+
		"B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001"+
		"B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001"+
		"B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0004B\u04e9\bB\u000b"+
		"B\fB\u04ea\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0001B\u0005B\u04f4"+
		"\bB\nB\fB\u04f7\tB\u0001B\u0001B\u0001B\u0001B\u0001B\u0003B\u04fe\bB"+
		"\u0001B\u0001B\u0005B\u0502\bB\nB\fB\u0505\tB\u0001C\u0001C\u0001C\u0001"+
		"C\u0001C\u0005C\u050c\bC\nC\fC\u050f\tC\u0003C\u0511\bC\u0001C\u0001C"+
		"\u0001C\u0001C\u0001D\u0001D\u0001D\u0000\u0002\u0082\u0084E\u0000\u0002"+
		"\u0004\u0006\b\n\f\u000e\u0010\u0012\u0014\u0016\u0018\u001a\u001c\u001e"+
		" \"$&(*,.02468:<>@BDFHJLNPRTVXZ\\^`bdfhjlnprtvxz|~\u0080\u0082\u0084\u0086"+
		"\u0088\u0000\u0007\u0001\u0000*+\u0001\u0000U^\u0002\u0000kkop\u0001\u0000"+
		"mn\u0001\u0000sv\u0001\u0000qr\u0001\u00002J\u05ab\u0000\u008b\u0001\u0000"+
		"\u0000\u0000\u0002\u009d\u0001\u0000\u0000\u0000\u0004\u009f\u0001\u0000"+
		"\u0000\u0000\u0006\u00b8\u0001\u0000\u0000\u0000\b\u00ba\u0001\u0000\u0000"+
		"\u0000\n\u00c2\u0001\u0000\u0000\u0000\f\u00cb\u0001\u0000\u0000\u0000"+
		"\u000e\u00cd\u0001\u0000\u0000\u0000\u0010\u00d3\u0001\u0000\u0000\u0000"+
		"\u0012\u0102\u0001\u0000\u0000\u0000\u0014\u0104\u0001\u0000\u0000\u0000"+
		"\u0016\u0108\u0001\u0000\u0000\u0000\u0018\u0116\u0001\u0000\u0000\u0000"+
		"\u001a\u011a\u0001\u0000\u0000\u0000\u001c\u0128\u0001\u0000\u0000\u0000"+
		"\u001e\u012c\u0001\u0000\u0000\u0000 \u013a\u0001\u0000\u0000\u0000\""+
		"\u0142\u0001\u0000\u0000\u0000$\u0146\u0001\u0000\u0000\u0000&\u0152\u0001"+
		"\u0000\u0000\u0000(\u0160\u0001\u0000\u0000\u0000*\u016b\u0001\u0000\u0000"+
		"\u0000,\u0188\u0001\u0000\u0000\u0000.\u018a\u0001\u0000\u0000\u00000"+
		"\u0199\u0001\u0000\u0000\u00002\u019b\u0001\u0000\u0000\u00004\u01c0\u0001"+
		"\u0000\u0000\u00006\u01c6\u0001\u0000\u0000\u00008\u01c8\u0001\u0000\u0000"+
		"\u0000:\u01d1\u0001\u0000\u0000\u0000<\u01db\u0001\u0000\u0000\u0000>"+
		"\u01ea\u0001\u0000\u0000\u0000@\u01f2\u0001\u0000\u0000\u0000B\u01fd\u0001"+
		"\u0000\u0000\u0000D\u0219\u0001\u0000\u0000\u0000F\u021b\u0001\u0000\u0000"+
		"\u0000H\u0229\u0001\u0000\u0000\u0000J\u022e\u0001\u0000\u0000\u0000L"+
		"\u0239\u0001\u0000\u0000\u0000N\u0244\u0001\u0000\u0000\u0000P\u0257\u0001"+
		"\u0000\u0000\u0000R\u0278\u0001\u0000\u0000\u0000T\u027a\u0001\u0000\u0000"+
		"\u0000V\u0281\u0001\u0000\u0000\u0000X\u0288\u0001\u0000\u0000\u0000Z"+
		"\u0290\u0001\u0000\u0000\u0000\\\u0298\u0001\u0000\u0000\u0000^\u02b8"+
		"\u0001\u0000\u0000\u0000`\u02c6\u0001\u0000\u0000\u0000b\u02c8\u0001\u0000"+
		"\u0000\u0000d\u02cd\u0001\u0000\u0000\u0000f\u02e1\u0001\u0000\u0000\u0000"+
		"h\u02e3\u0001\u0000\u0000\u0000j\u02e6\u0001\u0000\u0000\u0000l\u02e9"+
		"\u0001\u0000\u0000\u0000n\u02f6\u0001\u0000\u0000\u0000p\u0306\u0001\u0000"+
		"\u0000\u0000r\u0308\u0001\u0000\u0000\u0000t\u031c\u0001\u0000\u0000\u0000"+
		"v\u031e\u0001\u0000\u0000\u0000x\u0325\u0001\u0000\u0000\u0000z\u0328"+
		"\u0001\u0000\u0000\u0000|\u034a\u0001\u0000\u0000\u0000~\u034c\u0001\u0000"+
		"\u0000\u0000\u0080\u0357\u0001\u0000\u0000\u0000\u0082\u044c\u0001\u0000"+
		"\u0000\u0000\u0084\u04fd\u0001\u0000\u0000\u0000\u0086\u0506\u0001\u0000"+
		"\u0000\u0000\u0088\u0516\u0001\u0000\u0000\u0000\u008a\u008c\u0003\u0004"+
		"\u0002\u0000\u008b\u008a\u0001\u0000\u0000\u0000\u008b\u008c\u0001\u0000"+
		"\u0000\u0000\u008c\u0090\u0001\u0000\u0000\u0000\u008d\u008f\u0003\u0002"+
		"\u0001\u0000\u008e\u008d\u0001\u0000\u0000\u0000\u008f\u0092\u0001\u0000"+
		"\u0000\u0000\u0090\u008e\u0001\u0000\u0000\u0000\u0090\u0091\u0001\u0000"+
		"\u0000\u0000\u0091\u0096\u0001\u0000\u0000\u0000\u0092\u0090\u0001\u0000"+
		"\u0000\u0000\u0093\u0095\u0003\f\u0006\u0000\u0094\u0093\u0001\u0000\u0000"+
		"\u0000\u0095\u0098\u0001\u0000\u0000\u0000\u0096\u0094\u0001\u0000\u0000"+
		"\u0000\u0096\u0097\u0001\u0000\u0000\u0000\u0097\u0099\u0001\u0000\u0000"+
		"\u0000\u0098\u0096\u0001\u0000\u0000\u0000\u0099\u009a\u0005\u0000\u0000"+
		"\u0001\u009a\u0001\u0001\u0000\u0000\u0000\u009b\u009e\u0003\u0006\u0003"+
		"\u0000\u009c\u009e\u0003\n\u0005\u0000\u009d\u009b\u0001\u0000\u0000\u0000"+
		"\u009d\u009c\u0001\u0000\u0000\u0000\u009e\u0003\u0001\u0000\u0000\u0000"+
		"\u009f\u00a0\u0005\u0001\u0000\u0000\u00a0\u00a1\u0005T\u0000\u0000\u00a1"+
		"\u00a2\u0005_\u0000\u0000\u00a2\u0005\u0001\u0000\u0000\u0000\u00a3\u00a4"+
		"\u0005\u0002\u0000\u0000\u00a4\u00a5\u0003\b\u0004\u0000\u00a5\u00a6\u0005"+
		"a\u0000\u0000\u00a6\u00a7\u0005T\u0000\u0000\u00a7\u00a8\u0005_\u0000"+
		"\u0000\u00a8\u00b9\u0001\u0000\u0000\u0000\u00a9\u00aa\u0005\u0002\u0000"+
		"\u0000\u00aa\u00ab\u0003\b\u0004\u0000\u00ab\u00ac\u0005a\u0000\u0000"+
		"\u00ac\u00ad\u0005g\u0000\u0000\u00ad\u00b2\u0005T\u0000\u0000\u00ae\u00af"+
		"\u0005b\u0000\u0000\u00af\u00b1\u0005T\u0000\u0000\u00b0\u00ae\u0001\u0000"+
		"\u0000\u0000\u00b1\u00b4\u0001\u0000\u0000\u0000\u00b2\u00b0\u0001\u0000"+
		"\u0000\u0000\u00b2\u00b3\u0001\u0000\u0000\u0000\u00b3\u00b5\u0001\u0000"+
		"\u0000\u0000\u00b4\u00b2\u0001\u0000\u0000\u0000\u00b5\u00b6\u0005h\u0000"+
		"\u0000\u00b6\u00b7\u0005_\u0000\u0000\u00b7\u00b9\u0001\u0000\u0000\u0000"+
		"\u00b8\u00a3\u0001\u0000\u0000\u0000\u00b8\u00a9\u0001\u0000\u0000\u0000"+
		"\u00b9\u0007\u0001\u0000\u0000\u0000\u00ba\u00bf\u0005T\u0000\u0000\u00bb"+
		"\u00bc\u0005a\u0000\u0000\u00bc\u00be\u0005T\u0000\u0000\u00bd\u00bb\u0001"+
		"\u0000\u0000\u0000\u00be\u00c1\u0001\u0000\u0000\u0000\u00bf\u00bd\u0001"+
		"\u0000\u0000\u0000\u00bf\u00c0\u0001\u0000\u0000\u0000\u00c0\t\u0001\u0000"+
		"\u0000\u0000\u00c1\u00bf\u0001\u0000\u0000\u0000\u00c2\u00c3\u0007\u0000"+
		"\u0000\u0000\u00c3\u000b\u0001\u0000\u0000\u0000\u00c4\u00cc\u0003\u000e"+
		"\u0007\u0000\u00c5\u00cc\u0003\u0016\u000b\u0000\u00c6\u00cc\u0003\u001a"+
		"\r\u0000\u00c7\u00cc\u0003\u0010\b\u0000\u00c8\u00cc\u0003&\u0013\u0000"+
		"\u00c9\u00cc\u0003\u001e\u000f\u0000\u00ca\u00cc\u0003$\u0012\u0000\u00cb"+
		"\u00c4\u0001\u0000\u0000\u0000\u00cb\u00c5\u0001\u0000\u0000\u0000\u00cb"+
		"\u00c6\u0001\u0000\u0000\u0000\u00cb\u00c7\u0001\u0000\u0000\u0000\u00cb"+
		"\u00c8\u0001\u0000\u0000\u0000\u00cb\u00c9\u0001\u0000\u0000\u0000\u00cb"+
		"\u00ca\u0001\u0000\u0000\u0000\u00cc\r\u0001\u0000\u0000\u0000\u00cd\u00ce"+
		"\u0005\t\u0000\u0000\u00ce\u00cf\u0005T\u0000\u0000\u00cf\u00d0\u0005"+
		"d\u0000\u0000\u00d0\u00d1\u0003\u0084B\u0000\u00d1\u00d2\u0005_\u0000"+
		"\u0000\u00d2\u000f\u0001\u0000\u0000\u0000\u00d3\u00d4\u0005\u0006\u0000"+
		"\u0000\u00d4\u00d6\u0005T\u0000\u0000\u00d5\u00d7\u0003(\u0014\u0000\u00d6"+
		"\u00d5\u0001\u0000\u0000\u0000\u00d6\u00d7\u0001\u0000\u0000\u0000\u00d7"+
		"\u00d8\u0001\u0000\u0000\u0000\u00d8\u00d9\u0005g\u0000\u0000\u00d9\u00de"+
		"\u0003\u0012\t\u0000\u00da\u00db\u0005b\u0000\u0000\u00db\u00dd\u0003"+
		"\u0012\t\u0000\u00dc\u00da\u0001\u0000\u0000\u0000\u00dd\u00e0\u0001\u0000"+
		"\u0000\u0000\u00de\u00dc\u0001\u0000\u0000\u0000\u00de\u00df\u0001\u0000"+
		"\u0000\u0000\u00df\u00e2\u0001\u0000\u0000\u0000\u00e0\u00de\u0001\u0000"+
		"\u0000\u0000\u00e1\u00e3\u0005b\u0000\u0000\u00e2\u00e1\u0001\u0000\u0000"+
		"\u0000\u00e2\u00e3\u0001\u0000\u0000\u0000\u00e3\u00e4\u0001\u0000\u0000"+
		"\u0000\u00e4\u00e5\u0005h\u0000\u0000\u00e5\u0011\u0001\u0000\u0000\u0000"+
		"\u00e6\u0103\u0005T\u0000\u0000\u00e7\u00e8\u0005T\u0000\u0000\u00e8\u00e9"+
		"\u0005e\u0000\u0000\u00e9\u00ee\u0003\u0084B\u0000\u00ea\u00eb\u0005b"+
		"\u0000\u0000\u00eb\u00ed\u0003\u0084B\u0000\u00ec\u00ea\u0001\u0000\u0000"+
		"\u0000\u00ed\u00f0\u0001\u0000\u0000\u0000\u00ee\u00ec\u0001\u0000\u0000"+
		"\u0000\u00ee\u00ef\u0001\u0000\u0000\u0000\u00ef\u00f1\u0001\u0000\u0000"+
		"\u0000\u00f0\u00ee\u0001\u0000\u0000\u0000\u00f1\u00f2\u0005f\u0000\u0000"+
		"\u00f2\u0103\u0001\u0000\u0000\u0000\u00f3\u00f4\u0005T\u0000\u0000\u00f4"+
		"\u00f5\u0005g\u0000\u0000\u00f5\u00fa\u0003\u0014\n\u0000\u00f6\u00f7"+
		"\u0005b\u0000\u0000\u00f7\u00f9\u0003\u0014\n\u0000\u00f8\u00f6\u0001"+
		"\u0000\u0000\u0000\u00f9\u00fc\u0001\u0000\u0000\u0000\u00fa\u00f8\u0001"+
		"\u0000\u0000\u0000\u00fa\u00fb\u0001\u0000\u0000\u0000\u00fb\u00fe\u0001"+
		"\u0000\u0000\u0000\u00fc\u00fa\u0001\u0000\u0000\u0000\u00fd\u00ff\u0005"+
		"b\u0000\u0000\u00fe\u00fd\u0001\u0000\u0000\u0000\u00fe\u00ff\u0001\u0000"+
		"\u0000\u0000\u00ff\u0100\u0001\u0000\u0000\u0000\u0100\u0101\u0005h\u0000"+
		"\u0000\u0101\u0103\u0001\u0000\u0000\u0000\u0102\u00e6\u0001\u0000\u0000"+
		"\u0000\u0102\u00e7\u0001\u0000\u0000\u0000\u0102\u00f3\u0001\u0000\u0000"+
		"\u0000\u0103\u0013\u0001\u0000\u0000\u0000\u0104\u0105\u0005T\u0000\u0000"+
		"\u0105\u0106\u0005`\u0000\u0000\u0106\u0107\u0003\u0084B\u0000\u0107\u0015"+
		"\u0001\u0000\u0000\u0000\u0108\u0109\u0005\u0004\u0000\u0000\u0109\u010b"+
		"\u0005T\u0000\u0000\u010a\u010c\u0003(\u0014\u0000\u010b\u010a\u0001\u0000"+
		"\u0000\u0000\u010b\u010c\u0001\u0000\u0000\u0000\u010c\u010d\u0001\u0000"+
		"\u0000\u0000\u010d\u0111\u0005g\u0000\u0000\u010e\u0110\u0003\u0018\f"+
		"\u0000\u010f\u010e\u0001\u0000\u0000\u0000\u0110\u0113\u0001\u0000\u0000"+
		"\u0000\u0111\u010f\u0001\u0000\u0000\u0000\u0111\u0112\u0001\u0000\u0000"+
		"\u0000\u0112\u0114\u0001\u0000\u0000\u0000\u0113\u0111\u0001\u0000\u0000"+
		"\u0000\u0114\u0115\u0005h\u0000\u0000\u0115\u0017\u0001\u0000\u0000\u0000"+
		"\u0116\u0117\u0003\u0084B\u0000\u0117\u0118\u0005T\u0000\u0000\u0118\u0119"+
		"\u0005_\u0000\u0000\u0119\u0019\u0001\u0000\u0000\u0000\u011a\u011b\u0005"+
		"\u0005\u0000\u0000\u011b\u011d\u0005T\u0000\u0000\u011c\u011e\u0003(\u0014"+
		"\u0000\u011d\u011c\u0001\u0000\u0000\u0000\u011d\u011e\u0001\u0000\u0000"+
		"\u0000\u011e\u011f\u0001\u0000\u0000\u0000\u011f\u0123\u0005g\u0000\u0000"+
		"\u0120\u0122\u0003\u001c\u000e\u0000\u0121\u0120\u0001\u0000\u0000\u0000"+
		"\u0122\u0125\u0001\u0000\u0000\u0000\u0123\u0121\u0001\u0000\u0000\u0000"+
		"\u0123\u0124\u0001\u0000\u0000\u0000\u0124\u0126\u0001\u0000\u0000\u0000"+
		"\u0125\u0123\u0001\u0000\u0000\u0000\u0126\u0127\u0005h\u0000\u0000\u0127"+
		"\u001b\u0001\u0000\u0000\u0000\u0128\u0129\u0003\u0084B\u0000\u0129\u012a"+
		"\u0005T\u0000\u0000\u012a\u012b\u0005_\u0000\u0000\u012b\u001d\u0001\u0000"+
		"\u0000\u0000\u012c\u012d\u0005#\u0000\u0000\u012d\u012e\u0003\u0084B\u0000"+
		"\u012e\u012f\u0005T\u0000\u0000\u012f\u0131\u0005e\u0000\u0000\u0130\u0132"+
		"\u0003 \u0010\u0000\u0131\u0130\u0001\u0000\u0000\u0000\u0131\u0132\u0001"+
		"\u0000\u0000\u0000\u0132\u0135\u0001\u0000\u0000\u0000\u0133\u0134\u0005"+
		"b\u0000\u0000\u0134\u0136\u0005/\u0000\u0000\u0135\u0133\u0001\u0000\u0000"+
		"\u0000\u0135\u0136\u0001\u0000\u0000\u0000\u0136\u0137\u0001\u0000\u0000"+
		"\u0000\u0137\u0138\u0005f\u0000\u0000\u0138\u0139\u0005_\u0000\u0000\u0139"+
		"\u001f\u0001\u0000\u0000\u0000\u013a\u013f\u0003\"\u0011\u0000\u013b\u013c"+
		"\u0005b\u0000\u0000\u013c\u013e\u0003\"\u0011\u0000\u013d\u013b\u0001"+
		"\u0000\u0000\u0000\u013e\u0141\u0001\u0000\u0000\u0000\u013f\u013d\u0001"+
		"\u0000\u0000\u0000\u013f\u0140\u0001\u0000\u0000\u0000\u0140!\u0001\u0000"+
		"\u0000\u0000\u0141\u013f\u0001\u0000\u0000\u0000\u0142\u0144\u0003\u0084"+
		"B\u0000\u0143\u0145\u0005T\u0000\u0000\u0144\u0143\u0001\u0000\u0000\u0000"+
		"\u0144\u0145\u0001\u0000\u0000\u0000\u0145#\u0001\u0000\u0000\u0000\u0146"+
		"\u0147\u0003\u0084B\u0000\u0147\u0149\u0005T\u0000\u0000\u0148\u014a\u0003"+
		"(\u0014\u0000\u0149\u0148\u0001\u0000\u0000\u0000\u0149\u014a\u0001\u0000"+
		"\u0000\u0000\u014a\u014b\u0001\u0000\u0000\u0000\u014b\u014d\u0005e\u0000"+
		"\u0000\u014c\u014e\u0003.\u0017\u0000\u014d\u014c\u0001\u0000\u0000\u0000"+
		"\u014d\u014e\u0001\u0000\u0000\u0000\u014e\u014f\u0001\u0000\u0000\u0000"+
		"\u014f\u0150\u0005f\u0000\u0000\u0150\u0151\u00032\u0019\u0000\u0151%"+
		"\u0001\u0000\u0000\u0000\u0152\u0153\u0005\u001d\u0000\u0000\u0153\u0155"+
		"\u0005T\u0000\u0000\u0154\u0156\u0003(\u0014\u0000\u0155\u0154\u0001\u0000"+
		"\u0000\u0000\u0155\u0156\u0001\u0000\u0000\u0000\u0156\u0157\u0001\u0000"+
		"\u0000\u0000\u0157\u015b\u0005g\u0000\u0000\u0158\u015a\u0003,\u0016\u0000"+
		"\u0159\u0158\u0001\u0000\u0000\u0000\u015a\u015d\u0001\u0000\u0000\u0000"+
		"\u015b\u0159\u0001\u0000\u0000\u0000\u015b\u015c\u0001\u0000\u0000\u0000"+
		"\u015c\u015e\u0001\u0000\u0000\u0000\u015d\u015b\u0001\u0000\u0000\u0000"+
		"\u015e\u015f\u0005h\u0000\u0000\u015f\'\u0001\u0000\u0000\u0000\u0160"+
		"\u0161\u0005u\u0000\u0000\u0161\u0166\u0003*\u0015\u0000\u0162\u0163\u0005"+
		"b\u0000\u0000\u0163\u0165\u0003*\u0015\u0000\u0164\u0162\u0001\u0000\u0000"+
		"\u0000\u0165\u0168\u0001\u0000\u0000\u0000\u0166\u0164\u0001\u0000\u0000"+
		"\u0000\u0166\u0167\u0001\u0000\u0000\u0000\u0167\u0169\u0001\u0000\u0000"+
		"\u0000\u0168\u0166\u0001\u0000\u0000\u0000\u0169\u016a\u0005v\u0000\u0000"+
		"\u016a)\u0001\u0000\u0000\u0000\u016b\u016e\u0005T\u0000\u0000\u016c\u016d"+
		"\u0005`\u0000\u0000\u016d\u016f\u0005T\u0000\u0000\u016e\u016c\u0001\u0000"+
		"\u0000\u0000\u016e\u016f\u0001\u0000\u0000\u0000\u016f+\u0001\u0000\u0000"+
		"\u0000\u0170\u0171\u0003\u0084B\u0000\u0171\u0172\u0005T\u0000\u0000\u0172"+
		"\u0173\u0005e\u0000\u0000\u0173\u0174\u0005l\u0000\u0000\u0174\u0179\u0005"+
		"T\u0000\u0000\u0175\u0176\u0005b\u0000\u0000\u0176\u0178\u00030\u0018"+
		"\u0000\u0177\u0175\u0001\u0000\u0000\u0000\u0178\u017b\u0001\u0000\u0000"+
		"\u0000\u0179\u0177\u0001\u0000\u0000\u0000\u0179\u017a\u0001\u0000\u0000"+
		"\u0000\u017a\u017c\u0001\u0000\u0000\u0000\u017b\u0179\u0001\u0000\u0000"+
		"\u0000\u017c\u017d\u0005f\u0000\u0000\u017d\u017e\u00032\u0019\u0000\u017e"+
		"\u0189\u0001\u0000\u0000\u0000\u017f\u0180\u0003\u0084B\u0000\u0180\u0181"+
		"\u0005T\u0000\u0000\u0181\u0183\u0005e\u0000\u0000\u0182\u0184\u0003."+
		"\u0017\u0000\u0183\u0182\u0001\u0000\u0000\u0000\u0183\u0184\u0001\u0000"+
		"\u0000\u0000\u0184\u0185\u0001\u0000\u0000\u0000\u0185\u0186\u0005f\u0000"+
		"\u0000\u0186\u0187\u00032\u0019\u0000\u0187\u0189\u0001\u0000\u0000\u0000"+
		"\u0188\u0170\u0001\u0000\u0000\u0000\u0188\u017f\u0001\u0000\u0000\u0000"+
		"\u0189-\u0001\u0000\u0000\u0000\u018a\u018f\u00030\u0018\u0000\u018b\u018c"+
		"\u0005b\u0000\u0000\u018c\u018e\u00030\u0018\u0000\u018d\u018b\u0001\u0000"+
		"\u0000\u0000\u018e\u0191\u0001\u0000\u0000\u0000\u018f\u018d\u0001\u0000"+
		"\u0000\u0000\u018f\u0190\u0001\u0000\u0000\u0000\u0190/\u0001\u0000\u0000"+
		"\u0000\u0191\u018f\u0001\u0000\u0000\u0000\u0192\u0193\u0003\u0084B\u0000"+
		"\u0193\u0194\u0005/\u0000\u0000\u0194\u0195\u0005T\u0000\u0000\u0195\u019a"+
		"\u0001\u0000\u0000\u0000\u0196\u0197\u0003\u0084B\u0000\u0197\u0198\u0005"+
		"T\u0000\u0000\u0198\u019a\u0001\u0000\u0000\u0000\u0199\u0192\u0001\u0000"+
		"\u0000\u0000\u0199\u0196\u0001\u0000\u0000\u0000\u019a1\u0001\u0000\u0000"+
		"\u0000\u019b\u019f\u0005g\u0000\u0000\u019c\u019e\u00034\u001a\u0000\u019d"+
		"\u019c\u0001\u0000\u0000\u0000\u019e\u01a1\u0001\u0000\u0000\u0000\u019f"+
		"\u019d\u0001\u0000\u0000\u0000\u019f\u01a0\u0001\u0000\u0000\u0000\u01a0"+
		"\u01a2\u0001\u0000\u0000\u0000\u01a1\u019f\u0001\u0000\u0000\u0000\u01a2"+
		"\u01a3\u0005h\u0000\u0000\u01a33\u0001\u0000\u0000\u0000\u01a4\u01c1\u0003"+
		"D\"\u0000\u01a5\u01c1\u0003F#\u0000\u01a6\u01c1\u0003H$\u0000\u01a7\u01c1"+
		"\u0003R)\u0000\u01a8\u01c1\u0003P(\u0000\u01a9\u01c1\u0003N\'\u0000\u01aa"+
		"\u01c1\u0003J%\u0000\u01ab\u01c1\u0003L&\u0000\u01ac\u01c1\u0003T*\u0000"+
		"\u01ad\u01c1\u0003V+\u0000\u01ae\u01c1\u0003X,\u0000\u01af\u01c1\u0003"+
		"B!\u0000\u01b0\u01c1\u0003\\.\u0000\u01b1\u01c1\u0003^/\u0000\u01b2\u01c1"+
		"\u0003f3\u0000\u01b3\u01c1\u0003l6\u0000\u01b4\u01c1\u0003n7\u0000\u01b5"+
		"\u01c1\u0003p8\u0000\u01b6\u01c1\u0003h4\u0000\u01b7\u01c1\u0003j5\u0000"+
		"\u01b8\u01c1\u0003|>\u0000\u01b9\u01c1\u0003r9\u0000\u01ba\u01c1\u0003"+
		"t:\u0000\u01bb\u01c1\u0003z=\u0000\u01bc\u01c1\u00036\u001b\u0000\u01bd"+
		"\u01c1\u00038\u001c\u0000\u01be\u01c1\u0003:\u001d\u0000\u01bf\u01c1\u0003"+
		"<\u001e\u0000\u01c0\u01a4\u0001\u0000\u0000\u0000\u01c0\u01a5\u0001\u0000"+
		"\u0000\u0000\u01c0\u01a6\u0001\u0000\u0000\u0000\u01c0\u01a7\u0001\u0000"+
		"\u0000\u0000\u01c0\u01a8\u0001\u0000\u0000\u0000\u01c0\u01a9\u0001\u0000"+
		"\u0000\u0000\u01c0\u01aa\u0001\u0000\u0000\u0000\u01c0\u01ab\u0001\u0000"+
		"\u0000\u0000\u01c0\u01ac\u0001\u0000\u0000\u0000\u01c0\u01ad\u0001\u0000"+
		"\u0000\u0000\u01c0\u01ae\u0001\u0000\u0000\u0000\u01c0\u01af\u0001\u0000"+
		"\u0000\u0000\u01c0\u01b0\u0001\u0000\u0000\u0000\u01c0\u01b1\u0001\u0000"+
		"\u0000\u0000\u01c0\u01b2\u0001\u0000\u0000\u0000\u01c0\u01b3\u0001\u0000"+
		"\u0000\u0000\u01c0\u01b4\u0001\u0000\u0000\u0000\u01c0\u01b5\u0001\u0000"+
		"\u0000\u0000\u01c0\u01b6\u0001\u0000\u0000\u0000\u01c0\u01b7\u0001\u0000"+
		"\u0000\u0000\u01c0\u01b8\u0001\u0000\u0000\u0000\u01c0\u01b9\u0001\u0000"+
		"\u0000\u0000\u01c0\u01ba\u0001\u0000\u0000\u0000\u01c0\u01bb\u0001\u0000"+
		"\u0000\u0000\u01c0\u01bc\u0001\u0000\u0000\u0000\u01c0\u01bd\u0001\u0000"+
		"\u0000\u0000\u01c0\u01be\u0001\u0000\u0000\u0000\u01c0\u01bf\u0001\u0000"+
		"\u0000\u0000\u01c15\u0001\u0000\u0000\u0000\u01c2\u01c3\u0005\"\u0000"+
		"\u0000\u01c3\u01c7\u0003X,\u0000\u01c4\u01c5\u0005\"\u0000\u0000\u01c5"+
		"\u01c7\u0003B!\u0000\u01c6\u01c2\u0001\u0000\u0000\u0000\u01c6\u01c4\u0001"+
		"\u0000\u0000\u0000\u01c77\u0001\u0000\u0000\u0000\u01c8\u01cc\u0005g\u0000"+
		"\u0000\u01c9\u01cb\u00034\u001a\u0000\u01ca\u01c9\u0001\u0000\u0000\u0000"+
		"\u01cb\u01ce\u0001\u0000\u0000\u0000\u01cc\u01ca\u0001\u0000\u0000\u0000"+
		"\u01cc\u01cd\u0001\u0000\u0000\u0000\u01cd\u01cf\u0001\u0000\u0000\u0000"+
		"\u01ce\u01cc\u0001\u0000\u0000\u0000\u01cf\u01d0\u0005h\u0000\u0000\u01d0"+
		"9\u0001\u0000\u0000\u0000\u01d1\u01d2\u0005,\u0000\u0000\u01d2\u01d6\u0005"+
		"g\u0000\u0000\u01d3\u01d5\u00034\u001a\u0000\u01d4\u01d3\u0001\u0000\u0000"+
		"\u0000\u01d5\u01d8\u0001\u0000\u0000\u0000\u01d6\u01d4\u0001\u0000\u0000"+
		"\u0000\u01d6\u01d7\u0001\u0000\u0000\u0000\u01d7\u01d9\u0001\u0000\u0000"+
		"\u0000\u01d8\u01d6\u0001\u0000\u0000\u0000\u01d9\u01da\u0005h\u0000\u0000"+
		"\u01da;\u0001\u0000\u0000\u0000\u01db\u01dc\u0005-\u0000\u0000\u01dc\u01de"+
		"\u0005e\u0000\u0000\u01dd\u01df\u0003>\u001f\u0000\u01de\u01dd\u0001\u0000"+
		"\u0000\u0000\u01de\u01df\u0001\u0000\u0000\u0000\u01df\u01e0\u0001\u0000"+
		"\u0000\u0000\u01e0\u01e1\u0005f\u0000\u0000\u01e1\u01e5\u0005g\u0000\u0000"+
		"\u01e2\u01e4\u00034\u001a\u0000\u01e3\u01e2\u0001\u0000\u0000\u0000\u01e4"+
		"\u01e7\u0001\u0000\u0000\u0000\u01e5\u01e3\u0001\u0000\u0000\u0000\u01e5"+
		"\u01e6\u0001\u0000\u0000\u0000\u01e6\u01e8\u0001\u0000\u0000\u0000\u01e7"+
		"\u01e5\u0001\u0000\u0000\u0000\u01e8\u01e9\u0005h\u0000\u0000\u01e9=\u0001"+
		"\u0000\u0000\u0000\u01ea\u01ef\u0003@ \u0000\u01eb\u01ec\u0005b\u0000"+
		"\u0000\u01ec\u01ee\u0003@ \u0000\u01ed\u01eb\u0001\u0000\u0000\u0000\u01ee"+
		"\u01f1\u0001\u0000\u0000\u0000\u01ef\u01ed\u0001\u0000\u0000\u0000\u01ef"+
		"\u01f0\u0001\u0000\u0000\u0000\u01f0?\u0001\u0000\u0000\u0000\u01f1\u01ef"+
		"\u0001\u0000\u0000\u0000\u01f2\u01f5\u0005T\u0000\u0000\u01f3\u01f4\u0005"+
		"c\u0000\u0000\u01f4\u01f6\u0005T\u0000\u0000\u01f5\u01f3\u0001\u0000\u0000"+
		"\u0000\u01f5\u01f6\u0001\u0000\u0000\u0000\u01f6\u01f7\u0001\u0000\u0000"+
		"\u0000\u01f7\u01f9\u0005e\u0000\u0000\u01f8\u01fa\u0003Z-\u0000\u01f9"+
		"\u01f8\u0001\u0000\u0000\u0000\u01f9\u01fa\u0001\u0000\u0000\u0000\u01fa"+
		"\u01fb\u0001\u0000\u0000\u0000\u01fb\u01fc\u0005f\u0000\u0000\u01fcA\u0001"+
		"\u0000\u0000\u0000\u01fd\u01fe\u0003\u0082A\u0000\u01fe\u01ff\u0005_\u0000"+
		"\u0000\u01ffC\u0001\u0000\u0000\u0000\u0200\u0201\u0003\u0084B\u0000\u0201"+
		"\u0202\u0005e\u0000\u0000\u0202\u0207\u0005T\u0000\u0000\u0203\u0204\u0005"+
		"b\u0000\u0000\u0204\u0206\u0005T\u0000\u0000\u0205\u0203\u0001\u0000\u0000"+
		"\u0000\u0206\u0209\u0001\u0000\u0000\u0000\u0207\u0205\u0001\u0000\u0000"+
		"\u0000\u0207\u0208\u0001\u0000\u0000\u0000\u0208\u020a\u0001\u0000\u0000"+
		"\u0000\u0209\u0207\u0001\u0000\u0000\u0000\u020a\u020b\u0005f\u0000\u0000"+
		"\u020b\u020c\u0005d\u0000\u0000\u020c\u020d\u0003\u0082A\u0000\u020d\u020e"+
		"\u0005_\u0000\u0000\u020e\u021a\u0001\u0000\u0000\u0000\u020f\u0210\u0003"+
		"\u0084B\u0000\u0210\u0211\u0005T\u0000\u0000\u0211\u0212\u0005d\u0000"+
		"\u0000\u0212\u0213\u0003\u0082A\u0000\u0213\u0214\u0005_\u0000\u0000\u0214"+
		"\u021a\u0001\u0000\u0000\u0000\u0215\u0216\u0003\u0084B\u0000\u0216\u0217"+
		"\u0005T\u0000\u0000\u0217\u0218\u0005_\u0000\u0000\u0218\u021a\u0001\u0000"+
		"\u0000\u0000\u0219\u0200\u0001\u0000\u0000\u0000\u0219\u020f\u0001\u0000"+
		"\u0000\u0000\u0219\u0215\u0001\u0000\u0000\u0000\u021aE\u0001\u0000\u0000"+
		"\u0000\u021b\u0222\u0005T\u0000\u0000\u021c\u021d\u0005i\u0000\u0000\u021d"+
		"\u021e\u0003\u0082A\u0000\u021e\u021f\u0005j\u0000\u0000\u021f\u0221\u0001"+
		"\u0000\u0000\u0000\u0220\u021c\u0001\u0000\u0000\u0000\u0221\u0224\u0001"+
		"\u0000\u0000\u0000\u0222\u0220\u0001\u0000\u0000\u0000\u0222\u0223\u0001"+
		"\u0000\u0000\u0000\u0223\u0225\u0001\u0000\u0000\u0000\u0224\u0222\u0001"+
		"\u0000\u0000\u0000\u0225\u0226\u0005d\u0000\u0000\u0226\u0227\u0003\u0082"+
		"A\u0000\u0227\u0228\u0005_\u0000\u0000\u0228G\u0001\u0000\u0000\u0000"+
		"\u0229\u022a\u0005T\u0000\u0000\u022a\u022b\u0007\u0001\u0000\u0000\u022b"+
		"\u022c\u0003\u0082A\u0000\u022c\u022d\u0005_\u0000\u0000\u022dI\u0001"+
		"\u0000\u0000\u0000\u022e\u0231\u0005T\u0000\u0000\u022f\u0230\u0005c\u0000"+
		"\u0000\u0230\u0232\u0005T\u0000\u0000\u0231\u022f\u0001\u0000\u0000\u0000"+
		"\u0232\u0233\u0001\u0000\u0000\u0000\u0233\u0231\u0001\u0000\u0000\u0000"+
		"\u0233\u0234\u0001\u0000\u0000\u0000\u0234\u0235\u0001\u0000\u0000\u0000"+
		"\u0235\u0236\u0005d\u0000\u0000\u0236\u0237\u0003\u0082A\u0000\u0237\u0238"+
		"\u0005_\u0000\u0000\u0238K\u0001\u0000\u0000\u0000\u0239\u023c\u0005T"+
		"\u0000\u0000\u023a\u023b\u0005c\u0000\u0000\u023b\u023d\u0005T\u0000\u0000"+
		"\u023c\u023a\u0001\u0000\u0000\u0000\u023d\u023e\u0001\u0000\u0000\u0000"+
		"\u023e\u023c\u0001\u0000\u0000\u0000\u023e\u023f\u0001\u0000\u0000\u0000"+
		"\u023f\u0240\u0001\u0000\u0000\u0000\u0240\u0241\u0007\u0001\u0000\u0000"+
		"\u0241\u0242\u0003\u0082A\u0000\u0242\u0243\u0005_\u0000\u0000\u0243M"+
		"\u0001\u0000\u0000\u0000\u0244\u0249\u0005T\u0000\u0000\u0245\u0246\u0005"+
		"i\u0000\u0000\u0246\u0247\u0003\u0082A\u0000\u0247\u0248\u0005j\u0000"+
		"\u0000\u0248\u024a\u0001\u0000\u0000\u0000\u0249\u0245\u0001\u0000\u0000"+
		"\u0000\u024a\u024b\u0001\u0000\u0000\u0000\u024b\u0249\u0001\u0000\u0000"+
		"\u0000\u024b\u024c\u0001\u0000\u0000\u0000\u024c\u024f\u0001\u0000\u0000"+
		"\u0000\u024d\u024e\u0005c\u0000\u0000\u024e\u0250\u0005T\u0000\u0000\u024f"+
		"\u024d\u0001\u0000\u0000\u0000\u0250\u0251\u0001\u0000\u0000\u0000\u0251"+
		"\u024f\u0001\u0000\u0000\u0000\u0251\u0252\u0001\u0000\u0000\u0000\u0252"+
		"\u0253\u0001\u0000\u0000\u0000\u0253\u0254\u0005d\u0000\u0000\u0254\u0255"+
		"\u0003\u0082A\u0000\u0255\u0256\u0005_\u0000\u0000\u0256O\u0001\u0000"+
		"\u0000\u0000\u0257\u025a\u0005T\u0000\u0000\u0258\u0259\u0005c\u0000\u0000"+
		"\u0259\u025b\u0005T\u0000\u0000\u025a\u0258\u0001\u0000\u0000\u0000\u025b"+
		"\u025c\u0001\u0000\u0000\u0000\u025c\u025a\u0001\u0000\u0000\u0000\u025c"+
		"\u025d\u0001\u0000\u0000\u0000\u025d\u0262\u0001\u0000\u0000\u0000\u025e"+
		"\u025f\u0005i\u0000\u0000\u025f\u0260\u0003\u0082A\u0000\u0260\u0261\u0005"+
		"j\u0000\u0000\u0261\u0263\u0001\u0000\u0000\u0000\u0262\u025e\u0001\u0000"+
		"\u0000\u0000\u0263\u0264\u0001\u0000\u0000\u0000\u0264\u0262\u0001\u0000"+
		"\u0000\u0000\u0264\u0265\u0001\u0000\u0000\u0000\u0265\u0266\u0001\u0000"+
		"\u0000\u0000\u0266\u0267\u0005d\u0000\u0000\u0267\u0268\u0003\u0082A\u0000"+
		"\u0268\u0269\u0005_\u0000\u0000\u0269Q\u0001\u0000\u0000\u0000\u026a\u026b"+
		"\u0005k\u0000\u0000\u026b\u026c\u0005T\u0000\u0000\u026c\u026d\u0005d"+
		"\u0000\u0000\u026d\u026e\u0003\u0082A\u0000\u026e\u026f\u0005_\u0000\u0000"+
		"\u026f\u0279\u0001\u0000\u0000\u0000\u0270\u0271\u0005k\u0000\u0000\u0271"+
		"\u0272\u0005e\u0000\u0000\u0272\u0273\u0003\u0082A\u0000\u0273\u0274\u0005"+
		"f\u0000\u0000\u0274\u0275\u0005d\u0000\u0000\u0275\u0276\u0003\u0082A"+
		"\u0000\u0276\u0277\u0005_\u0000\u0000\u0277\u0279\u0001\u0000\u0000\u0000"+
		"\u0278\u026a\u0001\u0000\u0000\u0000\u0278\u0270\u0001\u0000\u0000\u0000"+
		"\u0279S\u0001\u0000\u0000\u0000\u027a\u027b\u0005T\u0000\u0000\u027b\u027c"+
		"\u0005)\u0000\u0000\u027c\u027d\u0005T\u0000\u0000\u027d\u027e\u0005d"+
		"\u0000\u0000\u027e\u027f\u0003\u0082A\u0000\u027f\u0280\u0005_\u0000\u0000"+
		"\u0280U\u0001\u0000\u0000\u0000\u0281\u0282\u0005T\u0000\u0000\u0282\u0283"+
		"\u0005)\u0000\u0000\u0283\u0284\u0005T\u0000\u0000\u0284\u0285\u0007\u0001"+
		"\u0000\u0000\u0285\u0286\u0003\u0082A\u0000\u0286\u0287\u0005_\u0000\u0000"+
		"\u0287W\u0001\u0000\u0000\u0000\u0288\u0289\u0005T\u0000\u0000\u0289\u028b"+
		"\u0005e\u0000\u0000\u028a\u028c\u0003Z-\u0000\u028b\u028a\u0001\u0000"+
		"\u0000\u0000\u028b\u028c\u0001\u0000\u0000\u0000\u028c\u028d\u0001\u0000"+
		"\u0000\u0000\u028d\u028e\u0005f\u0000\u0000\u028e\u028f\u0005_\u0000\u0000"+
		"\u028fY\u0001\u0000\u0000\u0000\u0290\u0295\u0003\u0082A\u0000\u0291\u0292"+
		"\u0005b\u0000\u0000\u0292\u0294\u0003\u0082A\u0000\u0293\u0291\u0001\u0000"+
		"\u0000\u0000\u0294\u0297\u0001\u0000\u0000\u0000\u0295\u0293\u0001\u0000"+
		"\u0000\u0000\u0295\u0296\u0001\u0000\u0000\u0000\u0296[\u0001\u0000\u0000"+
		"\u0000\u0297\u0295\u0001\u0000\u0000\u0000\u0298\u029a\u0005\u0003\u0000"+
		"\u0000\u0299\u029b\u0003\u0082A\u0000\u029a\u0299\u0001\u0000\u0000\u0000"+
		"\u029a\u029b\u0001\u0000\u0000\u0000\u029b\u029c\u0001\u0000\u0000\u0000"+
		"\u029c\u029d\u0005_\u0000\u0000\u029d]\u0001\u0000\u0000\u0000\u029e\u029f"+
		"\u0005\u000e\u0000\u0000\u029f\u02a0\u0005e\u0000\u0000\u02a0\u02a1\u0003"+
		"\u0082A\u0000\u02a1\u02a2\u0005f\u0000\u0000\u02a2\u02a6\u0003d2\u0000"+
		"\u02a3\u02a5\u0003`0\u0000\u02a4\u02a3\u0001\u0000\u0000\u0000\u02a5\u02a8"+
		"\u0001\u0000\u0000\u0000\u02a6\u02a4\u0001\u0000\u0000\u0000\u02a6\u02a7"+
		"\u0001\u0000\u0000\u0000\u02a7\u02aa\u0001\u0000\u0000\u0000\u02a8\u02a6"+
		"\u0001\u0000\u0000\u0000\u02a9\u02ab\u0003b1\u0000\u02aa\u02a9\u0001\u0000"+
		"\u0000\u0000\u02aa\u02ab\u0001\u0000\u0000\u0000\u02ab\u02b9\u0001\u0000"+
		"\u0000\u0000\u02ac\u02ad\u0005\u000e\u0000\u0000\u02ad\u02ae\u0003\u0082"+
		"A\u0000\u02ae\u02b2\u0003d2\u0000\u02af\u02b1\u0003`0\u0000\u02b0\u02af"+
		"\u0001\u0000\u0000\u0000\u02b1\u02b4\u0001\u0000\u0000\u0000\u02b2\u02b0"+
		"\u0001\u0000\u0000\u0000\u02b2\u02b3\u0001\u0000\u0000\u0000\u02b3\u02b6"+
		"\u0001\u0000\u0000\u0000\u02b4\u02b2\u0001\u0000\u0000\u0000\u02b5\u02b7"+
		"\u0003b1\u0000\u02b6\u02b5\u0001\u0000\u0000\u0000\u02b6\u02b7\u0001\u0000"+
		"\u0000\u0000\u02b7\u02b9\u0001\u0000\u0000\u0000\u02b8\u029e\u0001\u0000"+
		"\u0000\u0000\u02b8\u02ac\u0001\u0000\u0000\u0000\u02b9_\u0001\u0000\u0000"+
		"\u0000\u02ba\u02bb\u0005\u000f\u0000\u0000\u02bb\u02bc\u0005\u000e\u0000"+
		"\u0000\u02bc\u02bd\u0005e\u0000\u0000\u02bd\u02be\u0003\u0082A\u0000\u02be"+
		"\u02bf\u0005f\u0000\u0000\u02bf\u02c0\u0003d2\u0000\u02c0\u02c7\u0001"+
		"\u0000\u0000\u0000\u02c1\u02c2\u0005\u000f\u0000\u0000\u02c2\u02c3\u0005"+
		"\u000e\u0000\u0000\u02c3\u02c4\u0003\u0082A\u0000\u02c4\u02c5\u0003d2"+
		"\u0000\u02c5\u02c7\u0001\u0000\u0000\u0000\u02c6\u02ba\u0001\u0000\u0000"+
		"\u0000\u02c6\u02c1\u0001\u0000\u0000\u0000\u02c7a\u0001\u0000\u0000\u0000"+
		"\u02c8\u02c9\u0005\u000f\u0000\u0000\u02c9\u02ca\u0003d2\u0000\u02cac"+
		"\u0001\u0000\u0000\u0000\u02cb\u02ce\u00032\u0019\u0000\u02cc\u02ce\u0003"+
		"4\u001a\u0000\u02cd\u02cb\u0001\u0000\u0000\u0000\u02cd\u02cc\u0001\u0000"+
		"\u0000\u0000\u02cee\u0001\u0000\u0000\u0000\u02cf\u02d0\u0005\u0010\u0000"+
		"\u0000\u02d0\u02d1\u0003\u0084B\u0000\u02d1\u02d2\u0005T\u0000\u0000\u02d2"+
		"\u02d3\u0005\u0011\u0000\u0000\u02d3\u02d4\u0003\u0082A\u0000\u02d4\u02d5"+
		"\u00032\u0019\u0000\u02d5\u02e2\u0001\u0000\u0000\u0000\u02d6\u02d7\u0005"+
		"\u0010\u0000\u0000\u02d7\u02d8\u0003\u0084B\u0000\u02d8\u02d9\u0005T\u0000"+
		"\u0000\u02d9\u02da\u0005d\u0000\u0000\u02da\u02db\u0003\u0082A\u0000\u02db"+
		"\u02dc\u0005_\u0000\u0000\u02dc\u02dd\u0003\u0082A\u0000\u02dd\u02de\u0005"+
		"_\u0000\u0000\u02de\u02df\u0003\u0082A\u0000\u02df\u02e0\u00032\u0019"+
		"\u0000\u02e0\u02e2\u0001\u0000\u0000\u0000\u02e1\u02cf\u0001\u0000\u0000"+
		"\u0000\u02e1\u02d6\u0001\u0000\u0000\u0000\u02e2g\u0001\u0000\u0000\u0000"+
		"\u02e3\u02e4\u0005\u0015\u0000\u0000\u02e4\u02e5\u0005_\u0000\u0000\u02e5"+
		"i\u0001\u0000\u0000\u0000\u02e6\u02e7\u0005\u0016\u0000\u0000\u02e7\u02e8"+
		"\u0005_\u0000\u0000\u02e8k\u0001\u0000\u0000\u0000\u02e9\u02ea\u0005\u0012"+
		"\u0000\u0000\u02ea\u02eb\u00032\u0019\u0000\u02ebm\u0001\u0000\u0000\u0000"+
		"\u02ec\u02ed\u0005\u0013\u0000\u0000\u02ed\u02ee\u0003\u0082A\u0000\u02ee"+
		"\u02ef\u00032\u0019\u0000\u02ef\u02f7\u0001\u0000\u0000\u0000\u02f0\u02f1"+
		"\u0005\u0013\u0000\u0000\u02f1\u02f2\u0005e\u0000\u0000\u02f2\u02f3\u0003"+
		"\u0082A\u0000\u02f3\u02f4\u0005f\u0000\u0000\u02f4\u02f5\u00032\u0019"+
		"\u0000\u02f5\u02f7\u0001\u0000\u0000\u0000\u02f6\u02ec\u0001\u0000\u0000"+
		"\u0000\u02f6\u02f0\u0001\u0000\u0000\u0000\u02f7o\u0001\u0000\u0000\u0000"+
		"\u02f8\u02f9\u0005\u0014\u0000\u0000\u02f9\u02fa\u00032\u0019\u0000\u02fa"+
		"\u02fb\u0005\u0013\u0000\u0000\u02fb\u02fc\u0003\u0082A\u0000\u02fc\u02fd"+
		"\u0005_\u0000\u0000\u02fd\u0307\u0001\u0000\u0000\u0000\u02fe\u02ff\u0005"+
		"\u0014\u0000\u0000\u02ff\u0300\u00032\u0019\u0000\u0300\u0301\u0005\u0013"+
		"\u0000\u0000\u0301\u0302\u0005e\u0000\u0000\u0302\u0303\u0003\u0082A\u0000"+
		"\u0303\u0304\u0005f\u0000\u0000\u0304\u0305\u0005_\u0000\u0000\u0305\u0307"+
		"\u0001\u0000\u0000\u0000\u0306\u02f8\u0001\u0000\u0000\u0000\u0306\u02fe"+
		"\u0001\u0000\u0000\u0000\u0307q\u0001\u0000\u0000\u0000\u0308\u0309\u0005"+
		"\u001c\u0000\u0000\u0309\u030a\u0005e\u0000\u0000\u030a\u030b\u0003\u0082"+
		"A\u0000\u030b\u030c\u0005f\u0000\u0000\u030c\u030d\u00032\u0019\u0000"+
		"\u030ds\u0001\u0000\u0000\u0000\u030e\u030f\u0005\u001e\u0000\u0000\u030f"+
		"\u0311\u00032\u0019\u0000\u0310\u0312\u0003v;\u0000\u0311\u0310\u0001"+
		"\u0000\u0000\u0000\u0312\u0313\u0001\u0000\u0000\u0000\u0313\u0311\u0001"+
		"\u0000\u0000\u0000\u0313\u0314\u0001\u0000\u0000\u0000\u0314\u0316\u0001"+
		"\u0000\u0000\u0000\u0315\u0317\u0003x<\u0000\u0316\u0315\u0001\u0000\u0000"+
		"\u0000\u0316\u0317\u0001\u0000\u0000\u0000\u0317\u031d\u0001\u0000\u0000"+
		"\u0000\u0318\u0319\u0005\u001e\u0000\u0000\u0319\u031a\u00032\u0019\u0000"+
		"\u031a\u031b\u0003x<\u0000\u031b\u031d\u0001\u0000\u0000\u0000\u031c\u030e"+
		"\u0001\u0000\u0000\u0000\u031c\u0318\u0001\u0000\u0000\u0000\u031du\u0001"+
		"\u0000\u0000\u0000\u031e\u031f\u0005\u001f\u0000\u0000\u031f\u0320\u0005"+
		"e\u0000\u0000\u0320\u0321\u0003\u0084B\u0000\u0321\u0322\u0005T\u0000"+
		"\u0000\u0322\u0323\u0005f\u0000\u0000\u0323\u0324\u00032\u0019\u0000\u0324"+
		"w\u0001\u0000\u0000\u0000\u0325\u0326\u0005 \u0000\u0000\u0326\u0327\u0003"+
		"2\u0019\u0000\u0327y\u0001\u0000\u0000\u0000\u0328\u0329\u0005!\u0000"+
		"\u0000\u0329\u032a\u0003\u0082A\u0000\u032a\u032b\u0005_\u0000\u0000\u032b"+
		"{\u0001\u0000\u0000\u0000\u032c\u032d\u0005\u0017\u0000\u0000\u032d\u032e"+
		"\u0003\u0082A\u0000\u032e\u0332\u0005g\u0000\u0000\u032f\u0331\u0003~"+
		"?\u0000\u0330\u032f\u0001\u0000\u0000\u0000\u0331\u0334\u0001\u0000\u0000"+
		"\u0000\u0332\u0330\u0001\u0000\u0000\u0000\u0332\u0333\u0001\u0000\u0000"+
		"\u0000\u0333\u0336\u0001\u0000\u0000\u0000\u0334\u0332\u0001\u0000\u0000"+
		"\u0000\u0335\u0337\u0003\u0080@\u0000\u0336\u0335\u0001\u0000\u0000\u0000"+
		"\u0336\u0337\u0001\u0000\u0000\u0000\u0337\u0338\u0001\u0000\u0000\u0000"+
		"\u0338\u0339\u0005h\u0000\u0000\u0339\u034b\u0001\u0000\u0000\u0000\u033a"+
		"\u033b\u0005\u0017\u0000\u0000\u033b\u033c\u0005e\u0000\u0000\u033c\u033d"+
		"\u0003\u0082A\u0000\u033d\u033e\u0005f\u0000\u0000\u033e\u0342\u0005g"+
		"\u0000\u0000\u033f\u0341\u0003~?\u0000\u0340\u033f\u0001\u0000\u0000\u0000"+
		"\u0341\u0344\u0001\u0000\u0000\u0000\u0342\u0340\u0001\u0000\u0000\u0000"+
		"\u0342\u0343\u0001\u0000\u0000\u0000\u0343\u0346\u0001\u0000\u0000\u0000"+
		"\u0344\u0342\u0001\u0000\u0000\u0000\u0345\u0347\u0003\u0080@\u0000\u0346"+
		"\u0345\u0001\u0000\u0000\u0000\u0346\u0347\u0001\u0000\u0000\u0000\u0347"+
		"\u0348\u0001\u0000\u0000\u0000\u0348\u0349\u0005h\u0000\u0000\u0349\u034b"+
		"\u0001\u0000\u0000\u0000\u034a\u032c\u0001\u0000\u0000\u0000\u034a\u033a"+
		"\u0001\u0000\u0000\u0000\u034b}\u0001\u0000\u0000\u0000\u034c\u034d\u0005"+
		"\u0018\u0000\u0000\u034d\u0352\u0003\u0082A\u0000\u034e\u034f\u0005b\u0000"+
		"\u0000\u034f\u0351\u0003\u0082A\u0000\u0350\u034e\u0001\u0000\u0000\u0000"+
		"\u0351\u0354\u0001\u0000\u0000\u0000\u0352\u0350\u0001\u0000\u0000\u0000"+
		"\u0352\u0353\u0001\u0000\u0000\u0000\u0353\u0355\u0001\u0000\u0000\u0000"+
		"\u0354\u0352\u0001\u0000\u0000\u0000\u0355\u0356\u00032\u0019\u0000\u0356"+
		"\u007f\u0001\u0000\u0000\u0000\u0357\u0358\u0005\u0019\u0000\u0000\u0358"+
		"\u0359\u00032\u0019\u0000\u0359\u0081\u0001\u0000\u0000\u0000\u035a\u035b"+
		"\u0006A\uffff\uffff\u0000\u035b\u035c\u0005T\u0000\u0000\u035c\u035d\u0005"+
		"u\u0000\u0000\u035d\u0362\u0003\u0084B\u0000\u035e\u035f\u0005b\u0000"+
		"\u0000\u035f\u0361\u0003\u0084B\u0000\u0360\u035e\u0001\u0000\u0000\u0000"+
		"\u0361\u0364\u0001\u0000\u0000\u0000\u0362\u0360\u0001\u0000\u0000\u0000"+
		"\u0362\u0363\u0001\u0000\u0000\u0000\u0363\u0365\u0001\u0000\u0000\u0000"+
		"\u0364\u0362\u0001\u0000\u0000\u0000\u0365\u0366\u0005v\u0000\u0000\u0366"+
		"\u0368\u0005e\u0000\u0000\u0367\u0369\u0003Z-\u0000\u0368\u0367\u0001"+
		"\u0000\u0000\u0000\u0368\u0369\u0001\u0000\u0000\u0000\u0369\u036a\u0001"+
		"\u0000\u0000\u0000\u036a\u036b\u0005f\u0000\u0000\u036b\u044d\u0001\u0000"+
		"\u0000\u0000\u036c\u036d\u0005T\u0000\u0000\u036d\u036e\u0005u\u0000\u0000"+
		"\u036e\u0373\u0003\u0084B\u0000\u036f\u0370\u0005b\u0000\u0000\u0370\u0372"+
		"\u0003\u0084B\u0000\u0371\u036f\u0001\u0000\u0000\u0000\u0372\u0375\u0001"+
		"\u0000\u0000\u0000\u0373\u0371\u0001\u0000\u0000\u0000\u0373\u0374\u0001"+
		"\u0000\u0000\u0000\u0374\u0376\u0001\u0000\u0000\u0000\u0375\u0373\u0001"+
		"\u0000\u0000\u0000\u0376\u0377\u0005v\u0000\u0000\u0377\u0378\u0005a\u0000"+
		"\u0000\u0378\u0379\u0005T\u0000\u0000\u0379\u037b\u0005e\u0000\u0000\u037a"+
		"\u037c\u0003Z-\u0000\u037b\u037a\u0001\u0000\u0000\u0000\u037b\u037c\u0001"+
		"\u0000\u0000\u0000\u037c\u037d\u0001\u0000\u0000\u0000\u037d\u037e\u0005"+
		"f\u0000\u0000\u037e\u044d\u0001\u0000\u0000\u0000\u037f\u0380\u0005T\u0000"+
		"\u0000\u0380\u038d\u0005g\u0000\u0000\u0381\u0382\u0005T\u0000\u0000\u0382"+
		"\u0383\u0005`\u0000\u0000\u0383\u038a\u0003\u0082A\u0000\u0384\u0385\u0005"+
		"b\u0000\u0000\u0385\u0386\u0005T\u0000\u0000\u0386\u0387\u0005`\u0000"+
		"\u0000\u0387\u0389\u0003\u0082A\u0000\u0388\u0384\u0001\u0000\u0000\u0000"+
		"\u0389\u038c\u0001\u0000\u0000\u0000\u038a\u0388\u0001\u0000\u0000\u0000"+
		"\u038a\u038b\u0001\u0000\u0000\u0000\u038b\u038e\u0001\u0000\u0000\u0000"+
		"\u038c\u038a\u0001\u0000\u0000\u0000\u038d\u0381\u0001\u0000\u0000\u0000"+
		"\u038d\u038e\u0001\u0000\u0000\u0000\u038e\u038f\u0001\u0000\u0000\u0000"+
		"\u038f\u044d\u0005h\u0000\u0000\u0390\u0391\u0005T\u0000\u0000\u0391\u0392"+
		"\u0005u\u0000\u0000\u0392\u0397\u0003\u0084B\u0000\u0393\u0394\u0005b"+
		"\u0000\u0000\u0394\u0396\u0003\u0084B\u0000\u0395\u0393\u0001\u0000\u0000"+
		"\u0000\u0396\u0399\u0001\u0000\u0000\u0000\u0397\u0395\u0001\u0000\u0000"+
		"\u0000\u0397\u0398\u0001\u0000\u0000\u0000\u0398\u039a\u0001\u0000\u0000"+
		"\u0000\u0399\u0397\u0001\u0000\u0000\u0000\u039a\u039b\u0005v\u0000\u0000"+
		"\u039b\u03a8\u0005g\u0000\u0000\u039c\u039d\u0005T\u0000\u0000\u039d\u039e"+
		"\u0005`\u0000\u0000\u039e\u03a5\u0003\u0082A\u0000\u039f\u03a0\u0005b"+
		"\u0000\u0000\u03a0\u03a1\u0005T\u0000\u0000\u03a1\u03a2\u0005`\u0000\u0000"+
		"\u03a2\u03a4\u0003\u0082A\u0000\u03a3\u039f\u0001\u0000\u0000\u0000\u03a4"+
		"\u03a7\u0001\u0000\u0000\u0000\u03a5\u03a3\u0001\u0000\u0000\u0000\u03a5"+
		"\u03a6\u0001\u0000\u0000\u0000\u03a6\u03a9\u0001\u0000\u0000\u0000\u03a7"+
		"\u03a5\u0001\u0000\u0000\u0000\u03a8\u039c\u0001\u0000\u0000\u0000\u03a8"+
		"\u03a9\u0001\u0000\u0000\u0000\u03a9\u03aa\u0001\u0000\u0000\u0000\u03aa"+
		"\u03ab\u0005h\u0000\u0000\u03ab\u044d\u0001\u0000\u0000\u0000\u03ac\u03ad"+
		"\u0005T\u0000\u0000\u03ad\u03ae\u0005a\u0000\u0000\u03ae\u03af\u0005T"+
		"\u0000\u0000\u03af\u03b1\u0005e\u0000\u0000\u03b0\u03b2\u0003Z-\u0000"+
		"\u03b1\u03b0\u0001\u0000\u0000\u0000\u03b1\u03b2\u0001\u0000\u0000\u0000"+
		"\u03b2\u03b3\u0001\u0000\u0000\u0000\u03b3\u044d\u0005f\u0000\u0000\u03b4"+
		"\u03b5\u0005T\u0000\u0000\u03b5\u03b6\u0005u\u0000\u0000\u03b6\u03bb\u0003"+
		"\u0084B\u0000\u03b7\u03b8\u0005b\u0000\u0000\u03b8\u03ba\u0003\u0084B"+
		"\u0000\u03b9\u03b7\u0001\u0000\u0000\u0000\u03ba\u03bd\u0001\u0000\u0000"+
		"\u0000\u03bb\u03b9\u0001\u0000\u0000\u0000\u03bb\u03bc\u0001\u0000\u0000"+
		"\u0000\u03bc\u03be\u0001\u0000\u0000\u0000\u03bd\u03bb\u0001\u0000\u0000"+
		"\u0000\u03be\u03bf\u0005v\u0000\u0000\u03bf\u03c0\u0005a\u0000\u0000\u03c0"+
		"\u03c1\u0005T\u0000\u0000\u03c1\u044d\u0001\u0000\u0000\u0000\u03c2\u03c3"+
		"\u0005T\u0000\u0000\u03c3\u03c4\u0005a\u0000\u0000\u03c4\u03c5\u0005T"+
		"\u0000\u0000\u03c5\u03d2\u0005g\u0000\u0000\u03c6\u03c7\u0005T\u0000\u0000"+
		"\u03c7\u03c8\u0005`\u0000\u0000\u03c8\u03cf\u0003\u0082A\u0000\u03c9\u03ca"+
		"\u0005b\u0000\u0000\u03ca\u03cb\u0005T\u0000\u0000\u03cb\u03cc\u0005`"+
		"\u0000\u0000\u03cc\u03ce\u0003\u0082A\u0000\u03cd\u03c9\u0001\u0000\u0000"+
		"\u0000\u03ce\u03d1\u0001\u0000\u0000\u0000\u03cf\u03cd\u0001\u0000\u0000"+
		"\u0000\u03cf\u03d0\u0001\u0000\u0000\u0000\u03d0\u03d3\u0001\u0000\u0000"+
		"\u0000\u03d1\u03cf\u0001\u0000\u0000\u0000\u03d2\u03c6\u0001\u0000\u0000"+
		"\u0000\u03d2\u03d3\u0001\u0000\u0000\u0000\u03d3\u03d4\u0001\u0000\u0000"+
		"\u0000\u03d4\u044d\u0005h\u0000\u0000\u03d5\u03d6\u0005T\u0000\u0000\u03d6"+
		"\u03d7\u0005u\u0000\u0000\u03d7\u03dc\u0003\u0084B\u0000\u03d8\u03d9\u0005"+
		"b\u0000\u0000\u03d9\u03db\u0003\u0084B\u0000\u03da\u03d8\u0001\u0000\u0000"+
		"\u0000\u03db\u03de\u0001\u0000\u0000\u0000\u03dc\u03da\u0001\u0000\u0000"+
		"\u0000\u03dc\u03dd\u0001\u0000\u0000\u0000\u03dd\u03df\u0001\u0000\u0000"+
		"\u0000\u03de\u03dc\u0001\u0000\u0000\u0000\u03df\u03e0\u0005v\u0000\u0000"+
		"\u03e0\u03e1\u0005a\u0000\u0000\u03e1\u03e2\u0005T\u0000\u0000\u03e2\u03ef"+
		"\u0005g\u0000\u0000\u03e3\u03e4\u0005T\u0000\u0000\u03e4\u03e5\u0005`"+
		"\u0000\u0000\u03e5\u03ec\u0003\u0082A\u0000\u03e6\u03e7\u0005b\u0000\u0000"+
		"\u03e7\u03e8\u0005T\u0000\u0000\u03e8\u03e9\u0005`\u0000\u0000\u03e9\u03eb"+
		"\u0003\u0082A\u0000\u03ea\u03e6\u0001\u0000\u0000\u0000\u03eb\u03ee\u0001"+
		"\u0000\u0000\u0000\u03ec\u03ea\u0001\u0000\u0000\u0000\u03ec\u03ed\u0001"+
		"\u0000\u0000\u0000\u03ed\u03f0\u0001\u0000\u0000\u0000\u03ee\u03ec\u0001"+
		"\u0000\u0000\u0000\u03ef\u03e3\u0001\u0000\u0000\u0000\u03ef\u03f0\u0001"+
		"\u0000\u0000\u0000\u03f0\u03f1\u0001\u0000\u0000\u0000\u03f1\u03f2\u0005"+
		"h\u0000\u0000\u03f2\u044d\u0001\u0000\u0000\u0000\u03f3\u03f4\u0005T\u0000"+
		"\u0000\u03f4\u03f5\u0005a\u0000\u0000\u03f5\u044d\u0005T\u0000\u0000\u03f6"+
		"\u03f7\u0005k\u0000\u0000\u03f7\u044d\u0003\u0082A,\u03f8\u03f9\u0005"+
		"l\u0000\u0000\u03f9\u044d\u0003\u0082A+\u03fa\u03fb\u0005m\u0000\u0000"+
		"\u03fb\u044d\u0003\u0082A*\u03fc\u03fd\u0005\u001a\u0000\u0000\u03fd\u044d"+
		"\u0003\u0082A)\u03fe\u03ff\u0005\u001b\u0000\u0000\u03ff\u044d\u0003\u0082"+
		"A(\u0400\u0401\u0005y\u0000\u0000\u0401\u044d\u0003\u0082A\'\u0402\u0403"+
		"\u0005\u007f\u0000\u0000\u0403\u044d\u0003\u0082A&\u0404\u0405\u0005z"+
		"\u0000\u0000\u0405\u044d\u0003\u0082A%\u0406\u0407\u0005{\u0000\u0000"+
		"\u0407\u044d\u0003\u0082A$\u0408\u0409\u0005\f\u0000\u0000\u0409\u040a"+
		"\u0005e\u0000\u0000\u040a\u040b\u0003\u0084B\u0000\u040b\u040c\u0005f"+
		"\u0000\u0000\u040c\u044d\u0001\u0000\u0000\u0000\u040d\u040e\u0005\r\u0000"+
		"\u0000\u040e\u040f\u0005e\u0000\u0000\u040f\u0410\u0003\u0082A\u0000\u0410"+
		"\u0411\u0005f\u0000\u0000\u0411\u044d\u0001\u0000\u0000\u0000\u0412\u0413"+
		"\u0005\u001e\u0000\u0000\u0413\u044d\u0003\u0082A\u0011\u0414\u0415\u0005"+
		"e\u0000\u0000\u0415\u0416\u0003\u0082A\u0000\u0416\u0417\u0005b\u0000"+
		"\u0000\u0417\u041c\u0003\u0082A\u0000\u0418\u0419\u0005b\u0000\u0000\u0419"+
		"\u041b\u0003\u0082A\u0000\u041a\u0418\u0001\u0000\u0000\u0000\u041b\u041e"+
		"\u0001\u0000\u0000\u0000\u041c\u041a\u0001\u0000\u0000\u0000\u041c\u041d"+
		"\u0001\u0000\u0000\u0000\u041d\u041f\u0001\u0000\u0000\u0000\u041e\u041c"+
		"\u0001\u0000\u0000\u0000\u041f\u0420\u0005f\u0000\u0000\u0420\u044d\u0001"+
		"\u0000\u0000\u0000\u0421\u0422\u0005e\u0000\u0000\u0422\u0423\u0003\u0082"+
		"A\u0000\u0423\u0424\u0005f\u0000\u0000\u0424\u044d\u0001\u0000\u0000\u0000"+
		"\u0425\u0426\u0005/\u0000\u0000\u0426\u044d\u0003\u0082A\u000e\u0427\u0428"+
		"\u0005i\u0000\u0000\u0428\u0429\u0003\u0082A\u0000\u0429\u042a\u0005}"+
		"\u0000\u0000\u042a\u042b\u0005\u0010\u0000\u0000\u042b\u042c\u0003\u0084"+
		"B\u0000\u042c\u042d\u0005T\u0000\u0000\u042d\u042e\u0005\u0011\u0000\u0000"+
		"\u042e\u0431\u0003\u0082A\u0000\u042f\u0430\u0005\u000e\u0000\u0000\u0430"+
		"\u0432\u0003\u0082A\u0000\u0431\u042f\u0001\u0000\u0000\u0000\u0431\u0432"+
		"\u0001\u0000\u0000\u0000\u0432\u0433\u0001\u0000\u0000\u0000\u0433\u0434"+
		"\u0005j\u0000\u0000\u0434\u044d\u0001\u0000\u0000\u0000\u0435\u043e\u0005"+
		"i\u0000\u0000\u0436\u043b\u0003\u0082A\u0000\u0437\u0438\u0005b\u0000"+
		"\u0000\u0438\u043a\u0003\u0082A\u0000\u0439\u0437\u0001\u0000\u0000\u0000"+
		"\u043a\u043d\u0001\u0000\u0000\u0000\u043b\u0439\u0001\u0000\u0000\u0000"+
		"\u043b\u043c\u0001\u0000\u0000\u0000\u043c\u043f\u0001\u0000\u0000\u0000"+
		"\u043d\u043b\u0001\u0000\u0000\u0000\u043e\u0436\u0001\u0000\u0000\u0000"+
		"\u043e\u043f\u0001\u0000\u0000\u0000\u043f\u0440\u0001\u0000\u0000\u0000"+
		"\u0440\u044d\u0005j\u0000\u0000\u0441\u044d\u0005\u0007\u0000\u0000\u0442"+
		"\u044d\u0005N\u0000\u0000\u0443\u044d\u0005K\u0000\u0000\u0444\u044d\u0005"+
		"L\u0000\u0000\u0445\u044d\u0005M\u0000\u0000\u0446\u044d\u0005O\u0000"+
		"\u0000\u0447\u044d\u0005P\u0000\u0000\u0448\u044d\u0005S\u0000\u0000\u0449"+
		"\u044d\u0005R\u0000\u0000\u044a\u044d\u0005Q\u0000\u0000\u044b\u044d\u0005"+
		"T\u0000\u0000\u044c\u035a\u0001\u0000\u0000\u0000\u044c\u036c\u0001\u0000"+
		"\u0000\u0000\u044c\u037f\u0001\u0000\u0000\u0000\u044c\u0390\u0001\u0000"+
		"\u0000\u0000\u044c\u03ac\u0001\u0000\u0000\u0000\u044c\u03b4\u0001\u0000"+
		"\u0000\u0000\u044c\u03c2\u0001\u0000\u0000\u0000\u044c\u03d5\u0001\u0000"+
		"\u0000\u0000\u044c\u03f3\u0001\u0000\u0000\u0000\u044c\u03f6\u0001\u0000"+
		"\u0000\u0000\u044c\u03f8\u0001\u0000\u0000\u0000\u044c\u03fa\u0001\u0000"+
		"\u0000\u0000\u044c\u03fc\u0001\u0000\u0000\u0000\u044c\u03fe\u0001\u0000"+
		"\u0000\u0000\u044c\u0400\u0001\u0000\u0000\u0000\u044c\u0402\u0001\u0000"+
		"\u0000\u0000\u044c\u0404\u0001\u0000\u0000\u0000\u044c\u0406\u0001\u0000"+
		"\u0000\u0000\u044c\u0408\u0001\u0000\u0000\u0000\u044c\u040d\u0001\u0000"+
		"\u0000\u0000\u044c\u0412\u0001\u0000\u0000\u0000\u044c\u0414\u0001\u0000"+
		"\u0000\u0000\u044c\u0421\u0001\u0000\u0000\u0000\u044c\u0425\u0001\u0000"+
		"\u0000\u0000\u044c\u0427\u0001\u0000\u0000\u0000\u044c\u0435\u0001\u0000"+
		"\u0000\u0000\u044c\u0441\u0001\u0000\u0000\u0000\u044c\u0442\u0001\u0000"+
		"\u0000\u0000\u044c\u0443\u0001\u0000\u0000\u0000\u044c\u0444\u0001\u0000"+
		"\u0000\u0000\u044c\u0445\u0001\u0000\u0000\u0000\u044c\u0446\u0001\u0000"+
		"\u0000\u0000\u044c\u0447\u0001\u0000\u0000\u0000\u044c\u0448\u0001\u0000"+
		"\u0000\u0000\u044c\u0449\u0001\u0000\u0000\u0000\u044c\u044a\u0001\u0000"+
		"\u0000\u0000\u044c\u044b\u0001\u0000\u0000\u0000\u044d\u04c4\u0001\u0000"+
		"\u0000\u0000\u044e\u044f\n!\u0000\u0000\u044f\u0450\u0007\u0002\u0000"+
		"\u0000\u0450\u04c3\u0003\u0082A\"\u0451\u0452\n \u0000\u0000\u0452\u0453"+
		"\u0007\u0003\u0000\u0000\u0453\u04c3\u0003\u0082A!\u0454\u0455\n\u001f"+
		"\u0000\u0000\u0455\u0456\u0005|\u0000\u0000\u0456\u04c3\u0003\u0082A "+
		"\u0457\u0458\n\u001e\u0000\u0000\u0458\u0459\u0005v\u0000\u0000\u0459"+
		"\u045a\u0005v\u0000\u0000\u045a\u04c3\u0003\u0082A\u001f\u045b\u045c\n"+
		"\u001d\u0000\u0000\u045c\u045d\u0007\u0004\u0000\u0000\u045d\u04c3\u0003"+
		"\u0082A\u001e\u045e\u045f\n\u001c\u0000\u0000\u045f\u0460\u0007\u0005"+
		"\u0000\u0000\u0460\u04c3\u0003\u0082A\u001d\u0461\u0462\n\u001b\u0000"+
		"\u0000\u0462\u0463\u0005l\u0000\u0000\u0463\u04c3\u0003\u0082A\u001c\u0464"+
		"\u0465\n\u001a\u0000\u0000\u0465\u0466\u0005~\u0000\u0000\u0466\u04c3"+
		"\u0003\u0082A\u001b\u0467\u0468\n\u0019\u0000\u0000\u0468\u0469\u0005"+
		"}\u0000\u0000\u0469\u04c3\u0003\u0082A\u001a\u046a\u046b\n\u0018\u0000"+
		"\u0000\u046b\u046c\u0005w\u0000\u0000\u046c\u04c3\u0003\u0082A\u0019\u046d"+
		"\u046e\n\u0017\u0000\u0000\u046e\u046f\u0005x\u0000\u0000\u046f\u04c3"+
		"\u0003\u0082A\u0018\u0470\u0471\n\u0016\u0000\u0000\u0471\u0472\u0005"+
		".\u0000\u0000\u0472\u04c3\u0003\u0082A\u0017\u0473\u0474\n\u0015\u0000"+
		"\u0000\u0474\u0475\u00051\u0000\u0000\u0475\u04c3\u0003\u0082A\u0016\u0476"+
		"\u0477\n\u0014\u0000\u0000\u0477\u0478\u00050\u0000\u0000\u0478\u04c3"+
		"\u0003\u0082A\u0015\u0479\u047a\n\u0013\u0000\u0000\u047a\u047b\u0005"+
		"\u0080\u0000\u0000\u047b\u047c\u0003\u0082A\u0000\u047c\u047d\u0005`\u0000"+
		"\u0000\u047d\u047e\u0003\u0082A\u0013\u047e\u04c3\u0001\u0000\u0000\u0000"+
		"\u047f\u0480\nC\u0000\u0000\u0480\u0481\u0005c\u0000\u0000\u0481\u0482"+
		"\u0005T\u0000\u0000\u0482\u0484\u0005e\u0000\u0000\u0483\u0485\u0003Z"+
		"-\u0000\u0484\u0483\u0001\u0000\u0000\u0000\u0484\u0485\u0001\u0000\u0000"+
		"\u0000\u0485\u0486\u0001\u0000\u0000\u0000\u0486\u04c3\u0005f\u0000\u0000"+
		"\u0487\u0488\nB\u0000\u0000\u0488\u048a\u0005e\u0000\u0000\u0489\u048b"+
		"\u0003Z-\u0000\u048a\u0489\u0001\u0000\u0000\u0000\u048a\u048b\u0001\u0000"+
		"\u0000\u0000\u048b\u048c\u0001\u0000\u0000\u0000\u048c\u04c3\u0005f\u0000"+
		"\u0000\u048d\u048e\n?\u0000\u0000\u048e\u048f\u0005c\u0000\u0000\u048f"+
		"\u04c3\u0005T\u0000\u0000\u0490\u0491\n>\u0000\u0000\u0491\u0492\u0005"+
		"c\u0000\u0000\u0492\u04c3\u0005N\u0000\u0000\u0493\u0494\n=\u0000\u0000"+
		"\u0494\u0495\u0005c\u0000\u0000\u0495\u04c3\u0005O\u0000\u0000\u0496\u0497"+
		"\n<\u0000\u0000\u0497\u0498\u0005)\u0000\u0000\u0498\u0499\u0005T\u0000"+
		"\u0000\u0499\u049b\u0005e\u0000\u0000\u049a\u049c\u0003Z-\u0000\u049b"+
		"\u049a\u0001\u0000\u0000\u0000\u049b\u049c\u0001\u0000\u0000\u0000\u049c"+
		"\u049d\u0001\u0000\u0000\u0000\u049d\u04c3\u0005f\u0000\u0000\u049e\u049f"+
		"\n;\u0000\u0000\u049f\u04a0\u0005)\u0000\u0000\u04a0\u04c3\u0005T\u0000"+
		"\u0000\u04a1\u04a2\n:\u0000\u0000\u04a2\u04a3\u0005)\u0000\u0000\u04a3"+
		"\u04c3\u0005N\u0000\u0000\u04a4\u04a5\n9\u0000\u0000\u04a5\u04a6\u0005"+
		")\u0000\u0000\u04a6\u04c3\u0005O\u0000\u0000\u04a7\u04a8\n8\u0000\u0000"+
		"\u04a8\u04a9\u0005i\u0000\u0000\u04a9\u04aa\u0003\u0082A\u0000\u04aa\u04ab"+
		"\u0005j\u0000\u0000\u04ab\u04c3\u0001\u0000\u0000\u0000\u04ac\u04ad\n"+
		"7\u0000\u0000\u04ad\u04ae\u0005\n\u0000\u0000\u04ae\u04c3\u0003\u0084"+
		"B\u0000\u04af\u04b0\n6\u0000\u0000\u04b0\u04b1\u0005\u000b\u0000\u0000"+
		"\u04b1\u04b9\u0003\u0084B\u0000\u04b2\u04b3\u0005a\u0000\u0000\u04b3\u04b7"+
		"\u0005T\u0000\u0000\u04b4\u04b5\u0005e\u0000\u0000\u04b5\u04b6\u0005T"+
		"\u0000\u0000\u04b6\u04b8\u0005f\u0000\u0000\u04b7\u04b4\u0001\u0000\u0000"+
		"\u0000\u04b7\u04b8\u0001\u0000\u0000\u0000\u04b8\u04ba\u0001\u0000\u0000"+
		"\u0000\u04b9\u04b2\u0001\u0000\u0000\u0000\u04b9\u04ba\u0001\u0000\u0000"+
		"\u0000\u04ba\u04c3\u0001\u0000\u0000\u0000\u04bb\u04bc\n5\u0000\u0000"+
		"\u04bc\u04c3\u0005z\u0000\u0000\u04bd\u04be\n4\u0000\u0000\u04be\u04c3"+
		"\u0005{\u0000\u0000\u04bf\u04c0\n\u0012\u0000\u0000\u04c0\u04c1\u0005"+
		"\u001f\u0000\u0000\u04c1\u04c3\u00032\u0019\u0000\u04c2\u044e\u0001\u0000"+
		"\u0000\u0000\u04c2\u0451\u0001\u0000\u0000\u0000\u04c2\u0454\u0001\u0000"+
		"\u0000\u0000\u04c2\u0457\u0001\u0000\u0000\u0000\u04c2\u045b\u0001\u0000"+
		"\u0000\u0000\u04c2\u045e\u0001\u0000\u0000\u0000\u04c2\u0461\u0001\u0000"+
		"\u0000\u0000\u04c2\u0464\u0001\u0000\u0000\u0000\u04c2\u0467\u0001\u0000"+
		"\u0000\u0000\u04c2\u046a\u0001\u0000\u0000\u0000\u04c2\u046d\u0001\u0000"+
		"\u0000\u0000\u04c2\u0470\u0001\u0000\u0000\u0000\u04c2\u0473\u0001\u0000"+
		"\u0000\u0000\u04c2\u0476\u0001\u0000\u0000\u0000\u04c2\u0479\u0001\u0000"+
		"\u0000\u0000\u04c2\u047f\u0001\u0000\u0000\u0000\u04c2\u0487\u0001\u0000"+
		"\u0000\u0000\u04c2\u048d\u0001\u0000\u0000\u0000\u04c2\u0490\u0001\u0000"+
		"\u0000\u0000\u04c2\u0493\u0001\u0000\u0000\u0000\u04c2\u0496\u0001\u0000"+
		"\u0000\u0000\u04c2\u049e\u0001\u0000\u0000\u0000\u04c2\u04a1\u0001\u0000"+
		"\u0000\u0000\u04c2\u04a4\u0001\u0000\u0000\u0000\u04c2\u04a7\u0001\u0000"+
		"\u0000\u0000\u04c2\u04ac\u0001\u0000\u0000\u0000\u04c2\u04af\u0001\u0000"+
		"\u0000\u0000\u04c2\u04bb\u0001\u0000\u0000\u0000\u04c2\u04bd\u0001\u0000"+
		"\u0000\u0000\u04c2\u04bf\u0001\u0000\u0000\u0000\u04c3\u04c6\u0001\u0000"+
		"\u0000\u0000\u04c4\u04c2\u0001\u0000\u0000\u0000\u04c4\u04c5\u0001\u0000"+
		"\u0000\u0000\u04c5\u0083\u0001\u0000\u0000\u0000\u04c6\u04c4\u0001\u0000"+
		"\u0000\u0000\u04c7\u04c8\u0006B\uffff\uffff\u0000\u04c8\u04c9\u0005k\u0000"+
		"\u0000\u04c9\u04fe\u0003\u0084B\r\u04ca\u04cb\u0005i\u0000\u0000\u04cb"+
		"\u04cc\u0005N\u0000\u0000\u04cc\u04cd\u0005j\u0000\u0000\u04cd\u04fe\u0003"+
		"\u0084B\u000b\u04ce\u04cf\u0005i\u0000\u0000\u04cf\u04d0\u0005j\u0000"+
		"\u0000\u04d0\u04fe\u0003\u0084B\n\u04d1\u04fe\u0003\u0086C\u0000\u04d2"+
		"\u04d3\u0005%\u0000\u0000\u04d3\u04d4\u0005u\u0000\u0000\u04d4\u04d5\u0003"+
		"\u0084B\u0000\u04d5\u04d6\u0005v\u0000\u0000\u04d6\u04fe\u0001\u0000\u0000"+
		"\u0000\u04d7\u04d8\u0005&\u0000\u0000\u04d8\u04d9\u0005u\u0000\u0000\u04d9"+
		"\u04da\u0003\u0084B\u0000\u04da\u04db\u0005b\u0000\u0000\u04db\u04dc\u0003"+
		"\u0084B\u0000\u04dc\u04dd\u0005v\u0000\u0000\u04dd\u04fe\u0001\u0000\u0000"+
		"\u0000\u04de\u04df\u0005\'\u0000\u0000\u04df\u04e0\u0005u\u0000\u0000"+
		"\u04e0\u04e1\u0003\u0084B\u0000\u04e1\u04e2\u0005v\u0000\u0000\u04e2\u04fe"+
		"\u0001\u0000\u0000\u0000\u04e3\u04e4\u0005(\u0000\u0000\u04e4\u04e5\u0005"+
		"u\u0000\u0000\u04e5\u04e8\u0003\u0084B\u0000\u04e6\u04e7\u0005b\u0000"+
		"\u0000\u04e7\u04e9\u0003\u0084B\u0000\u04e8\u04e6\u0001\u0000\u0000\u0000"+
		"\u04e9\u04ea\u0001\u0000\u0000\u0000\u04ea\u04e8\u0001\u0000\u0000\u0000"+
		"\u04ea\u04eb\u0001\u0000\u0000\u0000\u04eb\u04ec\u0001\u0000\u0000\u0000"+
		"\u04ec\u04ed\u0005v\u0000\u0000\u04ed\u04fe\u0001\u0000\u0000\u0000\u04ee"+
		"\u04ef\u0005T\u0000\u0000\u04ef\u04f0\u0005u\u0000\u0000\u04f0\u04f5\u0003"+
		"\u0084B\u0000\u04f1\u04f2\u0005b\u0000\u0000\u04f2\u04f4\u0003\u0084B"+
		"\u0000\u04f3\u04f1\u0001\u0000\u0000\u0000\u04f4\u04f7\u0001\u0000\u0000"+
		"\u0000\u04f5\u04f3\u0001\u0000\u0000\u0000\u04f5\u04f6\u0001\u0000\u0000"+
		"\u0000\u04f6\u04f8\u0001\u0000\u0000\u0000\u04f7\u04f5\u0001\u0000\u0000"+
		"\u0000\u04f8\u04f9\u0005v\u0000\u0000\u04f9\u04fe\u0001\u0000\u0000\u0000"+
		"\u04fa\u04fe\u0003\u0088D\u0000\u04fb\u04fe\u0005$\u0000\u0000\u04fc\u04fe"+
		"\u0005T\u0000\u0000\u04fd\u04c7\u0001\u0000\u0000\u0000\u04fd\u04ca\u0001"+
		"\u0000\u0000\u0000\u04fd\u04ce\u0001\u0000\u0000\u0000\u04fd\u04d1\u0001"+
		"\u0000\u0000\u0000\u04fd\u04d2\u0001\u0000\u0000\u0000\u04fd\u04d7\u0001"+
		"\u0000\u0000\u0000\u04fd\u04de\u0001\u0000\u0000\u0000\u04fd\u04e3\u0001"+
		"\u0000\u0000\u0000\u04fd\u04ee\u0001\u0000\u0000\u0000\u04fd\u04fa\u0001"+
		"\u0000\u0000\u0000\u04fd\u04fb\u0001\u0000\u0000\u0000\u04fd\u04fc\u0001"+
		"\u0000\u0000\u0000\u04fe\u0503\u0001\u0000\u0000\u0000\u04ff\u0500\n\f"+
		"\u0000\u0000\u0500\u0502\u0005k\u0000\u0000\u0501\u04ff\u0001\u0000\u0000"+
		"\u0000\u0502\u0505\u0001\u0000\u0000\u0000\u0503\u0501\u0001\u0000\u0000"+
		"\u0000\u0503\u0504\u0001\u0000\u0000\u0000\u0504\u0085\u0001\u0000\u0000"+
		"\u0000\u0505\u0503\u0001\u0000\u0000\u0000\u0506\u0507\u0005\b\u0000\u0000"+
		"\u0507\u0510\u0005e\u0000\u0000\u0508\u050d\u0003\u0084B\u0000\u0509\u050a"+
		"\u0005b\u0000\u0000\u050a\u050c\u0003\u0084B\u0000\u050b\u0509\u0001\u0000"+
		"\u0000\u0000\u050c\u050f\u0001\u0000\u0000\u0000\u050d\u050b\u0001\u0000"+
		"\u0000\u0000\u050d\u050e\u0001\u0000\u0000\u0000\u050e\u0511\u0001\u0000"+
		"\u0000\u0000\u050f\u050d\u0001\u0000\u0000\u0000\u0510\u0508\u0001\u0000"+
		"\u0000\u0000\u0510\u0511\u0001\u0000\u0000\u0000\u0511\u0512\u0001\u0000"+
		"\u0000\u0000\u0512\u0513\u0005f\u0000\u0000\u0513\u0514\u0005)\u0000\u0000"+
		"\u0514\u0515\u0003\u0084B\u0000\u0515\u0087\u0001\u0000\u0000\u0000\u0516"+
		"\u0517\u0007\u0006\u0000\u0000\u0517\u0089\u0001\u0000\u0000\u0000n\u008b"+
		"\u0090\u0096\u009d\u00b2\u00b8\u00bf\u00cb\u00d6\u00de\u00e2\u00ee\u00fa"+
		"\u00fe\u0102\u010b\u0111\u011d\u0123\u0131\u0135\u013f\u0144\u0149\u014d"+
		"\u0155\u015b\u0166\u016e\u0179\u0183\u0188\u018f\u0199\u019f\u01c0\u01c6"+
		"\u01cc\u01d6\u01de\u01e5\u01ef\u01f5\u01f9\u0207\u0219\u0222\u0233\u023e"+
		"\u024b\u0251\u025c\u0264\u0278\u028b\u0295\u029a\u02a6\u02aa\u02b2\u02b6"+
		"\u02b8\u02c6\u02cd\u02e1\u02f6\u0306\u0313\u0316\u031c\u0332\u0336\u0342"+
		"\u0346\u034a\u0352\u0362\u0368\u0373\u037b\u038a\u038d\u0397\u03a5\u03a8"+
		"\u03b1\u03bb\u03cf\u03d2\u03dc\u03ec\u03ef\u041c\u0431\u043b\u043e\u044c"+
		"\u0484\u048a\u049b\u04b7\u04b9\u04c2\u04c4\u04ea\u04f5\u04fd\u0503\u050d"+
		"\u0510";
	public static final ATN _ATN =
		new ATNDeserializer().deserialize(_serializedATN.toCharArray());
	static {
		_decisionToDFA = new DFA[_ATN.getNumberOfDecisions()];
		for (int i = 0; i < _ATN.getNumberOfDecisions(); i++) {
			_decisionToDFA[i] = new DFA(_ATN.getDecisionState(i), i);
		}
	}
}