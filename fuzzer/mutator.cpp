#include "mutator.hpp"

#include <algorithm>
#include <regex>
#include <sstream>
#include <cassert>

// =========================================================================
// LucisType copy/move/dtor
// =========================================================================
Mutator::LucisType::LucisType(const LucisType& o)
    : base(o.base), isPointer(o.isPointer), isArray(o.isArray),
      arraySize(o.arraySize), isUnsizedArray(o.isUnsizedArray),
      isVec(o.isVec), isMap(o.isMap), isSet(o.isSet),
      isTuple(o.isTuple), isGeneric(o.isGeneric),
      genericName(o.genericName), typeArgs(o.typeArgs), pointee(nullptr) {
    if (o.pointee) pointee = new LucisType(*o.pointee);
}

Mutator::LucisType& Mutator::LucisType::operator=(const LucisType& o) {
    if (this == &o) return *this;
    base = o.base; isPointer = o.isPointer;
    isArray = o.isArray; arraySize = o.arraySize;
    isUnsizedArray = o.isUnsizedArray; isVec = o.isVec;
    isMap = o.isMap; isSet = o.isSet; isTuple = o.isTuple;
    isGeneric = o.isGeneric; genericName = o.genericName;
    typeArgs = o.typeArgs;
    delete pointee;
    pointee = o.pointee ? new LucisType(*o.pointee) : nullptr;
    return *this;
}

Mutator::LucisType::~LucisType() { delete pointee; }

// =========================================================================
// LucisType predicates
// =========================================================================
bool Mutator::LucisType::isInteger() const {
    switch (base) {
    case BaseType::Int1: case BaseType::Int8:
    case BaseType::Int16: case BaseType::Int32:
    case BaseType::Int64: case BaseType::Int128:
    case BaseType::IntInf: case BaseType::ISize:
    case BaseType::UInt1: case BaseType::UInt8:
    case BaseType::UInt16: case BaseType::UInt32:
    case BaseType::UInt64: case BaseType::UInt128:
    case BaseType::USize:
        return true;
    default: return false;
    }
}

bool Mutator::LucisType::isSigned() const {
    switch (base) {
    case BaseType::Int1: case BaseType::Int8:
    case BaseType::Int16: case BaseType::Int32:
    case BaseType::Int64: case BaseType::Int128:
    case BaseType::IntInf: case BaseType::ISize:
        return true;
    default: return false;
    }
}

bool Mutator::LucisType::isUnsigned() const {
    switch (base) {
    case BaseType::UInt1: case BaseType::UInt8:
    case BaseType::UInt16: case BaseType::UInt32:
    case BaseType::UInt64: case BaseType::UInt128:
    case BaseType::USize:
        return true;
    default: return false;
    }
}

bool Mutator::LucisType::isFloat() const {
    switch (base) {
    case BaseType::Float32: case BaseType::Float64:
    case BaseType::Float80: case BaseType::Float128:
    case BaseType::Double:
        return true;
    default: return false;
    }
}

bool Mutator::LucisType::isNumeric() const {
    return isInteger() || isFloat();
}

// =========================================================================
// LucisType::str() — convert to Lucis type syntax
// =========================================================================
std::string Mutator::LucisType::str() const {
    if (isVec) {
        std::string s = "vec<";
        if (!typeArgs.empty()) s += typeArgs[0].str();
        else s += "int32";
        return s + ">";
    }
    if (isMap) {
        std::string s = "map<";
        if (typeArgs.size() >= 2) {
            s += typeArgs[0].str() + ", " + typeArgs[1].str();
        } else s += "string, int32";
        return s + ">";
    }
    if (isSet) {
        std::string s = "set<";
        if (!typeArgs.empty()) s += typeArgs[0].str();
        else s += "string";
        return s + ">";
    }
    if (isTuple) {
        std::string s = "tuple<";
        for (size_t i = 0; i < typeArgs.size(); i++) {
            if (i > 0) s += ", ";
            s += typeArgs[i].str();
        }
        return s + ">";
    }
    if (isGeneric) {
        std::string s = genericName + "<";
        for (size_t i = 0; i < typeArgs.size(); i++) {
            if (i > 0) s += ", ";
            s += typeArgs[i].str();
        }
        return s + ">";
    }

    std::string s;
    switch (base) {
    case BaseType::Int1:     s = "int1";     break;
    case BaseType::Int8:     s = "int8";     break;
    case BaseType::Int16:    s = "int16";    break;
    case BaseType::Int32:    s = "int32";    break;
    case BaseType::Int64:    s = "int64";    break;
    case BaseType::Int128:   s = "int128";   break;
    case BaseType::IntInf:   s = "intinf";   break;
    case BaseType::ISize:    s = "isize";    break;
    case BaseType::UInt1:    s = "uint1";    break;
    case BaseType::UInt8:    s = "uint8";    break;
    case BaseType::UInt16:   s = "uint16";   break;
    case BaseType::UInt32:   s = "uint32";   break;
    case BaseType::UInt64:   s = "uint64";   break;
    case BaseType::UInt128:  s = "uint128";  break;
    case BaseType::USize:    s = "usize";    break;
    case BaseType::Float32:  s = "float32";  break;
    case BaseType::Float64:  s = "float64";  break;
    case BaseType::Float80:  s = "float80";  break;
    case BaseType::Float128: s = "float128"; break;
    case BaseType::Double:   s = "double";   break;
    case BaseType::Bool:     s = "bool";     break;
    case BaseType::Char:     s = "char";     break;
    case BaseType::Void:     s = "void";     break;
    case BaseType::String:   s = "string";   break;
    case BaseType::CString:  s = "cstring";  break;
    }

    if (isUnsizedArray) s = "[]" + s;
    else if (isArray) s = "[" + std::to_string(arraySize) + "]" + s;
    if (isPointer) s = "*" + s;
    return s;
}

// =========================================================================
// RNG helpers
// =========================================================================

int Mutator::randInt(int min, int max) {
    std::uniform_int_distribution<int> d(min, max);
    return d(rng_);
}

template<typename T>
T Mutator::pick(const std::vector<T>& vec) {
    return vec[randInt(0, static_cast<int>(vec.size()) - 1)];
}

// =========================================================================
// Type generators
// =========================================================================

Mutator::LucisType Mutator::randomBaseType() {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(BaseType::CString));
    return {static_cast<BaseType>(dist(rng_))};
}

Mutator::LucisType Mutator::randomIntType() {
    static const BaseType ints[] = {
        BaseType::Int1, BaseType::Int8, BaseType::Int16,
        BaseType::Int32, BaseType::Int64, BaseType::Int128,
        BaseType::IntInf, BaseType::ISize,
        BaseType::UInt1, BaseType::UInt8, BaseType::UInt16,
        BaseType::UInt32, BaseType::UInt64, BaseType::UInt128,
        BaseType::USize
    };
    return {ints[randInt(0, 14)]};
}

Mutator::LucisType Mutator::randomSignedInt() {
    static const BaseType sints[] = {
        BaseType::Int1, BaseType::Int8, BaseType::Int16,
        BaseType::Int32, BaseType::Int64, BaseType::Int128,
        BaseType::IntInf, BaseType::ISize
    };
    return {sints[randInt(0, 7)]};
}

Mutator::LucisType Mutator::randomUnsignedInt() {
    static const BaseType uints[] = {
        BaseType::UInt1, BaseType::UInt8, BaseType::UInt16,
        BaseType::UInt32, BaseType::UInt64, BaseType::UInt128,
        BaseType::USize
    };
    return {uints[randInt(0, 6)]};
}

Mutator::LucisType Mutator::randomFloatType() {
    static const BaseType floats[] = {
        BaseType::Float32, BaseType::Float64,
        BaseType::Float80, BaseType::Float128, BaseType::Double
    };
    return {floats[randInt(0, 4)]};
}

Mutator::LucisType Mutator::randomScalar() {
    std::uniform_int_distribution<int> cat(0, 3);
    switch (cat(rng_)) {
    case 0: return randomIntType();
    case 1: return randomFloatType();
    case 2: return {BaseType::Bool};
    case 3: return {BaseType::Char};
    default: return {BaseType::Int32};
    }
}

Mutator::LucisType Mutator::randomFullType(int depth) {
    if (depth > 4) return randomScalar();

    LucisType t;
    std::uniform_int_distribution<int> mod(0, 10);
    switch (mod(rng_)) {
    case 0: {
        // Pointer
        LucisType inner = randomScalar();
        t = inner;
        t.isPointer = true;
        return t;
    }
    case 1: {
        // Array
        t = randomScalar();
        t.isArray = true;
        t.arraySize = static_cast<unsigned>(randInt(1, 20));
        return t;
    }
    case 2: {
        // Vec
        LucisType elem = randomScalar();
        t.isVec = true;
        t.typeArgs.push_back(elem);
        return t;
    }
    case 3: {
        // Map
        LucisType k = randomScalar();
        LucisType v = randomScalar();
        t.isMap = true;
        t.typeArgs.push_back(k);
        t.typeArgs.push_back(v);
        return t;
    }
    case 4: {
        // Unsized array
        t = randomScalar();
        t.isUnsizedArray = true;
        return t;
    }
    default:
        return randomScalar();
    }
}

Mutator::LucisType Mutator::randomGenericType(int depth) {
    if (depth > 3) return randomScalar();

    LucisType t;
    t.isGeneric = true;
    static const char* genNames[] = {"Result", "Option", "Pair", "Container"};
    t.genericName = genNames[randInt(0, 3)];

    int numArgs = randInt(1, 2);
    for (int i = 0; i < numArgs; i++)
        t.typeArgs.push_back(randomScalar());

    return t;
}

// =========================================================================
// Literals
// =========================================================================

std::string Mutator::typeLit(const LucisType& t) {
    // The suffix for a literal of this type
    if (t.isPointer || t.isArray || t.isVec || t.isMap || t.isSet)
        return ""; // no literal for these
    return randomLit(t);
}

std::string Mutator::randomIntLit(const LucisType& t) {
    int64_t val = randInt(0, 255);
    switch (t.base) {
    case BaseType::Int1:    return std::to_string(val & 1) + "i8"; // int1 via cast
    case BaseType::Int8:    return std::to_string(val) + "i8";
    case BaseType::Int16:   return std::to_string(val) + "i16";
    case BaseType::Int32:   return std::to_string(val);
    case BaseType::Int64:   return std::to_string(val) + "i64";
    case BaseType::Int128:  return std::to_string(val) + "i128";
    case BaseType::IntInf:  return std::to_string(val);
    case BaseType::ISize:   return std::to_string(val) + "isize";
    case BaseType::UInt1:   return std::to_string(val & 1) + "u8";
    case BaseType::UInt8:   return std::to_string(val) + "u8";
    case BaseType::UInt16:  return std::to_string(val) + "u16";
    case BaseType::UInt32:  return std::to_string(val) + "u32";
    case BaseType::UInt64:  return std::to_string(val) + "u64";
    case BaseType::UInt128: return std::to_string(val) + "u128";
    case BaseType::USize:   return std::to_string(val) + "usize";
    default:                return std::to_string(val);
    }
}

std::string Mutator::randomBoolLit() {
    return randInt(0, 1) ? "true" : "false";
}

std::string Mutator::randomCharLit() {
    static const char chars[] = {'a', 'b', 'c', 'x', 'y', 'z', '0', '1', ' ', '\n'};
    char c = chars[randInt(0, 8)];
    if (c == '\n') return "'\\n'";
    return std::string("'") + c + "'";
}

std::string Mutator::randomFloatLit(const LucisType& t) {
    double val = randInt(0, 100) / 10.0;
    std::string s = std::to_string(val);
    switch (t.base) {
    case BaseType::Float32:  return s + "f32";
    case BaseType::Float64:  return s;
    case BaseType::Float80:  return s + "f80";
    case BaseType::Float128: return s + "f128";
    case BaseType::Double:   return s;
    default:                 return s;
    }
}

std::string Mutator::randomLit(const LucisType& t) {
    if (t.base == BaseType::Bool) return randomBoolLit();
    if (t.base == BaseType::Char) return randomCharLit();
    if (t.isFloat()) return randomFloatLit(t);
    return randomIntLit(t);
}

// =========================================================================
// Scope management
// =========================================================================

Mutator::Scope& Mutator::currentScope() {
    if (scopes_.empty())
        scopes_.push_back({});
    return scopes_.back();
}

std::string Mutator::freshVarName() {
    static const char* names[] = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
        "x", "y", "z", "w", "p", "q", "r", "s", "t", "u",
        "v", "m", "n"
    };
    return names[varCounter_++ % 23];
}

std::string Mutator::pushVar(const std::string& name, const LucisType& type) {
    currentScope().vars.push_back({name, type});
    return name;
}

// =========================================================================
// Expression generation (grammar-aware, depth-controlled)
// =========================================================================

std::string Mutator::genExpr(const LucisType& desired, int depth) {
    if (depth > 6) {
        // Fallback to a simple literal or variable
        if (!currentScope().vars.empty() && randInt(0, 1)) {
            return genIdentExpr(desired);
        }
        return randomLit(desired);
    }

    std::uniform_int_distribution<int> choice(0, 8);
    switch (choice(rng_)) {
    case 0: return randomLit(desired);
    case 1: return genIdentExpr(desired);
    case 2: return genParenExpr(desired, depth);
    case 3: return genUnaryExpr(desired, depth + 1);
    case 4: return genBinaryExpr(desired, depth + 1);
    case 5: return genCastExpr(desired, depth + 1);
    case 6: return genCallExpr(desired, depth + 1);
    case 7: return genTernaryExpr(desired, depth + 1);
    default: return randomLit(desired);
    }
}

std::string Mutator::genIdentExpr(const LucisType& desired) {
    auto& scope = currentScope();
    if (scope.vars.empty()) return randomLit(desired);

    // Find compatible variables (same type category)
    std::vector<VarInfo*> candidates;
    for (auto& v : scope.vars) {
        if (v.type.base == desired.base)
            candidates.push_back(&v);
    }
    if (candidates.empty()) return randomLit(desired);

    return candidates[randInt(0, static_cast<int>(candidates.size()) - 1)]->name;
}

std::string Mutator::genParenExpr(const LucisType& desired, int depth) {
    return "(" + genExpr(desired, depth + 1) + ")";
}

std::string Mutator::genUnaryExpr(const LucisType& desired, int depth) {
    if (desired.base == BaseType::Bool) {
        // Bool only supports logical not (!)
        return "!" + genParenExpr(desired, depth + 1);
    }
    if (desired.isInteger()) {
        // Integers support negation (-) and bitwise not (~)
        const char* op = (randInt(0, 1) == 0) ? "-" : "~";
        return std::string(op) + genParenExpr(desired, depth + 1);
    }
    if (desired.isFloat()) {
        // Floats support negation (-)
        return "-" + genParenExpr(desired, depth + 1);
    }
    // For other types, fall back to a literal
    return randomLit(desired);
}

std::string Mutator::genBinaryExpr(const LucisType& desired, int depth) {
    if (desired.isInteger()) {
        // Arithmetic and bitwise operators — return same type as operands
        static const char* ops[] = {"+", "-", "*", "/", "%", "&", "|", "^", "<<"};
        const char* op = ops[randInt(0, 8)];
        std::string lhs = genExpr(desired, depth + 1);
        std::string rhs = genExpr(desired, depth + 1);
        return lhs + " " + op + " " + rhs;
    }
    if (desired.isFloat()) {
        // Arithmetic operators — return same type as operands
        static const char* ops[] = {"+", "-", "*", "/"};
        const char* op = ops[randInt(0, 3)];
        std::string lhs = genExpr(desired, depth + 1);
        std::string rhs = genExpr(desired, depth + 1);
        return lhs + " " + op + " " + rhs;
    }
    if (desired.base == BaseType::Bool) {
        if (randInt(0, 1)) {
            // Comparison operators — take numeric operands, return bool
            LucisType numeric = randomIntType();
            static const char* cmpOps[] = {"==", "!=", "<", ">", "<=", ">="};
            const char* op = cmpOps[randInt(0, 5)];
            std::string lhs = genExpr(numeric, depth + 1);
            std::string rhs = genExpr(numeric, depth + 1);
            return lhs + " " + op + " " + rhs;
        } else {
            // Logical operators — take bool operands, return bool
            static const char* logOps[] = {"&&", "||"};
            const char* op = logOps[randInt(0, 1)];
            std::string lhs = genExpr(desired, depth + 1);
            std::string rhs = genExpr(desired, depth + 1);
            return lhs + " " + op + " " + rhs;
        }
    }
    // For other types, fall back to a literal
    return randomLit(desired);
}

std::string Mutator::genCastExpr(const LucisType& desired, int depth) {
    LucisType src = randomScalar();
    return "(" + genExpr(src, depth + 1) + ") as " + desired.str();
}

std::string Mutator::genCallExpr(const LucisType& desired, int depth) {
    // Generate a call to a built-in or a known function
    // We may not have user functions in scope, use sizeof/typeof
    std::uniform_int_distribution<int> choice(0, 3);
    switch (choice(rng_)) {
    case 0: return "sizeof(" + desired.str() + ")";
    case 1: return "typeof(" + genExpr(desired, depth + 1) + ")";
    case 2: {
        // Try to call a named function from corpus or a helper
        LucisType argType = randomScalar();
        return "dummy_fn(" + genExpr(argType, depth + 1) + ")";
    }
    default: return genExpr(desired, depth + 1);
    }
}

std::string Mutator::genTernaryExpr(const LucisType& desired, int depth) {
    LucisType condType;
    condType.base = BaseType::Bool;
    return "(" + genExpr(condType, depth + 1) + " ? " +
           genExpr(desired, depth + 1) + " : " +
           genExpr(desired, depth + 1) + ")";
}

// =========================================================================
// Statement generation
// =========================================================================

std::string Mutator::genVarDecl(const LucisType& t) {
    std::string name = freshVarName();
    pushVar(name, t);

    if (t.isVec) {
        return "  vec<" + t.typeArgs[0].str() + "> " + name + ";\n";
    }
    if (t.isMap) {
        return "  map<" + t.typeArgs[0].str() + ", " + t.typeArgs[1].str() + "> " + name + ";\n";
    }
    if (t.isPointer) {
        // Need to allocate something to point to
        LucisType pointeeType = t.pointee ? *t.pointee : randomScalar();
        return "  " + t.str() + " " + name + " = null;\n";
    }

    return "  " + t.str() + " " + name + " = " + randomLit(t) + ";\n";
}

std::string Mutator::genAssign(const std::string& varName, const LucisType& t) {
    return "  " + varName + " = " + randomLit(t) + ";\n";
}

std::string Mutator::genReturn(const LucisType& t) {
    if (t.base == BaseType::Void)
        return "  ret;\n";
    return "  ret " + randomLit(t) + ";\n";
}

std::string Mutator::genIf(const LucisType& retType) {
    LucisType boolType;
    boolType.base = BaseType::Bool;
    std::string code;
    code += "  if (" + randomBoolLit() + ") {\n";
    code += genBlock(retType, randInt(1, 3), 1);
    code += "  } else {\n";
    code += genBlock(retType, randInt(1, 2), 1);
    code += "  }\n";
    return code;
}

std::string Mutator::genFor(const LucisType& retType) {
    LucisType idxType;
    idxType.base = BaseType::Int32;
    std::string idxName = freshVarName();
    pushVar(idxName, idxType);
    std::string code;
    code += "  for int32 " + idxName + " = 0; " + idxName + " < 10; " + idxName + "++ {\n";
    code += genBlock(retType, randInt(1, 2), 1);
    code += "  }\n";
    return code;
}

std::string Mutator::genWhile(const LucisType& retType) {
    std::string code;
    code += "  while (" + randomBoolLit() + ") {\n";
    code += genBlock(retType, randInt(1, 2), 1);
    code += "  }\n";
    return code;
}

std::string Mutator::genStatement(const LucisType& retType, int depth) {
    std::uniform_int_distribution<int> choice(0, 8);
    switch (choice(rng_)) {
    case 0: return genVarDecl(randomScalar());
    case 1: return genVarDecl(randomIntType());
    case 2: {
        if (currentScope().vars.empty()) return genVarDecl(randomScalar());
        auto& v = currentScope().vars[randInt(0, static_cast<int>(currentScope().vars.size()) - 1)];
        return genAssign(v.name, v.type);
    }
    case 3:
    case 4: {
        LucisType exprType = randomScalar();
        std::string expr = genExpr(exprType, depth);
        return "  " + expr + ";\n";
    }
    case 5: return genIf(retType);
    case 6: return genFor(retType);
    case 7: return genWhile(retType);
    default: return "  {} // empty block\n";
    }
}

std::string Mutator::genBlock(const LucisType& retType, int stmtCount, int depth) {
    scopes_.push_back({});
    std::string code;
    for (int i = 0; i < stmtCount; i++)
        code += genStatement(retType, depth);
    scopes_.pop_back();
    return code;
}

// =========================================================================
// Wrapping: create a complete program
// =========================================================================

std::string Mutator::wrapProgram(const std::string& body) {
    std::string full;
    full += "use std::log::println;\n\n";
    full += body;
    full += "\nfn main() int32 {\n";
    full += "  test_fuzz();\n";
    full += "  return 0;\n";
    full += "}\n";
    return full;
}

// =========================================================================
// Generation strategy: GrammarSimple
// =========================================================================

std::string Mutator::genGrammarSimple() {
    scopes_.clear();
    varCounter_ = 0;

    LucisType retType;
    retType.base = BaseType::Void;

    std::string code;
    code += "fn test_fuzz() " + retType.str() + " {\n";

    int stmts = randInt(3, 10);
    for (int i = 0; i < stmts; i++)
        code += genStatement(retType, 0);

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Generation strategy: GrammarPointers
// =========================================================================

std::string Mutator::genGrammarPointers() {
    scopes_.clear();
    varCounter_ = 0;

    auto elemType = randomIntType();
    auto ptrType = elemType;
    ptrType.isPointer = true;

    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  " + elemType.str() + " val = " + randomIntLit(elemType) + ";\n";
    code += "  " + ptrType.str() + " p = &val;\n";
    code += "  " + elemType.str() + " read = *p;\n";

    // Pointer arithmetic
    auto offType = randomIntType();
    code += "  " + offType.str() + " offset = " + randomIntLit(offType) + ";\n";
    code += "  " + ptrType.str() + " q = p + offset;\n";

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Generation strategy: GrammarArrays
// =========================================================================

std::string Mutator::genGrammarArrays() {
    scopes_.clear();
    varCounter_ = 0;

    auto elem = randomIntType();
    unsigned sz = static_cast<unsigned>(randInt(1, 15));

    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  [" + std::to_string(sz) + "]" + elem.str() + " arr;\n";
    code += "  " + elem.str() + " first = arr[0];\n";
    if (sz > 1) {
        int idx = randInt(0, static_cast<int>(sz) - 1);
        code += "  " + elem.str() + " middle = arr[" + std::to_string(idx) + "];\n";
    }

    // Unsized array test
    code += "  []" + elem.str() + " slice = arr;\n";

    // Array copy
    code += "  [" + std::to_string(sz) + "]" + elem.str() + " copy = arr;\n";

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Generation strategy: GrammarGenerics
// =========================================================================

std::string Mutator::genGrammarGenerics() {
    scopes_.clear();
    varCounter_ = 0;

    auto elem = randomIntType();

    std::string code;
    code += "fn test_fuzz() void {\n";

    // Vec
    code += "  vec<" + elem.str() + "> v;\n";
    code += "  v.push(" + randomIntLit(elem) + ");\n";
    code += "  v.push(" + randomIntLit(elem) + ");\n";

    code += "  " + elem.str() + " x = v.at(0 as usize);\n";

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Generation strategy: GrammarControlFlow
// =========================================================================

std::string Mutator::genGrammarControlFlow() {
    scopes_.clear();
    varCounter_ = 0;

    std::string code;
    code += "fn test_fuzz() void {\n";

    LucisType voidType;
    voidType.base = BaseType::Void;

    code += genIf(voidType);
    code += genFor(voidType);
    code += genWhile(voidType);

    code += "  // switch (int)\n";
    code += "  {\n";
    code += "    int32 sw = 3;\n";
    code += "    switch (sw) {\n";
    code += "      case 1 { println(\"one\"); }\n";
    code += "      case 2 { println(\"two\"); }\n";
    code += "      default { println(\"other\"); }\n";
    code += "    }\n";
    code += "  }\n";

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Generation strategy: GrammarFull
// =========================================================================

std::string Mutator::genGrammarFull() {
    scopes_.clear();
    varCounter_ = 0;

    std::string code;
    code += "fn test_fuzz() void {\n";

    int stmts = randInt(5, 15);
    LucisType voidType;
    voidType.base = BaseType::Void;

    for (int i = 0; i < stmts; i++) {
        std::uniform_int_distribution<int> choice(0, 12);
        switch (choice(rng_)) {
        case 0: case 1: case 2:
            code += genVarDecl(randomScalar());
            break;
        case 3:
            code += genVarDecl(randomFullType(0));
            break;
        case 4: case 5:
            code += genStatement(voidType, 2);
            break;
        case 6:
            code += genIf(voidType);
            break;
        case 7:
            code += genFor(voidType);
            break;
        case 8: {
            // Nested block
            code += "  {\n";
            code += genBlock(voidType, randInt(2, 4), 1);
            code += "  }\n";
            break;
        }
        case 9: {
            // Expression statement
            LucisType exprType = randomScalar();
            code += "  " + genExpr(exprType, 2) + ";\n";
            break;
        }
        case 10: {
            // Call println
            code += "  println(\"fuzz\");\n";
            break;
        }
        case 11: {
            // Return early
            code += "  ret;\n";
            break;
        }
        case 12: {
            // Cast expression
            LucisType src = randomScalar();
            LucisType dst = randomScalar();
            code += "  " + dst.str() + " casted = " + genExpr(src, 2) + " as " + dst.str() + ";\n";
            break;
        }
        }
    }

    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Edge case generators (targeted, may contain deliberate errors)
// =========================================================================

std::string Mutator::genEdgeMixedArith() {
    auto ta = randomSignedInt();
    auto tb = randomUnsignedInt();
    auto tr = randomSignedInt();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  " + ta.str() + " a = " + randomIntLit(ta) + ";\n";
    code += "  " + tb.str() + " b = " + randomIntLit(tb) + ";\n";
    code += "  " + tr.str() + " c = a + b;\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeCastChain() {
    auto t1 = randomIntType();
    auto t2 = randomIntType();
    auto t3 = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  " + t1.str() + " a = " + randomIntLit(t1) + ";\n";
    code += "  " + t2.str() + " b = a as " + t2.str() + ";\n";
    code += "  " + t3.str() + " c = b as " + t3.str() + ";\n";
    code += "  " + t1.str() + " d = c as " + t1.str() + ";\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgePtrArith() {
    auto elem = randomIntType();
    auto off  = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  [20]" + elem.str() + " arr;\n";
    code += "  *" + elem.str() + " p = &arr[0];\n";
    code += "  " + off.str() + " n = " + randomIntLit(off) + ";\n";
    code += "  *" + elem.str() + " q = p + n;\n";
    code += "  int64 diff = q - p;\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeArrayEdge() {
    auto elem = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    // Single-element array (zero-size arrays are not valid in Lucis)
    code += "  [1]" + elem.str() + " single_elem;\n";
    // Single element
    code += "  [1]" + elem.str() + " single;\n";
    // Large array
    code += "  [100]" + elem.str() + " big;\n";
    // Array of arrays
    code += "  [5][10]" + elem.str() + " nested;\n";
    code += "  " + elem.str() + " x = nested[0][1];\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeGenericEdge() {
    auto elem = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    // Vec
    code += "  vec<" + elem.str() + "> v;\n";
    code += "  v.push(" + randomIntLit(elem) + ");\n";
    // Map (uses [] indexing, not .set)
    code += "  map<int32, " + elem.str() + "> m;\n";
    code += "  m[1] = " + randomIntLit(elem) + ";\n";
    code += "  " + elem.str() + " val = m[1];\n";
    // Set
    code += "  set<int32> s;\n";
    code += "  s.add(42);\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeOwnership() {
    std::string code;
    code += "fn test_fuzz() void {\n";
    // String operations (ownership tracked)
    code += "  string s1 = \"hello\";\n";
    code += "  string s2 = s1;\n";   // move
    code += "  println(s2);\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeMatchEnum() {
    auto elem = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  vec<" + elem.str() + "> v;\n";
    code += "  v.push(" + randomIntLit(elem) + ");\n";
    code += "  " + elem.str() + " x = v.at(0 as usize);\n";
    code += "  println(x.toString());\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeConstExpr() {
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  const int32 MAX = 100;\n";
    code += "  const int32 MIN = -MAX;\n";
    code += "  int32 range = MAX - MIN;\n";
    code += "  println(range.toString());\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeControlFlow() {
    auto elem = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  " + elem.str() + " sum = " + randomIntLit(elem) + ";\n";
    code += "  for int32 i = 0; i < 10; i++ {\n";
    code += "    if (i == 5) {\n";
    code += "      continue;\n";
    code += "    }\n";
    code += "    if (i == 8) {\n";
    code += "      break;\n";
    code += "    }\n";
    code += "    sum = sum + 1 as " + elem.str() + ";\n";
    code += "  }\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeConcurrency() {
    std::string code;
    code += "use std::thread::Task;\n\n";
    // Top-level helper function for spawn (functions must be top-level in Lucis)
    code += "fn spawn_helper() int32 {\n";
    code += "  ret 42;\n";
    code += "}\n\n";
    code += "fn test_fuzz() void {\n";
    code += "  Task<int32> t = spawn spawn_helper();\n";
    code += "  int32 result = await t;\n";
    code += "  println(result.toString());\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeStdlib() {
    std::string code;
    code += "fn test_fuzz() void {\n";
    // String operations
    code += "  string s = \"hello, world\";\n";
    code += "  int32 len = s.len() as int32;\n";
    code += "  string sub = s.slice(0 as usize, 5 as usize);\n";
    code += "  println(sub);\n";
    // Number conversion
    code += "  string num = 42.toString();\n";
    code += "  println(num);\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeNestedExpr() {
    auto t = randomIntType();
    std::string code;
    code += "fn test_fuzz() void {\n";
    code += "  " + t.str() + " a = " + randomIntLit(t) + ";\n";
    code += "  " + t.str() + " b = " + randomIntLit(t) + ";\n";
    code += "  " + t.str() + " c = ((a + b) * (a - b)) / (a + 1 as " + t.str() + ");\n";
    code += "  " + t.str() + " d = ((a as int64) as " + t.str() + ");\n";
    code += "  " + t.str() + " e = ((a + b) & (a - b)) | (a * b);\n";
    code += "}\n";
    return wrapProgram(code);
}

std::string Mutator::genEdgeCastSignedness() {
    auto ts = randomSignedInt();
    auto tu = randomUnsignedInt();
    std::string code;
    code += "fn test_fuzz() void {\n";
    // Sign extension: negative signed → wider signed
    code += "  " + ts.str() + " neg = -1 as " + ts.str() + ";\n";
    code += "  int64 extended = neg as int64;\n";
    // Signed → unsigned
    code += "  " + tu.str() + " unsigned = neg as " + tu.str() + ";\n";
    // Unsigned → signed (possible truncation)
    code += "  " + ts.str() + " back = unsigned as " + ts.str() + ";\n";
    code += "}\n";
    return wrapProgram(code);
}

// =========================================================================
// Corpus mutation strategies
// =========================================================================

std::string Mutator::genCorpusMutate() {
    if (!corpus_ || corpus_->empty()) return genGrammarSimple();

    auto fn = corpus_->randomFunction();

    // Simple mutations on the body
    std::string body = fn.body;

    // Maybe add a println
    if (randInt(0, 1)) {
        auto insertPos = body.rfind('}');
        if (insertPos != std::string::npos) {
            body.insert(insertPos, "  println(\"mutated\");\n");
        }
    }

    std::string code;
    code += fn.signature + " " + body + "\n";
    return wrapProgram(code);
}

std::string Mutator::genCorpusSplice() {
    if (!corpus_ || corpus_->size() < 2) return genGrammarSimple();

    auto fn1 = corpus_->randomFunction();
    auto fn2 = corpus_->randomFunction();

    // Take body from fn1, insert a line from fn2
    std::string body = fn1.body;
    auto insertPos = body.rfind('}');
    if (insertPos != std::string::npos) {
        // Take a random line from fn2's body
        std::istringstream stream(fn2.body);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(stream, line))
            lines.push_back(line);
        if (!lines.empty()) {
            std::string stolen = lines[randInt(0, static_cast<int>(lines.size()) - 1)];
            body.insert(insertPos, "  " + stolen + "\n");
        }
    }

    std::string code;
    code += fn1.signature + " " + body + "\n";
    return wrapProgram(code);
}

std::string Mutator::genCorpusTypeSwap() {
    if (!corpus_ || corpus_->empty()) return genGrammarSimple();

    auto fn = corpus_->randomFunction();

    // Simple type swap in the source
    std::string mutated = fn.fullSource;

    // Replace some int32 with int64 or vice versa
    if (randInt(0, 1))
        mutated = std::regex_replace(mutated, std::regex("int32"), "int64");
    else
        mutated = std::regex_replace(mutated, std::regex("int64"), "int32");

    return wrapProgram(mutated);
}

// =========================================================================
// Main generate() dispatch
// =========================================================================

std::string Mutator::generate(GenStrategy s) {
    if (s == GenStrategy::Count) {
        s = static_cast<GenStrategy>(randInt(0, static_cast<int>(GenStrategy::Count) - 1));
    }

    lastStrategy_ = s;

    switch (s) {
    case GenStrategy::GrammarSimple:    return genGrammarSimple();
    case GenStrategy::GrammarPointers:  return genGrammarPointers();
    case GenStrategy::GrammarArrays:    return genGrammarArrays();
    case GenStrategy::GrammarGenerics:  return genGrammarGenerics();
    case GenStrategy::GrammarControlFlow: return genGrammarControlFlow();
    case GenStrategy::GrammarFull:      return genGrammarFull();

    case GenStrategy::EdgeMixedArith:   return genEdgeMixedArith();
    case GenStrategy::EdgeCastChain:    return genEdgeCastChain();
    case GenStrategy::EdgePtrArith:     return genEdgePtrArith();
    case GenStrategy::EdgeArrayEdge:    return genEdgeArrayEdge();
    case GenStrategy::EdgeGenericEdge:  return genEdgeGenericEdge();
    case GenStrategy::EdgeOwnership:    return genEdgeOwnership();
    case GenStrategy::EdgeMatchEnum:    return genEdgeMatchEnum();
    case GenStrategy::EdgeConstExpr:    return genEdgeConstExpr();
    case GenStrategy::EdgeControlFlow:  return genEdgeControlFlow();
    case GenStrategy::EdgeConcurrency:  return genEdgeConcurrency();
    case GenStrategy::EdgeStdlib:       return genEdgeStdlib();
    case GenStrategy::EdgeNestedExpr:   return genEdgeNestedExpr();
    case GenStrategy::EdgeCastSignedness: return genEdgeCastSignedness();

    case GenStrategy::CorpusMutate:     return genCorpusMutate();
    case GenStrategy::CorpusSplice:     return genCorpusSplice();
    case GenStrategy::CorpusTypeSwap:   return genCorpusTypeSwap();

    default: return genGrammarSimple();
    }
}

// =========================================================================
// Strategy names
// =========================================================================

std::string_view Mutator::strategyName(size_t idx) const {
    static const std::string_view names[] = {
        "grammar_simple", "grammar_pointers", "grammar_arrays",
        "grammar_generics", "grammar_control_flow", "grammar_full",
        "edge_mixed_arith", "edge_cast_chain", "edge_ptr_arith",
        "edge_array_edge", "edge_generic_edge", "edge_ownership",
        "edge_match_enum", "edge_const_expr", "edge_control_flow",
        "edge_concurrency", "edge_stdlib", "edge_nested_expr",
        "edge_cast_signedness",
        "corpus_mutate", "corpus_splice", "corpus_type_swap",
    };
    if (idx < static_cast<size_t>(GenStrategy::Count))
        return names[idx];
    return "unknown";
}

Mutator::Mutator() : rng_(std::random_device{}()) {}
