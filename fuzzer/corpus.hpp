#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <random>

struct CorpusEntry {
    std::string path;
    std::string source;
    std::string label; // filename without .lc
};

struct FunctionFragment {
    std::string name;
    std::string signature;   // e.g. "fn foo(int32 a, *int32 b)"
    std::string returnType;
    std::string body;
    std::string fullSource;  // just the fn decl + body
};

struct TypeFragment {
    std::string name;   // e.g. "Point", "Result"
    std::string decl;   // e.g. "struct Point { int32 x; int32 y; }"
};

class Corpus {
public:
    void loadDir(const std::filesystem::path& dir);

    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }

    const CorpusEntry& randomEntry();

    /// Extract a random function body from the corpus
    FunctionFragment randomFunction();

    /// Extract a random type declaration from the corpus
    TypeFragment randomType();

    /// Get the full source text of a random entry
    const std::string& randomSource();

    /// Get a random line from a random entry (good for statement stealing)
    std::string randomLine();

private:
    std::mt19937 rng_;
    std::vector<CorpusEntry> entries_;
    std::vector<FunctionFragment> cachedFunctions_;
    std::vector<TypeFragment> cachedTypes_;
    bool fragmentsExtracted_ = false;

    void extractFragments();
    void extractFragmentsFrom(const std::string& source);
};
