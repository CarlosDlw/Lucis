#pragma once

#include <string>
#include <vector>

// Recursively scans a project directory for .lc source files.
// Includes hidden directories and skips only the .lucis directory.
class ProjectScanner {
public:
    static std::vector<std::string> scan(const std::string& rootDir);
};
