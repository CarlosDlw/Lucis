#include "namespace/ModuleRegistry.h"

#include <algorithm>
#include <unordered_set>
#include <sstream>

void ModuleRegistry::registerFile(const std::string& modulePath,
                                   const std::string& filePath,
                                   LucisParser::ProgramContext* tree) {
    auto& symbols = modules_[modulePath];

    for (auto* topLevel : tree->topLevelDecl()) {
        if (auto* funcDecl = topLevel->functionDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Function;
            sym.name       = funcDecl->IDENTIFIER(0)->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = funcDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* structDecl = topLevel->structDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Struct;
            sym.name       = structDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = structDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* unionDecl = topLevel->unionDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Union;
            sym.name       = unionDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = unionDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* enumDecl = topLevel->enumDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::Enum;
            sym.name       = enumDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = enumDecl;
            symbols.push_back(std::move(sym));
        }
        else if (auto* typeAlias = topLevel->typeAliasDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::TypeAlias;
            sym.name       = typeAlias->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = typeAlias;
            symbols.push_back(std::move(sym));
        }
        else if (auto* extDecl = topLevel->extendDecl()) {
            ExportedSymbol sym;
            sym.kind       = ExportedSymbol::ExtendBlock;
            sym.name       = extDecl->IDENTIFIER()->getText();
            sym.modulePath = modulePath;
            sym.sourceFile = filePath;
            sym.decl       = extDecl;
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
    // Use only the filename stem (last component) for cleaner FFI symbols
    std::string stem = modulePath;
    auto pos = stem.rfind('/');
    if (pos != std::string::npos) stem = stem.substr(pos + 1);
    // Replace any remaining special chars with _
    for (auto& c : stem) {
        if (c == '/' || c == ':' || c == '\\')
            c = '_';
    }
    std::string collapsed;
    bool lastUnderscore = false;
    for (auto& c : stem) {
        if (c == '_') {
            if (!lastUnderscore) {
                collapsed += '_';
                lastUnderscore = true;
            }
        } else {
            collapsed += c;
            lastUnderscore = false;
        }
    }
    return collapsed + "__" + name;
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
