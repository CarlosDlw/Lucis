#pragma once

#include <string>
#include <vector>

// Recursively scans a project directory for .lx source files.
// Includes hidden directories and skips only the .luxbuild directory.
class ProjectScanner {
public:
    static std::vector<std::string> scan(const std::string& rootDir);
};
