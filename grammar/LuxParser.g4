parser grammar LuxParser;

options {
    tokenVocab=LuxLexer;
}

// Entry point
program
    : namespaceDecl? preambleDecl* topLevelDecl* EOF
    ;

// use and #include can appear in any order
preambleDecl
    : useDecl
    | includeDecl
    ;

// namespace Main;
namespaceDecl
    : NAMESPACE IDENTIFIER SEMI
    ;

// use std::log::println;
// use std::log::{ println, print };
useDecl
    : USE modulePath SCOPE IDENTIFIER SEMI                                         # useItem
    | USE modulePath SCOPE LBRACE IDENTIFIER (COMMA IDENTIFIER)* RBRACE SEMI      # useGroup
    ;

// Module path: std::log, std::io, etc.
modulePath
    : IDENTIFIER (SCOPE IDENTIFIER)*
    ;

// #include <stdio.h>
// #include "mylib.h"
includeDecl
    : INCLUDE_SYS
    | INCLUDE_LOCAL
    ;

// Top-level declarations
topLevelDecl
    : typeAliasDecl
    | structDecl
    | unionDecl
    | enumDecl
    | extendDecl
    | externDecl
    | functionDecl
    ;

// type BinOp = fn(int32, int32) -> int32;
typeAliasDecl
    : TYPE IDENTIFIER ASSIGN typeSpec SEMI
    ;

// enum Color { Red, Green, Blue }
// enum Result<T, E> { Ok(T), Err(E) }
enumDecl
    : ENUM IDENTIFIER typeParamList? LBRACE enumVariant (COMMA enumVariant)* COMMA? RBRACE
    ;

enumVariant
    : IDENTIFIER
    | IDENTIFIER LPAREN typeSpec (COMMA typeSpec)* RPAREN
    | IDENTIFIER LBRACE enumPayloadField (COMMA enumPayloadField)* COMMA? RBRACE
    ;

enumPayloadField
    : IDENTIFIER COLON typeSpec
    ;

// struct Point { int32 x; int32 y; }
// struct Node<T> { T value; *Node<T> next; }
structDecl
    : STRUCT IDENTIFIER typeParamList? LBRACE structField* RBRACE
    ;

structField
    : typeSpec IDENTIFIER SEMI
    ;

// union Value { int32 i; float32 f; *void ptr; }
unionDecl
    : UNION IDENTIFIER typeParamList? LBRACE unionField* RBRACE
    ;

unionField
    : typeSpec IDENTIFIER SEMI
    ;

// extern int32 puts(*char s);
// extern int32 printf(*char fmt, ...);
externDecl
    : EXTERN typeSpec IDENTIFIER LPAREN externParamList? (COMMA SPREAD)? RPAREN SEMI
    ;

externParamList
    : externParam (COMMA externParam)*
    ;

externParam
    : typeSpec IDENTIFIER?
    ;

// int32 main() { ... }
// T max<T>(T a, T b) { ... }
functionDecl
    : typeSpec IDENTIFIER typeParamList? LPAREN paramList? RPAREN block
    ;

// extend Point { ... }
// extend Node<T> { ... }
extendDecl
    : EXTEND IDENTIFIER typeParamList? LBRACE extendMethod* RBRACE
    ;

// <T>, <K, V>, <T: numeric>
typeParamList
    : LT typeParam (COMMA typeParam)* GT
    ;

typeParam
    : IDENTIFIER (COLON IDENTIFIER)?  // T or T: constraint
    ;

extendMethod
    : typeSpec IDENTIFIER LPAREN AMPERSAND IDENTIFIER (COMMA param)* RPAREN block
    | typeSpec IDENTIFIER LPAREN paramList? RPAREN block
    ;

paramList
    : param (COMMA param)*
    ;

param
    : typeSpec SPREAD IDENTIFIER    // variadic: int32 ...n
    | typeSpec IDENTIFIER           // normal:   int32 n
    ;

// { statements }
block
    : LBRACE statement* RBRACE
    ;

// Statements
statement
    : varDeclStmt
    | assignStmt
    | compoundAssignStmt
    | derefAssignStmt
    | fieldIndexAssignStmt
    | indexFieldAssignStmt
    | fieldAssignStmt
    | fieldCompoundAssignStmt
    | arrowAssignStmt
    | arrowCompoundAssignStmt
    | callStmt
    | exprStmt
    | returnStmt
    | ifStmt
    | forStmt
    | loopStmt
    | whileStmt
    | doWhileStmt
    | breakStmt
    | continueStmt
    | switchStmt
    | lockStmt
    | tryCatchStmt
    | throwStmt
    | deferStmt
    | nakedBlockStmt
    | inlineBlockStmt
    | scopeBlockStmt
    ;

// defer free(ptr);  or  defer v.free();
deferStmt
    : DEFER callStmt
    | DEFER exprStmt
    ;

// { statements }  — creates a new lexical scope
nakedBlockStmt
    : LBRACE statement* RBRACE
    ;

// #inline { statements }  — injects statements into parent scope
inlineBlockStmt
    : INLINE_BLOCK LBRACE statement* RBRACE
    ;

// #scope (callback, ...) { statements }  — scoped RAII block with exit callbacks
scopeBlockStmt
    : SCOPE_BLOCK LPAREN scopeCallbackList? RPAREN LBRACE statement* RBRACE
    ;

scopeCallbackList
    : scopeCallback (COMMA scopeCallback)*
    ;

scopeCallback
    : IDENTIFIER (DOT IDENTIFIER)? LPAREN argList? RPAREN
    ;

// Expression as statement: x++; f(x); etc.
exprStmt
    : expression SEMI
    ;


// int32 x = 42;  or  int32 x;
varDeclStmt
    : typeSpec LPAREN IDENTIFIER (COMMA IDENTIFIER)* RPAREN ASSIGN expression SEMI  // auto (x, y) = expr;
    | typeSpec IDENTIFIER ASSIGN expression SEMI
    | typeSpec IDENTIFIER SEMI
    ;

// x = 42;  or  arr[i] = val;   arr[i][j] = val;
assignStmt
    : IDENTIFIER (LBRACKET expression RBRACKET)* ASSIGN expression SEMI
    ;

// x += 5;  x -= 3;  etc.
compoundAssignStmt
    : IDENTIFIER op=(PLUS_ASSIGN | MINUS_ASSIGN | STAR_ASSIGN | SLASH_ASSIGN
        | PERCENT_ASSIGN | AMP_ASSIGN | PIPE_ASSIGN | CARET_ASSIGN
        | LSHIFT_ASSIGN | RSHIFT_ASSIGN) expression SEMI
    ;

// p.x = 42;
fieldAssignStmt
    : IDENTIFIER (DOT IDENTIFIER)+ ASSIGN expression SEMI
    ;

// p.x += 5;  data.algo.pos.x -= 10;
fieldCompoundAssignStmt
    : IDENTIFIER (DOT IDENTIFIER)+ op=(PLUS_ASSIGN | MINUS_ASSIGN | STAR_ASSIGN | SLASH_ASSIGN
        | PERCENT_ASSIGN | AMP_ASSIGN | PIPE_ASSIGN | CARET_ASSIGN
        | LSHIFT_ASSIGN | RSHIFT_ASSIGN) expression SEMI
    ;

// arr[i].field = 42;
indexFieldAssignStmt
    : IDENTIFIER (LBRACKET expression RBRACKET)+ (DOT IDENTIFIER)+ ASSIGN expression SEMI
    ;

// p.field[i] = 42;   ts.buf[i] = val;
fieldIndexAssignStmt
    : IDENTIFIER (DOT IDENTIFIER)+ (LBRACKET expression RBRACKET)+ ASSIGN expression SEMI
    ;

// *p = 42;   or   *(p + 1) = 42;
derefAssignStmt
    : STAR IDENTIFIER ASSIGN expression SEMI
    | STAR LPAREN expression RPAREN ASSIGN expression SEMI
    ;

// ptr->field = 42;
arrowAssignStmt
    : IDENTIFIER ARROW IDENTIFIER ASSIGN expression SEMI
    ;

// ptr->field += 5;
arrowCompoundAssignStmt
    : IDENTIFIER ARROW IDENTIFIER op=(PLUS_ASSIGN | MINUS_ASSIGN | STAR_ASSIGN | SLASH_ASSIGN
        | PERCENT_ASSIGN | AMP_ASSIGN | PIPE_ASSIGN | CARET_ASSIGN
        | LSHIFT_ASSIGN | RSHIFT_ASSIGN) expression SEMI
    ;

// println(x);
callStmt
    : IDENTIFIER LPAREN argList? RPAREN SEMI
    ;

argList
    : expression (COMMA expression)*
    ;

// ret <expr>; or ret;
returnStmt
    : RET expression? SEMI
    ;

// if (cond) { ... } else if (cond) { ... } else { ... }
// parentheses are optional: if cond { ... } is also valid
ifStmt
    : IF LPAREN expression RPAREN ifBody elseIfClause* elseClause?
    | IF expression ifBody elseIfClause* elseClause?
    ;

elseIfClause
    : ELSE IF LPAREN expression RPAREN ifBody
    | ELSE IF expression ifBody
    ;

elseClause
    : ELSE ifBody
    ;

ifBody
    : block
    | statement
    ;

// for int32 x in arr {}
// for int32 i in 0..10 {}
// for int32 i in 0..=10 {}
// for int32 i = 0; i < 10; i++ {}
forStmt
    : FOR typeSpec IDENTIFIER IN expression block                                    # forInStmt
    | FOR typeSpec IDENTIFIER ASSIGN expression SEMI expression SEMI expression block # forClassicStmt
    ;

// break;
breakStmt
    : BREAK SEMI
    ;

// continue;
continueStmt
    : CONTINUE SEMI
    ;

// loop {}
loopStmt
    : LOOP block
    ;

// while cond {}
whileStmt
    : WHILE expression block
    | WHILE LPAREN expression RPAREN block
    ;

// do {} while cond;
doWhileStmt
    : DO block WHILE expression SEMI
    | DO block WHILE LPAREN expression RPAREN SEMI
    ;

// lock(mtx) { ... }
lockStmt
    : LOCK LPAREN expression RPAREN block
    ;

// try { ... } catch (Error e) { ... } finally { ... }
tryCatchStmt
    : TRY block catchClause+ finallyClause?
    | TRY block finallyClause
    ;

catchClause
    : CATCH LPAREN typeSpec IDENTIFIER RPAREN block
    ;

finallyClause
    : FINALLY block
    ;

// throw Error { message: "something went wrong" };
throwStmt
    : THROW expression SEMI
    ;

// switch x { case 1 { ... } case 2, 3 { ... } default { ... } }
switchStmt
    : SWITCH expression LBRACE caseClause* defaultClause? RBRACE
    | SWITCH LPAREN expression RPAREN LBRACE caseClause* defaultClause? RBRACE
    ;

caseClause
    : CASE expression (COMMA expression)* block
    ;

defaultClause
    : DEFAULT block
    ;

// Expressions (labeled alternatives for clean visitor dispatch)
// Order defines precedence: earlier = higher precedence for binary/suffix ops
expression
    // Postfix (highest precedence)
    : expression DOT IDENTIFIER LPAREN argList? RPAREN         # methodCallExpr
    | expression LPAREN argList? RPAREN                        # fnCallExpr    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT LPAREN argList? RPAREN  # genericFnCallExpr
    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT SCOPE IDENTIFIER LPAREN argList? RPAREN  # genericStaticMethodCallExpr    | expression DOT IDENTIFIER                               # fieldAccessExpr
    | expression DOT INT_LIT                                   # tupleIndexExpr
    | expression DOT FLOAT_LIT                                 # chainedTupleIndexExpr
    | expression ARROW IDENTIFIER LPAREN argList? RPAREN       # arrowMethodCallExpr
    | expression ARROW IDENTIFIER                              # arrowAccessExpr
    | expression ARROW INT_LIT                                 # tupleArrowIndexExpr
    | expression ARROW FLOAT_LIT                               # chainedTupleArrowIndexExpr
    | expression LBRACKET expression RBRACKET                  # indexExpr
    | expression AS typeSpec                                   # castExpr
    | expression IS typeSpec (SCOPE IDENTIFIER (LPAREN IDENTIFIER RPAREN)?)?   # isExpr
    | expression INCR                                          # postIncrExpr
    | expression DECR                                          # postDecrExpr
    // Special syntax
    | IDENTIFIER LBRACE (IDENTIFIER COLON expression (COMMA IDENTIFIER COLON expression)*)? RBRACE  # structLitExpr
    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT LBRACE (IDENTIFIER COLON expression (COMMA IDENTIFIER COLON expression)*)? RBRACE  # genericStructLitExpr
    | IDENTIFIER SCOPE IDENTIFIER LPAREN argList? RPAREN        # staticMethodCallExpr
    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT SCOPE IDENTIFIER  # genericEnumAccessExpr
    | IDENTIFIER SCOPE IDENTIFIER LBRACE (IDENTIFIER COLON expression (COMMA IDENTIFIER COLON expression)*)? RBRACE # enumNamedVariantExpr
    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT SCOPE IDENTIFIER LBRACE (IDENTIFIER COLON expression (COMMA IDENTIFIER COLON expression)*)? RBRACE # genericEnumNamedVariantExpr
    | IDENTIFIER SCOPE IDENTIFIER                              # enumAccessExpr
    // Unary prefix
    | STAR expression                                          # derefExpr
    | AMPERSAND expression                                     # addrOfExpr
    | MINUS expression                                         # negExpr
    | SPAWN expression                                         # spawnExpr
    | AWAIT expression                                         # awaitExpr
    | NOT expression                                           # logicalNotExpr
    | TILDE expression                                         # bitNotExpr
    | INCR expression                                          # preIncrExpr
    | DECR expression                                          # preDecrExpr
    | SIZEOF LPAREN typeSpec RPAREN                            # sizeofExpr
    | TYPEOF LPAREN expression RPAREN                           # typeofExpr
    // Multiplicative
    | expression op=(STAR | SLASH | PERCENT) expression        # mulExpr
    // Additive
    | expression op=(PLUS | MINUS) expression                  # addSubExpr
    // Shift
    | expression LSHIFT expression                              # lshiftExpr
    | expression GT GT expression                               # rshiftExpr
    // Relational
    | expression op=(LT | GT | LTE | GTE) expression           # relExpr
    // Equality
    | expression op=(EQ | NEQ) expression                      # eqExpr
    // Bitwise AND
    | expression AMPERSAND expression                          # bitAndExpr
    // Bitwise XOR
    | expression CARET expression                              # bitXorExpr
    // Bitwise OR
    | expression PIPE expression                               # bitOrExpr
    // Logical AND
    | expression LAND expression                               # logicalAndExpr
    // Logical OR
    | expression LOR expression                                # logicalOrExpr
    // Null coalescing
    | expression NULLCOAL expression                           # nullCoalExpr
    // Range
    | expression RANGE expression                              # rangeExpr
    | expression RANGE_INCL expression                         # rangeInclExpr
    // Ternary (right-associative for chaining: a ? b : c ? d : e)
    | <assoc=right> expression QUESTION expression COLON expression  # ternaryExpr
    // Unwrap-catch expression: expr catch { ... }
    | expression CATCH block                                    # catchUnwrapExpr
    // Try expression (lowest precedence – wraps entire sub-expression)
    | TRY expression                                           # tryExpr
    // Atoms
    | LPAREN expression COMMA expression (COMMA expression)* RPAREN  # tupleLitExpr
    | LPAREN expression RPAREN                                 # parenExpr
    | SPREAD expression                                        # spreadExpr
    | LBRACKET expression PIPE FOR typeSpec IDENTIFIER IN expression (IF expression)? RBRACKET  # listCompExpr
    | LBRACKET (expression (COMMA expression)*)? RBRACKET      # arrayLitExpr
    | NULL_LIT                                                 # nullLitExpr
    | INT_LIT                                                  # intLitExpr
    | HEX_LIT                                                  # hexLitExpr
    | OCT_LIT                                                  # octLitExpr
    | BIN_LIT                                                  # binLitExpr
    | FLOAT_LIT                                                # floatLitExpr
    | DOT INT_LIT                                              # leadingDotFloatLitExpr
    | BOOL_LIT                                                 # boolLitExpr
    | CHAR_LIT                                                 # charLitExpr
    | STR_LIT                                                  # strLitExpr
    | C_STR_LIT                                                # cStrLitExpr
    | IDENTIFIER                                               # identExpr
    ;

// Type specifiers (recursive for array types: []int32, [10]int32, [][]char)
typeSpec
    : STAR typeSpec                    // Pointer type: *int32, *[]char
    | typeSpec STAR                    // Error: C-style pointer (int32*)
    | LBRACKET INT_LIT RBRACKET typeSpec  // Fixed-size array: [10]int32, [3][3]float64
    | LBRACKET RBRACKET typeSpec       // Array type: []int32, [][]char
    | fnTypeSpec                       // Function type
    | VEC LT typeSpec GT               // Built-in: vec<int32>
    | MAP LT typeSpec COMMA typeSpec GT // Built-in: map<string, int32>
    | SET LT typeSpec GT               // Built-in: set<string>
    | TUPLE LT typeSpec (COMMA typeSpec)+ GT  // Built-in: tuple<int32, string>
    | IDENTIFIER LT typeSpec (COMMA typeSpec)* GT  // Generic extended type: Task<int32>
    | primitiveType                    // int32, float64, bool, etc.
    | AUTO                             // Type inference: auto x = 42;
    | IDENTIFIER                       // User-defined type: Point, Color, etc.
    ;

// fn(int32, int32) -> int32
fnTypeSpec
    : FN LPAREN (typeSpec (COMMA typeSpec)*)? RPAREN ARROW typeSpec
    ;

primitiveType
    : INT1
    | INT8
    | INT16
    | INT32
    | INT64
    | INT128
    | INTINF
    | ISIZE
    | UINT1
    | UINT8
    | UINT16
    | UINT32
    | UINT64
    | UINT128
    | USIZE
    | FLOAT32
    | FLOAT64
    | FLOAT80
    | FLOAT128
    | DOUBLE
    | BOOL
    | CHAR
    | VOID
    | STRING
    | CSTRING
    ;