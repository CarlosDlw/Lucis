#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct CorpusEntry {
    std::string path;
    std::string source;
};

class Corpus {
public:
    void loadDir(const std::filesystem::path& dir);

    const CorpusEntry& randomEntry() const;

    bool empty() const { return entries_.empty(); }
    size_t size() const { return entries_.size(); }

private:
    std::vector<CorpusEntry> entries_;
};
