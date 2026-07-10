#pragma once

#include "corpus.hpp"

#include <string>
#include <string_view>
#include <random>
#include <vector>
#include <map>

// =========================================================================
// Generation strategy identifiers
// =========================================================================
enum class GenStrategy : uint8_t {
    // Grammar-aware generation
    GrammarSimple,        // Simple functions with literals
    GrammarPointers,      // Pointer-heavy code
    GrammarArrays,        // Array-heavy code
    GrammarGenerics,      // Generic types and functions
    GrammarControlFlow,   // if/for/while/match/switch
    GrammarFull,          // Full grammar walk (most complex)

    // Targeted edge cases
    EdgeMixedArith,       // signed + unsigned mixing
    EdgeCastChain,        // multiple chained casts
    EdgePtrArith,         // pointer + offset, ptrdiff
    EdgeArrayEdge,        // zero-size, negative index, bounds
    EdgeGenericEdge,      // wrong arity, recursive, nested
    EdgeOwnership,        // move/borrow/use-after-move
    EdgeMatchEnum,        // non-exhaustive match, wrong variant
    EdgeConstExpr,        // comptime evaluation edge cases
    EdgeControlFlow,      // unreachable code, break/continue edge
    EdgeConcurrency,      // spawn/await edge cases
    EdgeStdlib,           // std::string, std::vec edge cases
    EdgeNestedExpr,       // deeply nested expressions
    EdgeCastSignedness,   // sign extension, truncation

    // Corpus mutation
    CorpusMutate,         // random corpus fragment mutation
    CorpusSplice,         // splice two corpus functions
    CorpusTypeSwap,       // swap types in a corpus function

    Count,
};

class Mutator {
public:
    Mutator();

    /// Set the corpus for mutation-based strategies
    void setCorpus(Corpus* corpus) { corpus_ = corpus; }

    /// Generate one complete, syntactically valid Lucis program
    /// using the given strategy (or random if Count).
    std::string generate(GenStrategy s = GenStrategy::Count);

    /// Return metadata about the last generated strategy
    GenStrategy lastStrategy() const { return lastStrategy_; }

    size_t strategyCount() const { return static_cast<size_t>(GenStrategy::Count); }
    std::string_view strategyName(size_t idx) const;

private:
    std::mt19937 rng_;
    Corpus* corpus_ = nullptr;
    GenStrategy lastStrategy_ = GenStrategy::GrammarSimple;

    // ── RNG helpers ────────────────────────────────────────────────
    int randInt(int min, int max);
    template<typename T> T pick(const std::vector<T>& vec);

    // ── Type system (from grammar) ─────────────────────────────────
    enum class BaseType : uint8_t {
        Int1, Int8, Int16, Int32, Int64, Int128, IntInf, ISize,
        UInt1, UInt8, UInt16, UInt32, UInt64, UInt128, USize,
        Float32, Float64, Float80, Float128, Double,
        Bool, Char, Void, String, CString
    };

    struct LucisType {
        BaseType base;
        bool isPointer = false;
        bool isArray = false;
        unsigned arraySize = 0;
        bool isUnsizedArray = false;
        bool isVec = false;
        bool isMap = false;
        bool isSet = false;
        bool isTuple = false;
        bool isGeneric = false;
        std::string genericName;     // for user-defined generics
        std::vector<LucisType> typeArgs;
        LucisType* pointee = nullptr;

        std::string str() const;

        LucisType() = default;
        LucisType(BaseType b) : base(b) {}
        LucisType(const LucisType& o);
        LucisType& operator=(const LucisType& o);
        ~LucisType();

        bool isInteger() const;
        bool isSigned() const;
        bool isUnsigned() const;
        bool isFloat() const;
        bool isNumeric() const;
    };

    LucisType randomBaseType();
    LucisType randomIntType();
    LucisType randomSignedInt();
    LucisType randomUnsignedInt();
    LucisType randomFloatType();
    LucisType randomScalar();
    LucisType randomFullType(int depth = 0);
    LucisType randomGenericType(int depth = 0);

    std::string typeLit(const LucisType& t);

    // ── Literal generation ─────────────────────────────────────────
    std::string randomIntLit(const LucisType& t);
    std::string randomBoolLit();
    std::string randomCharLit();
    std::string randomFloatLit(const LucisType& t);
    std::string randomLit(const LucisType& t);

    // ── Expression generation (grammar-aware) ──────────────────────
    struct VarInfo {
        std::string name;
        LucisType type;
    };

    struct Scope {
        std::vector<VarInfo> vars;
        int depth = 0;
    };

    std::string genExpr(const LucisType& desired, int depth = 0);
    std::string genIdentExpr(const LucisType& desired);
    std::string genUnaryExpr(const LucisType& desired, int depth);
    std::string genBinaryExpr(const LucisType& desired, int depth);
    std::string genCastExpr(const LucisType& desired, int depth);
    std::string genCallExpr(const LucisType& desired, int depth);
    std::string genTernaryExpr(const LucisType& desired, int depth);
    std::string genParenExpr(const LucisType& desired, int depth);

    // ── Statement generation ───────────────────────────────────────
    std::string genVarDecl(const LucisType& t);
    std::string genAssign(const std::string& varName, const LucisType& t);
    std::string genReturn(const LucisType& t);
    std::string genIf(const LucisType& retType);
    std::string genFor(const LucisType& retType);
    std::string genWhile(const LucisType& retType);
    std::string genStatement(const LucisType& retType, int depth = 0);
    std::string genBlock(const LucisType& retType, int stmtCount, int depth = 0);

    // ── Scope management ───────────────────────────────────────────
    Scope& currentScope();
    std::string pushVar(const std::string& name, const LucisType& type);
    std::string freshVarName();
    std::vector<Scope> scopes_;
    int varCounter_ = 0;

    // ── Generation strategies ──────────────────────────────────────
    std::string genGrammarSimple();
    std::string genGrammarPointers();
    std::string genGrammarArrays();
    std::string genGrammarGenerics();
    std::string genGrammarControlFlow();
    std::string genGrammarFull();

    std::string genEdgeMixedArith();
    std::string genEdgeCastChain();
    std::string genEdgePtrArith();
    std::string genEdgeArrayEdge();
    std::string genEdgeGenericEdge();
    std::string genEdgeOwnership();
    std::string genEdgeMatchEnum();
    std::string genEdgeConstExpr();
    std::string genEdgeControlFlow();
    std::string genEdgeConcurrency();
    std::string genEdgeStdlib();
    std::string genEdgeNestedExpr();
    std::string genEdgeCastSignedness();

    std::string genCorpusMutate();
    std::string genCorpusSplice();
    std::string genCorpusTypeSwap();

    // ── Wrapping helpers ───────────────────────────────────────────
    std::string wrapProgram(const std::string& body);
};
