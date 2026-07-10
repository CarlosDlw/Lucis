#include "mutator.hpp"

#include <algorithm>
#include <cstdlib>
#include <regex>
#include <sstream>

// ---------------------------------------------------------------------------
// Mutation strategies — each takes source text, returns a mutated copy.
// ---------------------------------------------------------------------------

/// Replace a random integer literal with a different type suffix or remove it.
static std::string mutChangeIntType(std::string_view src, std::mt19937& rng) {
    static const std::regex intPat(R"(\b(\d+)(u(?:size|8|16|32|64|128))?\b)");
    static const char* suffixes[] = {
        "", "u8", "u16", "u32", "u64", "usize",
        "i8", "i16", "i32", "i64", "isize"
    };
    std::string result(src);
    std::smatch m;
    std::string::const_iterator it = result.cbegin();

    // Collect all matches
    struct Match { ptrdiff_t pos; size_t len; std::string suffix; };
    std::vector<Match> matches;
    while (std::regex_search(it, result.cend(), m, intPat)) {
        ptrdiff_t pos = m.position() + (it - result.cbegin());
        matches.push_back({pos, static_cast<size_t>(m.length()), m[2].str()});
        it = m.suffix().first;
    }
    if (matches.empty()) return result;

    std::uniform_int_distribution<size_t> pick(0, matches.size() - 1);
    auto& m2 = matches[pick(rng)];

    // Re-parse the number without suffix
    std::string num = result.substr(m2.pos, m2.len);
    // Strip existing suffix
    auto uPos = num.find_first_of("ui");
    if (uPos != std::string::npos) num = num.substr(0, uPos);

    // Pick a new suffix (maybe same)
    std::uniform_int_distribution<size_t> sfx(0, sizeof(suffixes)/sizeof(suffixes[0]) - 1);
    std::string newSuffix = suffixes[sfx(rng)];

    std::string replacement = num + newSuffix;
    result.replace(m2.pos, m2.len, replacement);
    return result;
}

/// Change `as Type` to a different type or remove it.
static std::string mutChangeCast(std::string_view src, std::mt19937& rng) {
    static const std::regex castPat(R"(\bas\s+(int\d+|uint\d+|usize|isize|float\d+)\b)");
    static const char* types[] = {
        "int32", "int64", "uint32", "uint64", "usize", "isize", "float64"
    };
    std::string result(src);
    std::smatch m;
    if (!std::regex_search(result, m, castPat))
        return result;

    std::uniform_int_distribution<size_t> pick(0, sizeof(types)/sizeof(types[0]) - 1);
    std::string newType = types[pick(rng)];
    result.replace(m.position(), m.length(), "as " + newType);
    return result;
}

/// Remove an `as` cast entirely (e.g. `x as int32` → `x`).
static std::string mutRemoveCast(std::string_view src, std::mt19937& rng) {
    static const std::regex castPat(R"(\s+as\s+(int\d+|uint\d+|usize|isize|float\d+))");
    std::string result(src);
    std::smatch m;
    if (!std::regex_search(result, m, castPat))
        return result;
    result.erase(m.position(), m.length());
    return result;
}

/// Swap binary operator (e.g. + → -, * → /, & → |).
static std::string mutSwapOperator(std::string_view src, std::mt19937& rng) {
    static const std::regex opPat(R"(\s*([+\-*/%&|^])\s*)");
    std::string result(src);
    // Find all operator occurrences in context that look like binary ops
    // by scanning around word/brace boundaries.
    std::sregex_iterator it(result.begin(), result.end(), opPat);
    std::sregex_iterator end;
    std::vector<ptrdiff_t> positions;
    for (auto i = it; i != end; ++i) {
        auto pos = i->position(1);
        // Only swap operators that are between identifiers/braces (binary)
        if (pos > 0 && pos < static_cast<ptrdiff_t>(result.size()) - 1) {
            char before = result[pos - 1];
            char after  = result[pos + 1];
            if ((std::isalnum(before) || before == ')' || before == ']') &&
                (std::isalnum(after)  || after == '('  || after == '['))
                positions.push_back(pos);
        }
    }
    if (positions.empty()) return result;

    std::uniform_int_distribution<size_t> pickPos(0, positions.size() - 1);
    size_t opIdx = pickPos(rng);
    size_t opPos = static_cast<size_t>(positions[opIdx]);

    struct OpPair { char from, to; };
    static const OpPair swaps[] = {
        {'+', '-'}, {'-', '+'}, {'*', '/'}, {'/', '*'},
        {'&', '|'}, {'|', '&'}, {'^', '&'},
    };
    char op = result[opPos];
    for (auto& s : swaps) {
        if (s.from == op) {
            result[opPos] = s.to;
            break;
        }
    }
    return result;
}

/// Add a unary +/- prefix to a random number.
static std::string mutAddUnary(std::string_view src, std::mt19937& rng) {
    static const std::regex numPat(R"(\b(\d+)\b)");
    std::string result(src);
    std::sregex_iterator it(result.begin(), result.end(), numPat);
    std::sregex_iterator end;
    std::vector<std::pair<ptrdiff_t, ptrdiff_t>> candidates;
    for (auto i = it; i != end; ++i) {
        auto start = i->position(1);
        auto len   = i->length(1);
        // Skip if followed by something that indicates non-binary context
        ptrdiff_t after = start + len;
        if (after < static_cast<ptrdiff_t>(result.size())) {
            char next = result[static_cast<size_t>(after)];
            if (next == '.' || next == ']' || next == ')' ||
                std::isalnum(next) || next == '_')
                continue;
        }
        candidates.push_back({start, len});
    }
    if (candidates.empty()) return result;

    std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
    auto [pos, len] = candidates[pick(rng)];
    static const char* prefixes[] = {"-", "+"};
    std::uniform_int_distribution<size_t> pickPfx(0, 1);
    result.insert(static_cast<size_t>(pos), prefixes[pickPfx(rng)]);
    return result;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
std::vector<Mutator::StrategyFn> Mutator::strategies_ = {
    mutChangeIntType,
    mutChangeCast,
    mutRemoveCast,
    mutSwapOperator,
    mutAddUnary,
};

std::vector<std::string_view> Mutator::strategyNames_ = {
    "change_int_type",
    "change_cast_type",
    "remove_cast",
    "swap_operator",
    "add_unary",
};

// ---------------------------------------------------------------------------
// Mutator implementation
// ---------------------------------------------------------------------------
Mutator::Mutator() : rng_(std::random_device{}()) {}

std::string Mutator::mutate(std::string_view source) {
    std::uniform_int_distribution<size_t> pickStrat(0, strategies_.size() - 1);
    size_t idx = pickStrat(rng_);
    return strategies_[idx](source, rng_);
}

std::string_view Mutator::strategyName(size_t idx) const {
    return idx < strategyNames_.size() ? strategyNames_[idx] : "unknown";
}
