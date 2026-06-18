#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "generated/LucisParser.h"
#include "parser/Parser.h"

struct ExportedSymbol {
    enum Kind { Function, Struct, Union, Enum, TypeAlias, ExtendBlock };

    Kind        kind;
    std::string name;
    std::string modulePath;
    std::string sourceFile;

    // Location of the declaration (line 1-based, column 0-based).
    // Phase 4: replaces ParserRuleContext* for LSP navigation.
    unsigned    line   = 0;
    unsigned    column = 0;

    // DEPRECATED (Phase 4): will be removed once Checker/IRGen use SemanticDB.
    antlr4::ParserRuleContext* decl = nullptr;

    // Keeps the parse tree alive as long as this symbol exists.
    std::shared_ptr<ParseResult> treeAnchor;
};

class ModuleRegistry {
public:
    void registerFile(const std::string& modulePath,
                      const std::string& filePath,
                      LucisParser::ProgramContext* tree,
                      std::shared_ptr<ParseResult> anchor);

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
