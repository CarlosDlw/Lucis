#include "lsp/ProjectContext.h"
#include "ffi/CHeaderResolver.h"
#include "config/LucisConfig.h"
#include "imports/ImportResolver.h"

#include <filesystem>
#include <deque>
#include <unordered_set>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════
//  Build project context
// ═══════════════════════════════════════════════════════════════════════

bool ProjectContext::build(const std::string& filePath) {
    valid_ = false;
    units_.clear();
    fileModulePaths_.clear();
    registry_ = ModuleRegistry();
    cBindings_ = CBindings();
    cTypeReg_ = TypeRegistry();

    projectRoot_ = findProjectRoot(filePath);
    if (projectRoot_.empty()) return false;

    // Load source paths from lucis.yaml
    sourcePaths_.clear();
    if (auto config = LucisConfig::load(projectRoot_ + "/lucis.yaml"))
        sourcePaths_ = config->sourcePaths;

    // Build search directories: project root + stdlib paths
    std::vector<std::string> searchDirs;
    searchDirs.push_back(projectRoot_);
    for (auto& p : ImportResolver::stdlibSearchPaths())
        searchDirs.push_back(p);

    // BFS import resolution starting from the given file
    std::unordered_set<std::string> visited;
    // Queue stores (filePath, logicalModulePath, importChain)
    // importChain tracks the path from entry to current module for cycle detection
    auto entryModPath = fs::relative(filePath, projectRoot_).replace_extension("").string();
    std::deque<std::tuple<std::string, std::string, std::vector<std::string>>> queue;
    queue.emplace_back(filePath, entryModPath, std::vector<std::string>{entryModPath});
    visited.insert(filePath);

    while (!queue.empty()) {
        auto [curPath, modPath, importChain] = std::move(queue.front());
        queue.pop_front();

        SourceUnit unit;
        unit.filePath    = curPath;
        unit.modulePath  = modPath;

        // Use persistent stdlib cache to avoid re-parsing stdlib on every rebuild.
        unit.parseResult = getStdlibParse(curPath);
        if (!unit.parseResult) {
            try {
                unit.parseResult = std::make_shared<ParseResult>(Parser::parse(curPath));
            } catch (const std::exception& e) {
                std::cerr << "[lucis-lsp] parse error in " << curPath
                          << ": " << e.what() << "\n";
                continue;
            }
        }

        if (!unit.parseResult->tree)
            continue;

        registry_.registerFile(unit.modulePath, curPath, unit.parseResult->tree, unit.parseResult);
        fileModulePaths_[curPath] = unit.modulePath;
        try {
            fileModulePaths_[fs::canonical(curPath).string()] = unit.modulePath;
        } catch (...) {}

        // Resolve C headers from this file
        std::vector<LucisParser::IncludeDeclContext*> includes;
        for (auto* pre : unit.parseResult->tree->preambleDecl())
            if (auto* inc = pre->includeDecl()) includes.push_back(inc);
        if (!includes.empty()) {
            CHeaderResolver resolver(cTypeReg_, cBindings_);
            for (auto* incl : includes) {
                auto text = incl->getText();
                if (incl->INCLUDE_SYS()) {
                    auto header = CHeaderResolver::extractSystemHeader(text);
                    if (!header.empty())
                        resolver.resolveSystemHeader(header);
                } else if (incl->INCLUDE_LOCAL()) {
                    auto header = CHeaderResolver::extractLocalHeader(text);
                    if (!header.empty()) {
                        auto dir = fs::path(curPath).parent_path().string();
                        resolver.resolveLocalHeader(header, dir);
                    }
                }
            }
        }

        units_.push_back(std::move(unit));

        // Extract use declarations and enqueue their module files
        auto enqueueUse = [&](LucisParser::UseDeclContext* use) {
            std::string usePath;
            if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(use)) {
                usePath = root->IDENTIFIER()->getText();
            } else if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(use)) {
                for (auto* id : item->modulePath()->IDENTIFIER())
                    usePath += (usePath.empty() ? "" : "::") + id->getText();
            } else if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(use)) {
                for (auto* id : group->modulePath()->IDENTIFIER())
                    usePath += (usePath.empty() ? "" : "::") + id->getText();
            } else {
                return;
            }
            auto modFile = resolveUseToFile(usePath);
            if (modFile.empty()) return;

            auto logicalPath = ModuleRegistry::usePathToModulePath(usePath);

            // Circular import detection: check if this module is already in the current chain
            if (std::find(importChain.begin(), importChain.end(), logicalPath) != importChain.end()) {
                std::string chain;
                for (auto& m : importChain) chain += m + " → ";
                chain += logicalPath;
                // Get position from the `use` statement
                size_t errLine = 0, errCol = 0;
                if (auto* start = use->getStart()) {
                    errLine = start->getLine() > 0 ? start->getLine() - 1 : 0;
                    errCol  = start->getCharPositionInLine();
                }
                importErrors_.push_back({
                    "circular import detected: " + chain,
                    curPath,
                    errLine,
                    errCol
                });
                return;
            }

            if (visited.insert(modFile).second) {
                auto childChain = importChain;
                childChain.push_back(logicalPath);
                queue.emplace_back(modFile, logicalPath, std::move(childChain));
            }
        };

        for (auto* pre : unit.parseResult->tree->preambleDecl()) {
            if (auto* use = pre->useDecl())
                enqueueUse(use);
        }
        for (auto* top : unit.parseResult->tree->topLevelDecl()) {
            if (auto* use = top->useDecl())
                enqueueUse(use);
        }
    }

    valid_ = !units_.empty();
    return valid_;
}

std::string ProjectContext::modulePathFor(const std::string& filePath) const {
    // Try exact match first.
    auto it = fileModulePaths_.find(filePath);
    if (it != fileModulePaths_.end()) return it->second;

    // Try canonical path.
    try {
        auto canon = fs::canonical(filePath).string();
        it = fileModulePaths_.find(canon);
        if (it != fileModulePaths_.end()) return it->second;
    } catch (...) {}

    // Try weakly-canonical path.
    try {
        auto weak = fs::weakly_canonical(filePath).string();
        it = fileModulePaths_.find(weak);
        if (it != fileModulePaths_.end()) return it->second;
    } catch (...) {}

    // Fallback: match by filename.
    try {
        auto name = fs::path(filePath).filename().string();
        if (!name.empty()) {
            for (const auto& [p, mp] : fileModulePaths_) {
                if (fs::path(p).filename() == name)
                    return mp;
            }
        }
    } catch (...) {}

    return "";
}

std::shared_ptr<ParseResult> ProjectContext::getStdlibParse(const std::string& filePath) const {
    auto it = stdlibCache_.find(filePath);
    if (it != stdlibCache_.end()) return it->second;

    bool isStdlib = false;
    for (auto& dir : ImportResolver::stdlibSearchPaths()) {
        if (filePath.rfind(dir, 0) == 0) { isStdlib = true; break; }
    }
    if (!isStdlib) return nullptr;

    std::shared_ptr<ParseResult> pr;
    try {
        pr = std::make_shared<ParseResult>(Parser::parse(filePath));
    } catch (const std::exception& e) {
        std::cerr << "[lucis-lsp] parse error in stdlib file " << filePath
                  << ": " << e.what() << "\n";
        return nullptr;
    }
    if (pr->tree) stdlibCache_[filePath] = pr;
    return pr;
}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

std::string ProjectContext::findProjectRoot(const std::string& filePath) {
    static const std::vector<std::string> kMarkers = {
        "lucis.yaml", "CMakeLists.txt", "Makefile", "makefile", ".git", ".hg", ".svn"
    };

    try {
        auto dir = fs::canonical(fs::path(filePath).parent_path());

        while (dir.has_parent_path() && dir != dir.parent_path()) {
            for (const auto& marker : kMarkers) {
                std::error_code ec;
                if (fs::exists(dir / marker, ec) && !ec)
                    return dir.string();
            }
            dir = dir.parent_path();
        }

        return fs::canonical(fs::path(filePath).parent_path()).string();
    } catch (...) {
        return fs::path(filePath).parent_path().string();
    }
}

std::string ProjectContext::resolveUseToFile(const std::string& useIdent) const {
    // Convert "std::log" → "std/log"
    std::string modPath;
    {
        std::string tmp = useIdent;
        for (auto& c : tmp) if (c == ':') c = '/';
        for (size_t i = 0; i < tmp.size(); i++) {
            if (tmp[i] == '/' && i + 1 < tmp.size() && tmp[i+1] == '/') {
                modPath += '/';
                i++;
                continue;
            }
            modPath += tmp[i];
        }
    }

    // Build search directories: project root, source paths, stdlib
    std::vector<fs::path> searchPaths;
    searchPaths.push_back(fs::path(projectRoot_));
    for (auto& sp : sourcePaths_)
        searchPaths.push_back(fs::path(projectRoot_) / sp);
    for (auto& p : ImportResolver::stdlibSearchPaths())
        searchPaths.push_back(fs::path(p));

    for (auto& base : searchPaths) {
        auto candidate = base / (modPath + ".lc");
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec)
            return fs::canonical(candidate, ec).string();
    }
    return {};
}
