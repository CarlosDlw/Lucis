#include "namespace/ModuleRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <sstream>

void ModuleRegistry::registerFile(const std::string& modulePath,
                                   const std::string& filePath,
                                   LucisParser::ProgramContext* tree,
                                   std::shared_ptr<ParseResult> anchor) {
    auto& symbols = modules_[modulePath];

    for (auto* topLevel : tree->topLevelDecl()) {
        if (auto* funcDecl = topLevel->functionDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Function;
            sym.name       = funcDecl->IDENTIFIER(0)->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(funcDecl->getStart()->getLine());
            sym.column     = static_cast<unsigned>(funcDecl->getStart()->getCharPositionInLine());
            sym.decl       = funcDecl;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
        else if (auto* structDecl = topLevel->structDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Struct;
            sym.name       = structDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(structDecl->getStart()->getLine());
            sym.column     = static_cast<unsigned>(structDecl->getStart()->getCharPositionInLine());
            sym.decl       = structDecl;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
        else if (auto* unionDecl = topLevel->unionDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Union;
            sym.name       = unionDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(unionDecl->getStart()->getLine());
            sym.column     = static_cast<unsigned>(unionDecl->getStart()->getCharPositionInLine());
            sym.decl       = unionDecl;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
        else if (auto* enumDecl = topLevel->enumDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Enum;
            sym.name       = enumDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(enumDecl->getStart()->getLine());
            sym.column     = static_cast<unsigned>(enumDecl->getStart()->getCharPositionInLine());
            sym.decl       = enumDecl;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
        else if (auto* typeAlias = topLevel->typeAliasDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::TypeAlias;
            sym.name       = typeAlias->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(typeAlias->getStart()->getLine());
            sym.column     = static_cast<unsigned>(typeAlias->getStart()->getCharPositionInLine());
            sym.decl       = typeAlias;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
        else if (auto* extDecl = topLevel->extendDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::ExtendBlock;
            sym.name       = extDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.line       = static_cast<unsigned>(extDecl->getStart()->getLine());
            sym.column     = static_cast<unsigned>(extDecl->getStart()->getCharPositionInLine());
            sym.decl       = extDecl;
            sym.treeAnchor = anchor;
            symbols.push_back(std::move(sym));
        }
    }
}

std::vector<std::string> ModuleRegistry::validate() const {
    std::vector<std::string> errors;

    for (auto& [mod, symbols] : modules_) {
        std::unordered_map<std::string, const ExportedSymbol*> seen;
        for (auto& sym : symbols) {
            if (sym.kind == ExportedSymbol::ExtendBlock) continue;
            auto it = seen.find(sym.name);
            if (it != seen.end()) {
                errors.push_back(
                    "duplicate symbol '" + sym.name +
                    "' in module '" + mod +
                    "' (defined in '" + it->second->sourceFile +
                    "' and '" + sym.sourceFile + "')");
            } else {
                seen[sym.name] = &sym;
            }
        }
    }
    return errors;
}

const ExportedSymbol* ModuleRegistry::findSymbol(
    const std::string& modulePath, const std::string& name) const {

    auto it = modules_.find(modulePath);
    if (it == modules_.end()) return nullptr;

    for (auto& sym : it->second) {
        if (sym.name == name && sym.kind != ExportedSymbol::ExtendBlock)
            return &sym;
    }
    return nullptr;
}

std::vector<const ExportedSymbol*> ModuleRegistry::getModuleSymbols(
    const std::string& modulePath) const {

    std::vector<const ExportedSymbol*> result;
    auto it = modules_.find(modulePath);
    if (it == modules_.end()) return result;
    for (auto& sym : it->second)
        result.push_back(&sym);
    return result;
}

std::vector<const ExportedSymbol*> ModuleRegistry::getExternalSymbols(
    const std::string& modulePath, const std::string& excludeFile) const {

    std::vector<const ExportedSymbol*> result;
    auto it = modules_.find(modulePath);
    if (it == modules_.end()) return result;
    for (auto& sym : it->second) {
        if (sym.sourceFile != excludeFile)
            result.push_back(&sym);
    }
    return result;
}

bool ModuleRegistry::hasModule(const std::string& modulePath) const {
    return modules_.count(modulePath) > 0;
}

std::string ModuleRegistry::mangle(const std::string& modulePath,
                                    const std::string& name) {
    if (name == "main") return "main";
    std::string result;
    bool lastSep = false;
    for (auto& c : modulePath) {
        if (c == '/' || c == ':' || c == '\\') {
            if (!lastSep) { result += '_'; lastSep = true; }
        } else {
            result += c;
            lastSep = false;
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    return result + "__" + name;
}

std::vector<std::string> ModuleRegistry::allModules() const {
    std::vector<std::string> result;
    result.reserve(modules_.size());
    for (auto& [mod, _] : modules_)
        result.push_back(mod);
    std::sort(result.begin(), result.end());
    return result;
}

std::string ModuleRegistry::usePathToModulePath(const std::string& usePath) {
    std::string result = usePath;
    for (auto& c : result) {
        if (c == ':') c = '/';
    }
    std::string collapsed;
    bool lastSlash = false;
    for (auto& c : result) {
        if (c == '/') {
            if (!lastSlash) {
                collapsed += '/';
                lastSlash = true;
            }
        } else {
            collapsed += c;
            lastSlash = false;
        }
    }
    return collapsed;
}

std::string ModuleRegistry::modulePathToFile(const std::string& modulePath) {
    return modulePath + ".lc";
}
