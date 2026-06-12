#include "lsp/ProjectContext.h"
#include "namespace/ProjectScanner.h"
#include "ffi/CHeaderResolver.h"

#include <filesystem>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════════════
//  Build project context
// ═══════════════════════════════════════════════════════════════════════

bool ProjectContext::build(const std::string& filePath) {
    valid_ = false;
    units_.clear();
    fileNamespaces_.clear();
    registry_ = NamespaceRegistry();
    cBindings_ = CBindings();
    cTypeReg_ = TypeRegistry();

    projectRoot_ = findProjectRoot(filePath);
    if (projectRoot_.empty()) return false;

    // Scan all .lc files in the project.
    auto allFiles = ProjectScanner::scan(projectRoot_);
    if (allFiles.empty()) return false;

    // Parse all files and build the registry.
    for (auto& path : allFiles) {
        SourceUnit unit;
        unit.filePath    = path;
        unit.parseResult = Parser::parse(path);

        // Keep files with recoverable parse errors in the symbol registry to
        // avoid cascading "undeclared function" diagnostics in other files.
        if (!unit.parseResult.tree)
            continue;

        unit.namespaceName = extractNamespace(unit.parseResult.tree);
        if (unit.namespaceName.empty())
            continue;

        registry_.registerFile(unit.namespaceName, path,
                               unit.parseResult.tree);
        fileNamespaces_[path] = unit.namespaceName;
        try {
            fileNamespaces_[fs::canonical(path).string()] = unit.namespaceName;
        } catch (...) {}

        // Resolve C headers from this file.
        std::vector<LucisParser::IncludeDeclContext*> includes;
        for (auto* pre : unit.parseResult.tree->preambleDecl())
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
                        auto dir = fs::path(path).parent_path().string();
                        resolver.resolveLocalHeader(header, dir);
                    }
                }
            }
        }

        units_.push_back(std::move(unit));
    }

    valid_ = true;
    return true;
}

std::string ProjectContext::namespaceFor(const std::string& filePath) const {
    // Try exact match first.
    auto it = fileNamespaces_.find(filePath);
    if (it != fileNamespaces_.end()) return it->second;

    // Try canonical path.
    try {
        auto canon = fs::canonical(filePath).string();
        it = fileNamespaces_.find(canon);
        if (it != fileNamespaces_.end()) return it->second;
    } catch (...) {}

    // Try weakly-canonical path (works even if some path components are missing).
    try {
        auto weak = fs::weakly_canonical(filePath).string();
        it = fileNamespaces_.find(weak);
        if (it != fileNamespaces_.end()) return it->second;
    } catch (...) {}

    // Last-resort fallback: match by filename when path representations differ.
    try {
        auto name = fs::path(filePath).filename().string();
        if (!name.empty()) {
            for (const auto& [p, ns] : fileNamespaces_) {
                if (fs::path(p).filename() == name)
                    return ns;
            }
        }
    } catch (...) {}

    return "";
}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

std::string ProjectContext::findProjectRoot(const std::string& filePath) {
    // Walk up from the file's directory looking for an explicit project marker.
    // Using project markers (CMakeLists.txt, Makefile, .git, etc.) is far more
    // reliable than scanning for .lc files, which can cause runaway traversal
    // into system directories like /proc when no marker is found.
    static const std::vector<std::string> kMarkers = {
        "CMakeLists.txt", "Makefile", "makefile", ".git", ".hg", ".svn"
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

        // No marker found — fall back to the file's own directory.
        return fs::canonical(fs::path(filePath).parent_path()).string();
    } catch (...) {
        return fs::path(filePath).parent_path().string();
    }
}

std::string ProjectContext::extractNamespace(LucisParser::ProgramContext* tree) {
    if (!tree) return "";
    auto* nsDecl = tree->namespaceDecl();
    if (!nsDecl) return "";
    return nsDecl->IDENTIFIER()->getText();
}
