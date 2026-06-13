#include "namespace/ProjectScanner.h"

#include <filesystem>
#include <algorithm>
#include <unordered_set>

namespace fs = std::filesystem;

std::vector<std::string> ProjectScanner::scan(
        const std::string& rootDir,
        const std::vector<std::string>& sourcePaths) {

    std::vector<std::string> files;

    if (!fs::exists(rootDir) || !fs::is_directory(rootDir)) {
        return files;
    }

    // Build the set of directories to scan.
    // If sourcePaths is non-empty, only those subdirectories are scanned;
    // otherwise the entire rootDir is scanned recursively.
    std::vector<fs::path> scanRoots;
    if (sourcePaths.empty()) {
        scanRoots.push_back(rootDir);
    } else {
        for (const auto& sp : sourcePaths) {
            auto p = fs::path(rootDir) / sp;
            std::error_code ec;
            if (fs::exists(p, ec) && !ec)
                scanRoots.push_back(fs::canonical(p, ec));
        }
    }

    // Collect exclusions for fast lookup: stop recursion into these directories.
    static const std::unordered_set<std::string> kSkipDirs = {
        ".lucis", "build", ".git", ".svn", ".hg"
    };

    for (const auto& root : scanRoots) {
        if (!fs::is_directory(root)) continue;

        for (auto it = fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {

            if (it->is_directory()) {
                auto dirName = it->path().filename().string();
                if (kSkipDirs.count(dirName)) {
                    it.disable_recursion_pending();
                    continue;
                }
            }

            if (!it->is_regular_file()) continue;

            if (it->path().extension() == ".lc") {
                files.push_back(fs::canonical(it->path()).string());
            }
        }
    }

    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}
