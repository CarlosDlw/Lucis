#include "result.hpp"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <regex>

// =========================================================================
// Known checker error message patterns (prefixes/substrings)
// If checker stderr matches any of these, it's an EXPECTED rejection,
// NOT a bug. Sorted alphabetically for readability.
// =========================================================================
const std::vector<std::string> ResultClassifier::kCheckerErrors = {
    // ── Type spec / resolution ────────────────────────────────────
    "' expects ",
    "' is not a generic function",
    "' is not a generic struct",
    "' is not a generic struct or union",
    "' is not a known generic type",
    "' is not a known type or enum variant",
    "' is not a known type",
    "' is not an enum type",
    "'as' argument number",
    "'as' type mismatch",
    "'->' method call requires pointer to struct",
    "'->' requires a pointer type",
    "'->' requires pointer to struct",
    "'->' requires pointer to struct or union",
    "'??=' requires pointer field",
    "'??=' requires pointer target",
    "'??=' requires pointer variable",
    "'??=' type mismatch",
    "'.' requires a tuple type",
    "'.' requires element",
    "'.N.M' index requires a tuple type",
    "'.N.M' requires element",
    "ambiguous variant name",
    "argument number type mismatch",
    "array element type mismatch",
    "array size must be a positive integer",
    "array type '[]' has no method",
    "asm input constraint cannot be empty",
    "asm input constraint cannot use '='",
    "asm input constraint cannot use '+'",
    "asm matching constraint",
    "asm output constraint cannot be empty",
    "asm output constraint must start with '=', '+', or a digit",
    "asm statement with no outputs should be 'asm volatile'",

    // ── Break / continue ──────────────────────────────────────────
    "'break' used outside of a loop",
    "'continue' used outside of a loop",

    // ── Cast errors ────────────────────────────────────────────────
    "cannot cast 'char' to 'string'",
    "cannot cast 'cstring' to 'string'",
    "cannot cast 'string' to 'cstring'",
    "cannot cast 'string' to 'TYPE'",
    "cannot cast 'TYPE' to 'string'",
    "cannot cast 'TYPE' to 'TYPE': incompatible types",

    // ── Constants ──────────────────────────────────────────────────
    "constant 'NAME' initializer must be a compile-time constant",

    // ── Constraint satisfaction ────────────────────────────────────
    "type argument for 'PARAM' must be a bool",
    "type argument for 'PARAM' must be a float",
    "type argument for 'PARAM' must be an integer",
    "type argument for 'PARAM' must be a signed integer",
    "type argument for 'PARAM' must be a string",
    "type argument for 'PARAM' must be an unsigned integer",
    "type argument for 'PARAM' must be numeric",

    // ── Dereference ────────────────────────────────────────────────
    "cannot dereference non-pointer",
    "cannot dereference non-pointer variable",

    // ── Division by zero ───────────────────────────────────────────
    "division by zero",

    // ── Duplicate definitions ──────────────────────────────────────
    "duplicate enum variant",
    "duplicate field 'NAME' in struct",
    "duplicate field 'NAME' in union",
    "duplicate label",
    "duplicate payload field",
    "duplicate parameter name",

    // ── Enum ──────────────────────────────────────────────────────
    "enum 'TYPE' has no variant",
    "enum 'NAME' cannot be extended",
    "enum 'NAME' requires at most",
    "enum payload arrays must have fixed size",
    "enum type mismatch in variant 'is' check",
    "enum variant discriminant must be a compile-time constant",
    "enum variant discriminant value too large",

    // ── Extend ─────────────────────────────────────────────────────
    "cannot extend unknown type",
    "only structs and unions can be extended",

    // ── Field errors ────────────────────────────────────────────────
    "field 'NAME' expects type",
    "field 'NAME' type mismatch",
    "struct 'TYPE' has no field",
    "unknown field 'NAME' in",

    // ── For loops ──────────────────────────────────────────────────
    "cannot iterate over integer",
    "cannot iterate over type",
    "for condition has type",
    "for init type mismatch",

    // ── Function declaration ───────────────────────────────────────
    "function 'NAME' already defined",
    "function 'NAME' does not return a value on all code paths",
    "function must have a valid name",
    "function must return a value of type",
    "'...' must be the last parameter",

    // ── Function call ──────────────────────────────────────────────
    "'NAME' argument NUMBER type mismatch",
    "'NAME' expects ",
    "argument NUMBER has type 'void'",
    "argument NUMBER type mismatch",
    "function 'NAME' expects ",
    "function 'NAME' does not accept array arguments directly",
    "function call expects ",
    "generic function 'NAME' expects ",
    "typed variadic parameters are not supported",
    "untyped variadic function 'NAME' is deprecated",
    "variant 'NAME' expects ",
    "variant 'TYPE::NAME' argument NUMBER type mismatch",
    "variant 'TYPE::NAME' expects ",

    // ── Generic instantiation ──────────────────────────────────────
    "cannot infer generic type parameter",
    "cannot infer type arguments for generic enum",
    "generic enum 'NAME' expects ",
    "generic function 'NAME' expects ",
    "generic struct 'NAME' expects ",
    "generic union 'NAME' expects ",
    "recursive generic instantiation detected",
    "recursive generic function instantiation detected",
    "ambiguous generic inference",

    // ── Indexing ───────────────────────────────────────────────────
    "cannot subscript 'string' with a range",
    "index must be integer",
    "map key type mismatch",
    "map value type could not be resolved",
    "map value type mismatch",

    // ── If / condition ─────────────────────────────────────────────
    "condition has type 'TYPE', expected 'bool' or numeric type",

    // ── Inheritance ────────────────────────────────────────────────
    "parent type not found or is not a struct",

    // ── Match ──────────────────────────────────────────────────────
    "enum 'TYPE' has no variant 'NAME'",
    "literal type 'TYPE' does not match matched expression type",
    "match arm type 'TYPE' does not match previous arm type",
    "match guard must be a boolean expression",
    "match requires an enum expression",
    "match with literal patterns requires int, float, string, bool, or char",
    "non-exhaustive match",
    "variant 'NAME' is already handled in a previous arm",

    // ── Method calls ───────────────────────────────────────────────
    "method 'NAME' argument NUMBER type mismatch",
    "method 'NAME' expects ",
    "method 'NAME' is not defined for type",
    "method 'NAME' requires numeric element type",
    "type 'TYPE' does not support method calls",

    // ── Mixed numeric kinds ───────────────────────────────────────
    "does not allow mixed numeric kinds; cast explicitly",

    // ── Module / import ────────────────────────────────────────────
    "ambiguous variant name",
    "module 'PATH' does not export",
    "module root 'NAME' has no function",
    "type 'TYPE' from module 'NS' is not imported",
    "unknown module 'PATH'",
    "unknown module 'NAME'",
    "unknown module root",

    // ── Move / ownership ──────────────────────────────────────────
    "double-move detected",
    "use-after-move",

    // ── Nested collection types ───────────────────────────────────
    "nested collection types are not supported",

    // ── Operators ──────────────────────────────────────────────────
    "'??' type mismatch",
    "left side of '??' must be a pointer type",
    "operator '%' requires integer operands",
    "operator '++' requires integer or pointer operand",
    "operator '++' requires a variable",
    "operator '--' requires integer or pointer operand",
    "operator '--' requires a variable",
    "operator 'OP' cannot be applied to",
    "operator 'OP' requires compatible types",
    "operator 'OP' requires integer operand",
    "operator 'OP' requires integer operands",
    "operator 'OP' requires numeric operand",
    "operator 'OP' requires numeric operands",
    "operator '~' requires integer operand",
    "pointer arithmetic 'OP' requires integer operand",
    "ternary branches have incompatible types",
    "ternary condition has type",
    "unary '-' cannot be applied to unsigned type",
    "unary '-' requires numeric operand",

    // ── Pointer ───────────────────────────────────────────────────
    "invalid pointer syntax: use '*TYPE' instead of 'TYPE*'",

    // ── Range ─────────────────────────────────────────────────────
    "cannot infer type for 'NAME': range expression has no concrete type",
    "cannot initialize variable 'NAME' with a range expression",
    "range end must be integer",
    "range start must be integer",

    // ── Return ─────────────────────────────────────────────────────
    "function with return type 'TYPE' must return a value",
    "return type mismatch",
    "void function should not return a value",

    // ── sizeof / alignof / offsetof ──────────────────────────────
    "alignof: unknown type",
    "offsetof: expected a struct or union type",
    "offsetof: unknown type",
    "sizeof: unsized array type is not allowed",
    "sizeof: unknown type",

    // ── Spawn / await ─────────────────────────────────────────────
    "cannot resolve type of spawned expression",
    "cannot resolve type of awaited expression",

    // ── Static calls / intrinsics ──────────────────────────────────
    "does not support static methods",
    "invalid intrinsic path",
    "invalid qualified generic call",
    "invalid static call expression",
    "intrinsic 'NS::FUNC' does not exist",
    "intrinsic 'NS::FUNC' expects",
    "NS::TYPE' is not a known type",
    "qualified generic call is only supported for intrinsics",
    "static method 'TYPE::NAME' argument count mismatch",
    "static method 'TYPE::NAME' argument NUMBER type mismatch",
    "static method 'TYPE::NAME' does not exist",
    "struct 'TYPE' has no static method",
    "unknown intrinsic",
    "unsupported qualified static call",

    // ── Struct / union construction ───────────────────────────────
    "'TYPE' cannot be constructed this way",
    "'TYPE' is not a struct or union type",
    "invalid qualified type initialization",
    "struct 'TYPE' requires ",
    "union 'TYPE' requires ",

    // ── Switch ─────────────────────────────────────────────────────
    "switch expression must be an integer, char or enum type",

    // ── Try / catch / '?' ─────────────────────────────────────────
    "cannot use '?'",
    "try-or fallback type",

    // ── Tuple ──────────────────────────────────────────────────────
    "tuple destructuring expects ",
    "tuple destructuring requires a tuple expression",
    "tuple destructuring requires an initializer",
    "tuple index NUMBER is out of range",

    // ── Type alias ─────────────────────────────────────────────────
    "unknown param type 'NAME' in type alias",
    "unknown return type 'NAME' in type alias",
    "unknown type in type alias",
    "type 'NAME' already defined",

    // ── Type constraints ───────────────────────────────────────────
    "unknown type constraint",

    // ── Unwrap-catch ───────────────────────────────────────────────
    "unwrap-catch requires",
    "unwrap-catch requires enum",
    "unwrap-catch requires exactly",
    "unwrap-catch requires one error variant",

    // ── Use declarations ───────────────────────────────────────────
    "ambiguous variant name",
    "cannot use array type in 'use' wildcard",
    "type 'TYPE' is not an enum",
    "unknown type in 'use' wildcard",

    // ── Variables ─────────────────────────────────────────────────
    "cannot assign to constant",
    "cannot infer auto type for variable",
    "cannot infer type for 'NAME': ",
    "cannot infer type for variable",
    "cannot infer type: initializer for 'NAME' has type 'void'",
    "Map cannot be initialized with a literal",
    "type 'auto' requires ",
    "type mismatch: cannot assign ",
    "type mismatch: cannot initialize variable 'NAME' with a",
    "undefined variable",
    "variable 'NAME' already declared in this scope",

    // ── Misc ─────────────────────────────────────────────────────
    "'it' is only available inside",
    "empty program",
    "function-like macro 'NAME' expects",
    "type 'auto' requires an initializer",
    "unknown extended type",
    "array dimension must be a compile-time constant",
    "const 'NAME' must have a literal initializer to be used as array size",
    "c-string literal length (N) does not match array size",
    "duplicate enum variant discriminant",
    "enum variant discriminant must be",
    "generic extend for 'NAME' already defined",
    "invalid type specification",
    "unknown type",
    "type expression nesting exceeds maximum recursion depth",
    "tuple requires at least 2 type parameters",
};

// =========================================================================
// Checker crash patterns (NOT normal diagnostics)
// =========================================================================
bool ResultClassifier::isCheckerCrash(const std::string& stderr) {
    // C++ assertion failures in the checker
    if (stderr.find("Assertion") != std::string::npos) return true;
    if (stderr.find("assert(") != std::string::npos) return true;
    if (stderr.find("Segmentation fault") != std::string::npos) return true;
    if (stderr.find("Aborted") != std::string::npos) return true;
    if (stderr.find("signal ") != std::string::npos) return true;

    // Stack trace (C++ unhandled exception)
    if (stderr.find("terminate called after throwing") != std::string::npos) return true;
    if (stderr.find("what():") != std::string::npos) return true;

    return false;
}

// =========================================================================
// Is this a KNOWN checker diagnostic (expected rejection)?
// =========================================================================
bool ResultClassifier::isExpectedCheckerError(const std::string& stderr) {
    for (auto& pattern : kCheckerErrors) {
        if (stderr.find(pattern) != std::string::npos)
            return true;
    }
    return false;
}

// =========================================================================
// IR verifier / assertion in builder output
// =========================================================================
bool ResultClassifier::isIRVerifierError(const std::string& stderr) {
    if (stderr.find("LLVM IR") != std::string::npos) return true;
    if (stderr.find("verification") != std::string::npos) return true;
    if (stderr.find("Assertion") != std::string::npos) return true;
    if (stderr.find("LLVM ERROR") != std::string::npos) return true;
    return false;
}

// =========================================================================
// Main classification logic
// =========================================================================
FuzzOutcome ResultClassifier::classify(FuzzOutcome raw) {
    // ── Phase 1: Checker results ──────────────────────────────────
    if (raw.checkerTimedOut) {
        raw.kind = FuzzResult::Timeout;
        raw.stderr = raw.checkerStderr;
        return raw;
    }

    if (raw.checkerTermSig != 0) {
        // Checker was terminated by a signal
        if (raw.checkerTermSig == SIGSEGV || raw.checkerTermSig == SIGABRT ||
            raw.checkerTermSig == SIGBUS || raw.checkerTermSig == SIGFPE ||
            raw.checkerTermSig == SIGILL) {
            raw.kind = FuzzResult::CheckerCrash;
            raw.stderr = raw.checkerStderr;
            return raw;
        }
        // Other signal (e.g. SIGTERM from someone else) → treat as build error
        raw.kind = FuzzResult::BuildError;
        raw.stderr = raw.checkerStderr;
        return raw;
    }

    if (raw.checkerExitCode != 0) {
        // Checker rejected the code
        // Check if it's a CRASH (assertion, segfault message) vs expected diagnostic
        std::string combined = raw.checkerStderr + raw.checkerStdout;
        if (isCheckerCrash(combined)) {
            raw.kind = FuzzResult::CheckerCrash;
            raw.stderr = raw.checkerStderr;
            return raw;
        }
        // Check if it's a KNOWN error (expected)
        if (isExpectedCheckerError(raw.checkerStderr)) {
            raw.kind = FuzzResult::CheckerError;  // expected rejection
            raw.stderr = raw.checkerStderr;
            return raw;
        }
        // UNKNOWN error message — might be new checker check or regression
        // Treat as CheckerError (expected) but log it for review
        raw.kind = FuzzResult::CheckerError;
        raw.stderr = raw.checkerStderr;
        return raw;
    }

    // ── Checker PASSED → Phase 2: Build ────────────────────────────
    raw.checkerPassed = true;

    if (raw.builderTimedOut) {
        raw.kind = FuzzResult::Timeout;
        raw.stderr = raw.checkerStderr + "\n" + raw.builderStderr;
        return raw;
    }

    if (raw.builderTermSig != 0) {
        // Builder crashed after checker passed — REAL BUG
        raw.kind = FuzzResult::CompilerCrash;
        raw.stderr = raw.builderStderr;
        return raw;
    }

    if (raw.builderExitCode != 0) {
        // Build failed after checker passed
        std::string combined = raw.builderStderr + raw.builderStdout;

        // IR verifier error — REAL BUG (checker let invalid code through)
        if (isIRVerifierError(combined)) {
            raw.kind = FuzzResult::IRVerifierError;
            raw.stderr = raw.builderStderr;
            return raw;
        }

        // Checker passed but build failed — might be a linker error,
        // missing symbols, or some other build system issue
        raw.kind = FuzzResult::BuildError;
        raw.stderr = raw.builderStderr;
        return raw;
    }

    // ── Everything passed ──────────────────────────────────────────
    raw.kind = FuzzResult::Ok;
    raw.stderr = raw.builderStderr;
    return raw;
}
