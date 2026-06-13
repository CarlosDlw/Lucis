#pragma once

#include <string>
#include <vector>

// Recursively scans a project directory for .lc source files.
// If sourcePaths is non-empty, only those subdirectories (relative to rootDir)
// are scanned. Otherwise the entire rootDir is scanned recursively.
class ProjectScanner {
public:
    static std::vector<std::string> scan(
        const std::string& rootDir,
        const std::vector<std::string>& sourcePaths = {});
};
