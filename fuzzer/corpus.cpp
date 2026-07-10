#include "corpus.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

void Corpus::loadDir(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::cerr << "[corpus] dir not found: " << dir << "\n";
        return;
    }
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".lc") continue;
        std::ifstream f(entry.path());
        if (!f) continue;
        CorpusEntry ce;
        ce.path = entry.path().string();
        ce.source.assign(std::istreambuf_iterator<char>(f),
                         std::istreambuf_iterator<char>());
        ce.label = entry.path().stem().string();
        entries_.push_back(std::move(ce));
    }
    std::cerr << "[corpus] loaded " << entries_.size() << " files from " << dir << "\n";
}

const CorpusEntry& Corpus::randomEntry() {
    std::uniform_int_distribution<size_t> d(0, entries_.size() - 1);
    return entries_[d(rng_)];
}

const std::string& Corpus::randomSource() {
    return randomEntry().source;
}

// ── Fragment extraction ───────────────────────────────────────────

void Corpus::extractFragments() {
    if (fragmentsExtracted_) return;
    for (auto& e : entries_)
        extractFragmentsFrom(e.source);
    fragmentsExtracted_ = true;
}

void Corpus::extractFragmentsFrom(const std::string& source) {
    // Extract function declarations: fn name(params) retType { body }
    // We use a pragmatic regex that handles single-line fn signatures
    // followed by a balanced-brace block.
    static const std::regex fnRe(
        R"((fn\s+(\w+)\s*\([^)]*\)\s*\w+\s*\{))",
        std::regex::optimize
    );

    std::string::size_type searchPos = 0;
    while (searchPos < source.size()) {
        auto fnPos = source.find("fn ", searchPos);
        if (fnPos == std::string::npos) break;

        // Find opening paren
        auto openParen = source.find('(', fnPos);
        if (openParen == std::string::npos) { searchPos = fnPos + 3; continue; }

        // Find matching closing paren (simple: no nested parens in signature)
        auto closeParen = source.find(')', openParen);
        if (closeParen == std::string::npos) { searchPos = openParen + 1; continue; }

        // Find opening brace
        auto openBrace = source.find('{', closeParen);
        if (openBrace == std::string::npos) { searchPos = closeParen + 1; continue; }

        // Match braces robustly (count depth)
        int depth = 1;
        auto closeBrace = openBrace + 1;
        while (closeBrace < source.size() && depth > 0) {
            if (source[closeBrace] == '{') depth++;
            else if (source[closeBrace] == '}') depth--;
            if (depth > 0) closeBrace++;
        }
        if (depth != 0) { searchPos = openBrace + 1; continue; }

        // Extract the function name
        auto nameStart = fnPos + 3; // skip "fn "
        while (nameStart < source.size() && source[nameStart] == ' ') nameStart++;
        auto nameEnd = nameStart;
        while (nameEnd < openParen && (std::isalnum(source[nameEnd]) || source[nameEnd] == '_'))
            nameEnd++;

        // Extract signature (fn name(params) retType)
        auto sigEnd = openBrace;
        while (sigEnd > fnPos && source[sigEnd - 1] == ' ') sigEnd--;
        std::string sig = source.substr(fnPos, sigEnd - fnPos);

        // Extract return type (last word before brace)
        std::string retType;
        auto lastSpace = sig.rfind(' ');
        if (lastSpace != std::string::npos)
            retType = sig.substr(lastSpace + 1);

        std::string name = source.substr(nameStart, nameEnd - nameStart);
        std::string body = source.substr(openBrace, closeBrace - openBrace + 1);
        std::string full = sig + " " + body;

        FunctionFragment ff;
        ff.name = name;
        ff.signature = sig;
        ff.returnType = retType;
        ff.body = body;
        ff.fullSource = full;
        cachedFunctions_.push_back(std::move(ff));

        // Check if there's also a type declaration (struct/union/enum) nearby
        // Search backwards from fnPos for struct/enum/union
        auto searchStart = (fnPos > 200) ? fnPos - 200 : 0;
        std::string_view prelude(&source[searchStart], fnPos - searchStart);

        // We use a simpler approach: just find the last struct/union/enum decl
        auto typePos = prelude.rfind("struct ");
        if (typePos == std::string::npos) typePos = prelude.rfind("union ");
        if (typePos == std::string::npos) typePos = prelude.rfind("enum ");
        if (typePos != std::string::npos) {
            // Find the type name
            auto tnStart = typePos;
            while (tnStart < prelude.size() && prelude[tnStart] != ' ') tnStart++;
            while (tnStart < prelude.size() && prelude[tnStart] == ' ') tnStart++;
            auto tnEnd = tnStart;
            while (tnEnd < prelude.size() && (std::isalnum(prelude[tnEnd]) || prelude[tnEnd] == '_'))
                tnEnd++;
            std::string typeName(prelude.substr(tnStart, tnEnd - tnStart));
            // Find the closing brace of the type decl
            auto openBrace2 = prelude.find('{', typePos);
            if (openBrace2 != std::string::npos) {
                int d2 = 1;
                auto closeBrace2 = openBrace2 + 1;
                while (closeBrace2 < prelude.size() && d2 > 0) {
                    if (prelude[closeBrace2] == '{') d2++;
                    else if (prelude[closeBrace2] == '}') d2--;
                    if (d2 > 0) closeBrace2++;
                }
                if (d2 == 0) {
                    std::string fullTypeDecl(prelude.substr(typePos, closeBrace2 - typePos + 1));
                    TypeFragment tf;
                    tf.name = typeName;
                    tf.decl = fullTypeDecl;
                    // Avoid duplicates
                    bool dup = false;
                    for (auto& existing : cachedTypes_) {
                        if (existing.name == typeName) { dup = true; break; }
                    }
                    if (!dup)
                        cachedTypes_.push_back(std::move(tf));
                }
            }
        }

        searchPos = closeBrace + 1;
    }
}

FunctionFragment Corpus::randomFunction() {
    if (cachedFunctions_.empty()) {
        return {"test_fuzz", "fn test_fuzz() void", "void", "{}", "fn test_fuzz() void {}"};
    }
    std::uniform_int_distribution<size_t> d(0, cachedFunctions_.size() - 1);
    return cachedFunctions_[d(rng_)];
}

TypeFragment Corpus::randomType() {
    extractFragments();
    if (cachedTypes_.empty()) return {"", ""};
    std::uniform_int_distribution<size_t> d(0, cachedTypes_.size() - 1);
    return cachedTypes_[d(rng_)];
}

std::string Corpus::randomLine() {
    if (entries_.empty()) return "";
    auto& src = randomSource();
    std::istringstream stream(src);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line))
        lines.push_back(line);
    if (lines.empty()) return "";
    std::uniform_int_distribution<size_t> d(0, lines.size() - 1);
    return lines[d(rng_)];
}
