#include "corpus.hpp"

#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

void Corpus::loadDir(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) {
        std::cerr << "[corpus] directory not found: " << dir << "\n";
        return;
    }

    size_t count = 0;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".lc")
            continue;

        std::ifstream f(entry.path());
        if (!f) continue;

        std::stringstream ss;
        ss << f.rdbuf();
        entries_.push_back({entry.path().string(), ss.str()});
        count++;
    }
    std::cerr << "[corpus] loaded " << count << " files from " << dir << "\n";
}

const CorpusEntry& Corpus::randomEntry() const {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, entries_.size() - 1);
    return entries_[dist(rng)];
}
