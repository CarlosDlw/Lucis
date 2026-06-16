lexer grammar LucisLexer;

// Keywords
USE : 'use';
RET       : 'ret' | 'return';
STRUCT    : 'struct';
UNION     : 'union';
ENUM      : 'enum';
NULL_LIT  : 'null';
FN        : 'fn';
TYPE      : 'type';
AS        : 'as';
IS        : 'is';
SIZEOF    : 'sizeof';
TYPEOF    : 'typeof';
IF        : 'if';
ELSE      : 'else';
FOR       : 'for';
IN        : 'in';
LOOP      : 'loop';
WHILE     : 'while';
DO        : 'do';
BREAK     : 'break';
CONTINUE  : 'continue';
SWITCH    : 'switch';
CASE      : 'case';
DEFAULT   : 'default';
SPAWN     : 'spawn';
AWAIT     : 'await';
LOCK      : 'lock';
EXTEND    : 'extend';
TRY       : 'try';
CATCH     : 'catch';
FINALLY   : 'finally';
THROW     : 'throw';
DEFER     : 'defer';
OR        : 'or';
EXTERN    : 'extern';
AUTO      : 'auto';
VEC       : 'vec';
MAP       : 'map';
SET       : 'set';
TUPLE     : 'tuple';
ARROW     : '->';

// C header include directives
INCLUDE_SYS   : '#include' [ \t]+ '<' ~[>\r\n]+ '>';
INCLUDE_LOCAL  : '#include' [ \t]+ '"' ~["\r\n]+ '"';

// Structural block directives
INLINE_BLOCK  : '#inline';
SCOPE_BLOCK   : '#scope';

NULLCOAL  : '??';
SPREAD    : '...';
RANGE_INCL: '..=';
RANGE     : '..';

// Primitive types — sized integers
INT1      : 'int1';
INT8      : 'int8';
INT16     : 'int16';
INT32     : 'int32';
INT64     : 'int64';
INT128    : 'int128';
INTINF    : 'intinf';   // arbitrary-precision integer
ISIZE     : 'isize';    // pointer-sized integer (32 or 64 bits)

// Primitive types — unsigned integers
UINT1     : 'uint1';
UINT8     : 'uint8';
UINT16    : 'uint16';
UINT32    : 'uint32';
UINT64    : 'uint64';
UINT128   : 'uint128';
USIZE     : 'usize';    // pointer-sized unsigned integer

// Other primitives
FLOAT32   : 'float32';
FLOAT64   : 'float64';
FLOAT80   : 'float80';
FLOAT128  : 'float128';
DOUBLE    : 'double';
BOOL      : 'bool';
CHAR      : 'char';
VOID      : 'void';
STRING    : 'string';
CSTRING   : 'cstring';  // alias for *char

// Literals (suffixed forms before bare forms so ANTLR picks the longer match)
SUFFIXED_HEX   : '0' [xX] [0-9a-fA-F]+ [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf');
SUFFIXED_OCT   : '0' [oO] [0-7]+ [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf');
SUFFIXED_BIN   : '0' [bB] [01]+ [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf');
SUFFIXED_INT        : [0-9]+ [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf');
SUFFIXED_INT_FLOAT  : [0-9]+ 'f' ('32' | '64' | '80' | '128');
SUFFIXED_FLOAT      : [0-9]+ '.' [0-9]+ ([eE] [+-]? [0-9]+)? 'f' ('32' | '64' | '80' | '128')
                    | [0-9]+ [eE] [+-]? [0-9]+ 'f' ('32' | '64' | '80' | '128')
                    ;
SUFFIXED_FLOAT_INT  : [0-9]+ '.' [0-9]+ ([eE] [+-]? [0-9]+)? [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf')
                    | [0-9]+ [eE] [+-]? [0-9]+ [iu] ('8' | '16' | '32' | '64' | '128' | 'size' | 'inf')
                    ;
SUFFIXED_DOT_FLOAT  : '.' [0-9]+ 'f' ('32' | '64' | '80' | '128');
HEX_LIT   : '0' [xX] [0-9a-fA-F]+;
OCT_LIT   : '0' [oO] [0-7]+;
BIN_LIT   : '0' [bB] [01]+;
INT_LIT   : [0-9]+;
FLOAT_LIT : [0-9]+ '.' [0-9]+ ([eE] [+-]? [0-9]+)?
           | [0-9]+ [eE] [+-]? [0-9]+
           ;
BOOL_LIT  : 'true' | 'false';
C_STR_LIT : 'c"' (~["\\\r\n] | '\\' .)* '"';
STR_LIT   : '"' (~["\\\r\n] | '\\' .)* '"';

// Character literal with escape sequences: 'a', '\n', '\x41'
fragment CHAR_ESC : '\\' ('n' | 'r' | 't' | '\\' | '\'' | '"' | '0' | 'a' | 'b' | 'f' | 'v')
                  | '\\x' [0-9a-fA-F] [0-9a-fA-F]
                  ;
CHAR_LIT  : '\'' ( CHAR_ESC | ~['\\r\n] ) '\'';

// Identifier
ATTR_ERROR : '#[error]' ;

IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]*;

// Compound assignment (must appear before single-char symbols)
PLUS_ASSIGN    : '+=';
MINUS_ASSIGN   : '-=';
STAR_ASSIGN    : '*=';
SLASH_ASSIGN   : '/=';
PERCENT_ASSIGN : '%=';
AMP_ASSIGN     : '&=';
PIPE_ASSIGN    : '|=';
CARET_ASSIGN   : '^=';
LSHIFT_ASSIGN  : '<<=';
RSHIFT_ASSIGN  : '>>=';

// Symbols
SEMI      : ';';
COLON     : ':';
SCOPE     : '::';
COMMA     : ',';
DOT       : '.';
ASSIGN    : '=';
LPAREN    : '(';
RPAREN    : ')';
LBRACE    : '{';
RBRACE    : '}';
LBRACKET  : '[';
RBRACKET  : ']';
STAR      : '*';
AMPERSAND : '&';
MINUS     : '-';
PLUS      : '+';
SLASH     : '/';
PERCENT   : '%';
EQ        : '==';
NEQ       : '!=';
LTE       : '<=';
GTE       : '>=';
LT        : '<';
GT        : '>';
LAND      : '&&';
LOR       : '||';
NOT       : '!';
INCR      : '++';
DECR      : '--';
LSHIFT    : '<<';
PIPE      : '|';
CARET     : '^';
TILDE     : '~';
QUESTION  : '?';

// Whitespace and comments
WS        : [ \t\r\n]+ -> skip;
LINE_COMMENT : '//' ~[\r\n]* -> skip;
BLOCK_COMMENT: '/*' .*? '*/' -> skip;
