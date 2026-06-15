#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "generated/LucisParser.h"

struct ExportedSymbol {
    enum Kind { Function, Struct, Union, Enum, TypeAlias, ExtendBlock };

    Kind        kind;
    std::string name;
    std::string modulePath;
    std::string sourceFile;

    antlr4::ParserRuleContext* decl = nullptr;
};

class ModuleRegistry {
public:
    void registerFile(const std::string& modulePath,
                      const std::string& filePath,
                      LucisParser::ProgramContext* tree);

    std::vector<std::string> validate() const;

    const ExportedSymbol* findSymbol(const std::string& modulePath,
                                     const std::string& name) const;

    std::vector<const ExportedSymbol*> getModuleSymbols(const std::string& modulePath) const;

    std::vector<const ExportedSymbol*> getExternalSymbols(
        const std::string& modulePath,
        const std::string& excludeFile) const;

    bool hasModule(const std::string& modulePath) const;

    static std::string mangle(const std::string& modulePath, const std::string& name);

    std::vector<std::string> allModules() const;

    static std::string usePathToModulePath(const std::string& usePath);

    static std::string modulePathToFile(const std::string& modulePath);

private:
    std::unordered_map<std::string, std::vector<ExportedSymbol>> modules_;
};
