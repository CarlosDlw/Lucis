#include "namespace/ProjectScanner.h"

#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::vector<std::string> ProjectScanner::scan(const std::string& rootDir) {
    std::vector<std::string> files;

    if (!fs::exists(rootDir) || !fs::is_directory(rootDir)) {
        return files;
    }

    for (auto it = fs::recursive_directory_iterator(
             rootDir, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator(); ++it) {

        // Skip known non-project directories to avoid mixing unrelated
        // .lc files into the same project context.
        if (it->is_directory()) {
            auto dirName = it->path().filename().string();
            if (dirName == ".lucis" || dirName == "build" ||
                dirName == ".git" || dirName == ".svn" || dirName == ".hg") {
                it.disable_recursion_pending();
                continue;
            }
        }

        if (!it->is_regular_file()) continue;

        if (it->path().extension() == ".lc") {
            files.push_back(fs::canonical(it->path()).string());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}
