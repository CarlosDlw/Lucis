# Grammar Reference

This page summarizes the formal grammar of the Lucis language, derived from the ANTLR4 grammar files (`LucisLexer.g4` and `LucisParser.g4`).

---

## Program Structure

```
program       → useDecl* includeDecl* topLevelDecl* EOF
```

### Imports

```
useDecl → 'use' modulePath '::' IDENTIFIER ';'
        | 'use' modulePath '::' '{' IDENTIFIER (',' IDENTIFIER)* '}' ';'

modulePath → IDENTIFIER ('::' IDENTIFIER)*
```

### Includes

```
includeDecl → '#include' '<' header '>'
            | '#include' '"' header '"'
```

---

## Top-Level Declarations

```
topLevelDecl → typeAliasDecl
             | structDecl
             | unionDecl
             | enumDecl
             | extendDecl
             | externDecl
             | functionDecl
```

### Type Alias

```
typeAliasDecl → 'type' IDENTIFIER '=' typeSpec ';'
```

```
type BinOp = fn(int32, int32) -> int32;
type Byte = uint8;
```

### Struct

```
structDecl → 'struct' IDENTIFIER '{' structField* '}'
structField → typeSpec IDENTIFIER ';'
```

```
struct Point {
    float64 x;
    float64 y;
}
```

### Union

```
unionDecl → 'union' IDENTIFIER '{' unionField* '}'
unionField → typeSpec IDENTIFIER ';'
```

```
union Value {
    int32 i;
    float32 f;
    *void ptr;
}
```

### Enum

```
enumDecl → 'enum' IDENTIFIER typeParamList? '{' enumVariant (',' enumVariant)* ','? '}'
enumVariant → IDENTIFIER
            | IDENTIFIER '(' typeSpec (',' typeSpec)* ')'
            | IDENTIFIER '{' enumPayloadField (',' enumPayloadField)* ','? '}'
enumPayloadField → IDENTIFIER ':' typeSpec
```

```
enum Color { Red, Green, Blue }

enum Result<V, E> {
    Ok(V),
    Err(E),
}

enum Shape {
    Circle { r: float64 },
    Rect { w: float64, h: float64 },
}
```

### Extern

```
externDecl → 'extern' typeSpec IDENTIFIER '(' externParamList? (',' '...')? ')' ';'
externParamList → externParam (',' externParam)*
externParam → typeSpec IDENTIFIER?
```

```
extern int32 printf(*char fmt, ...);
extern void exit(int32 code);
```

### Function

```
functionDecl → 'fn' IDENTIFIER typeParamList? '(' paramList? ')' typeSpec block
paramList → param (',' param)*
param → typeSpec '...' IDENTIFIER    // variadic
      | typeSpec IDENTIFIER          // normal
```

```
fn main() int32 {
    ret 0;
}

fn add(int32 a, int32 b) int32 {
    ret a + b;
}
```

### Extend

```
extendDecl → 'extend' IDENTIFIER '{' extendMethod* '}'
extendMethod → 'fn' IDENTIFIER '(' '&' IDENTIFIER (',' param)* ')' typeSpec block
             | 'fn' IDENTIFIER '(' paramList? ')' typeSpec block
```

```
extend Point {
    fn distance(&self) float64 {
        ret sqrt(self.x * self.x + self.y * self.y);
    }
}
```

---

## Statements

```
statement → varDeclStmt | assignStmt | compoundAssignStmt
          | derefAssignStmt | fieldAssignStmt
          | arrowAssignStmt | arrowCompoundAssignStmt
          | callStmt | exprStmt | returnStmt
          | ifStmt | forStmt | loopStmt | whileStmt | doWhileStmt
          | breakStmt | continueStmt | switchStmt
          | lockStmt | tryCatchStmt | throwStmt | deferStmt
```

### Variable Declaration

```
varDeclStmt → typeSpec IDENTIFIER '=' expression ';'
            | typeSpec IDENTIFIER ';'
```

```
int32 x = 42;
string name;
```

### Assignment

```
assignStmt → IDENTIFIER ('[' expression ']')* '=' expression ';'
compoundAssignStmt → IDENTIFIER op expression ';'
fieldAssignStmt → IDENTIFIER ('.' IDENTIFIER)+ '=' expression ';'
derefAssignStmt → '*' IDENTIFIER '=' expression ';'
                | '*' '(' expression ')' '=' expression ';'
arrowAssignStmt → IDENTIFIER '->' IDENTIFIER '=' expression ';'
arrowCompoundAssignStmt → IDENTIFIER '->' IDENTIFIER op expression ';'
```

Compound operators: `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`

```
x = 10;
arr[0] = 5;
x += 1;
p.x = 3.0;
*ptr = 42;
node->value = 10;
```

### Return

```
returnStmt → 'ret' expression? ';'
```

### If / Else

```
ifStmt → 'if' ('(' expression ')' | expression) ifBody elseIfClause* elseClause?
elseIfClause → 'else' 'if' ('(' expression ')' | expression) ifBody
elseClause → 'else' ifBody
ifBody → block | statement
```

Parentheses around the condition are optional.

```
if x > 0 {
    println("positive");
} else if (x == 0) {
    println("zero");
} else {
    println("negative");
}
```

### For

```
forStmt → 'for' typeSpec IDENTIFIER 'in' expression block
        | 'for' typeSpec IDENTIFIER '=' expression ';' expression ';' expression block
```

```
for int32 x in arr { println(x); }
for int32 i in 0..10 { println(i); }
for int32 i = 0; i < 10; i++ { println(i); }
```

### While / Do-While / Loop

```
whileStmt → 'while' ('(' expression ')' | expression) block
doWhileStmt → 'do' block 'while' ('(' expression ')' | expression) ';'
loopStmt → 'loop' block
```

```
while x > 0 { x -= 1; }
do { x += 1; } while x < 10;
loop { if done { break; } }
```

### Switch

```
switchStmt → 'switch' ('(' expression ')' | expression) '{' caseClause* defaultClause? '}'
caseClause → 'case' expression (',' expression)* block
defaultClause → 'default' block
```

```
switch x {
    case 1 { println("one"); }
    case 2, 3 { println("two or three"); }
    default { println("other"); }
}
```

### Try / Catch / Finally

```
tryCatchStmt → 'try' block catchClause+ finallyClause?
             | 'try' block finallyClause
catchClause → 'catch' '(' typeSpec IDENTIFIER ')' block
finallyClause → 'finally' block
```

```
try {
    riskyOperation();
} catch (Error e) {
    println(e.message);
} finally {
    cleanup();
}
```

### Other Statements

```
throwStmt → 'throw' expression ';'
deferStmt → 'defer' callStmt | 'defer' exprStmt
breakStmt → 'break' ';'
continueStmt → 'continue' ';'
lockStmt → 'lock' '(' expression ')' block
callStmt → IDENTIFIER '(' argList? ')' ';'
exprStmt → expression ';'
```

---

## Expressions

Expressions listed from highest to lowest precedence. See [Operator Precedence](operator-precedence.md) for a full table.

### Postfix (highest)

```
expression '.' IDENTIFIER '(' argList? ')'     // method call
expression '(' argList? ')'                     // function call
expression '.' IDENTIFIER                       // field access
expression '->' IDENTIFIER                      // arrow access
expression '[' expression ']'                   // index
expression 'as' typeSpec                        // cast
expression 'is' typeSpec ('::' IDENTIFIER ('(' IDENTIFIER ')')?)?  // type/variant check
expression '++'                                 // post-increment
expression '--'                                 // post-decrement
```

### Special Syntax

```
IDENTIFIER '{' (IDENTIFIER ':' expression (',' IDENTIFIER ':' expression)*)? '}'  // struct literal
IDENTIFIER '::' IDENTIFIER '(' argList? ')'                                        // static method
IDENTIFIER '<' typeSpec (',' typeSpec)* '>' '::' IDENTIFIER                        // generic enum access
IDENTIFIER '::' IDENTIFIER '{' (IDENTIFIER ':' expression (',' IDENTIFIER ':' expression)*)? '}' // enum named payload literal
IDENTIFIER '<' typeSpec (',' typeSpec)* '>' '::' IDENTIFIER '{' (IDENTIFIER ':' expression (',' IDENTIFIER ':' expression)*)? '}' // generic enum named payload literal
IDENTIFIER '::' IDENTIFIER                                                         // enum access
```

### Unary Prefix

```
'*' expression              // dereference
'&' expression              // address-of
'-' expression              // negation
'spawn' expression           // spawn task
'await' expression           // await task
'!' expression              // logical NOT
'~' expression              // bitwise NOT
'++' expression             // pre-increment
'--' expression             // pre-decrement
'sizeof' '(' typeSpec ')'   // size in bytes
'typeof' '(' expression ')' // type name
```

### Binary (highest to lowest)

```
* / %                        // multiplicative
+ -                          // additive
<< >>                        // shift
< > <= >=                    // relational
== !=                        // equality
&                            // bitwise AND
^                            // bitwise XOR
|                            // bitwise OR
&&                           // logical AND
||                           // logical OR
??                           // null coalescing
.. ..=                       // range
? :                          // ternary
```

### Try / Catch Expressions

```
expression 'catch' block     // enum unwrap with inline catch
'try' expression             // try expression (low precedence)
```

Example:

```
auto value = divide(10, 0) catch {
    println(it.message);
    ret 1;
};
```

For semantic constraints of `expression catch block` (required enum shape and `it` scope), see [Error Handling](../language/error-handling.md).

### Atoms

```
'(' expression ')'                                          // grouping
'...' expression                                             // spread
'[' expression '|' 'for' typeSpec IDENTIFIER 'in' expression ('if' expression)? ']'  // list comprehension
'[' (expression (',' expression)*)? ']'                      // array literal
'null'                                                       // null literal
INT_LIT | FLOAT_LIT | BOOL_LIT | CHAR_LIT | STR_LIT | C_STR_LIT | IDENTIFIER
```

---

## Type Specifiers

```
typeSpec → '*' typeSpec                                        // pointer: *int32
         | '[' INT_LIT ']' typeSpec                            // fixed array: [10]int32
         | '[]' typeSpec                                       // dynamic array: []int32
         | fnTypeSpec                                          // function type
         | IDENTIFIER '<' typeSpec (',' typeSpec)* '>'         // generic: vec<int32>
         | primitiveType                                       // int32, bool, etc.
         | IDENTIFIER                                          // user-defined: Point

fnTypeSpec → 'fn' '(' (typeSpec (',' typeSpec)*)? ')' '->' typeSpec
```

---

## Lexical Elements

### Literals

| Literal | Pattern | Examples |
|---------|---------|----------|
| Integer | `[0-9]+` | `42`, `0`, `1000000` |
| Float | `[0-9]+.[0-9]+` or scientific | `3.14`, `1e10`, `2.5e-3` |
| Bool | `true` \| `false` | `true`, `false` |
| Char | `'c'` with escapes | `'a'`, `'\n'`, `'\x41'` |
| String | `"..."` with escapes | `"hello"`, `"line\n"` |
| C String | `c"..."` with escapes | `c"hello %s\n"` |
| Null | `null` | `null` |

### Character Escapes

| Escape | Meaning |
|--------|---------|
| `\n` | Newline |
| `\r` | Carriage return |
| `\t` | Tab |
| `\\` | Backslash |
| `\'` | Single quote |
| `\"` | Double quote |
| `\0` | Null byte |
| `\a` | Bell |
| `\b` | Backspace |
| `\f` | Form feed |
| `\v` | Vertical tab |
| `\xHH` | Hex byte (e.g., `\x41` = `'A'`) |

### Comments

```
// Line comment

/* Block
   comment */
```

### Identifiers

```
IDENTIFIER → [a-zA-Z_][a-zA-Z0-9_]*
```
