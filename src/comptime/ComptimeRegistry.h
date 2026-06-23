#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// Stores all comptime function declarations discovered during parsing.
// Queried by the checker, IRGen, and LSP.
// Uses void* for FunctionDeclContext to avoid including the ANTLR header.
class ComptimeRegistry {
public:
    struct Entry {
        std::string name;
        void* decl = nullptr; // LucisParser::FunctionDeclContext*
        bool isGeneric = false;
    };

    void registerFunction(const std::string& name,
                          void* decl,
                          bool isGeneric = false);

    bool isComptime(const std::string& name) const;

    void* lookup(const std::string& name) const;

    std::vector<std::string> allNames() const;

    size_t count() const { return entries_.size(); }

    void clear();

private:
    std::unordered_map<std::string, Entry> entries_;
};
