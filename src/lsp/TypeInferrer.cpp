#include "lsp/TypeInferrer.h"
#include "generated/LucisParser.h"
#include "lsp/ProjectContext.h"
#include "namespace/ModuleRegistry.h"

#include <vector>
#include <string>

namespace {

std::string safeText(antlr4::tree::TerminalNode* n) {
    return n ? n->getText() : "";
}

std::string safeText(antlr4::ParserRuleContext* ctx) {
    return ctx ? ctx->getText() : "";
}

} // anonymous namespace

LucisParser::StructDeclContext* TypeInferrer::findStruct(
    const std::string& name,
    LucisParser::ProgramContext* tree,
    const ProjectContext* project) {

    // Same-file
    if (tree) {
        for (auto* tld : tree->topLevelDecl()) {
            auto* sd = tld->structDecl();
            if (sd && safeText(sd->IDENTIFIER()) == name)
                return sd;
        }
    }

    // Cross-file via registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto syms = project->registry().getModuleSymbols(ns);
            for (auto* sym : syms) {
                if (sym->kind != ExportedSymbol::Struct) continue;
                if (sym->name != name) continue;
                return dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
            }
        }
    }

    return nullptr;
}

LucisParser::TypeSpecContext* TypeInferrer::findFieldTypeSpec(
    LucisParser::StructDeclContext* sd,
    const std::string& fieldName) {
    if (!sd) return nullptr;
    for (auto* f : sd->structField()) {
        if (safeText(f->IDENTIFIER()) == fieldName)
            return f->typeSpec();
    }
    return nullptr;
}

std::string TypeInferrer::functionReturnType(LucisParser::TypeSpecContext* ts) {
    if (!ts) return "";
    auto* ft = ts->fnTypeSpec();
    if (!ft) return "";
    // fn(Params) -> ReturnType: the last typeSpec child is the return type
    auto specs = ft->typeSpec();
    if (specs.empty()) return "";
    return safeText(specs.back());
}

static std::string extractBaseName(LucisParser::TypeSpecContext* ts) {
    auto text = ts->getText();
    auto pos = text.find('<');
    if (pos != std::string::npos)
        text.resize(pos);
    while (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

LucisParser::EnumDeclContext* TypeInferrer::findEnum(
    const std::string& name,
    LucisParser::ProgramContext* tree,
    const ProjectContext* project,
    std::string* resolvedName) {

    // 1) Direct enum lookup in same-file
    if (tree) {
        for (auto* tld : tree->topLevelDecl()) {
            auto* ed = tld->enumDecl();
            if (ed && ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name) {
                if (resolvedName) *resolvedName = name;
                return ed;
            }
        }
    }

    // 2) Type alias resolution (local)
    if (tree) {
        for (auto* tld : tree->topLevelDecl()) {
            auto* ta = tld->typeAliasDecl();
            if (!ta || safeText(ta->IDENTIFIER()) != name) continue;
            auto base = extractBaseName(ta->typeSpec());
            // Recurse with the resolved base name
            return findEnum(base, tree, project, resolvedName);
        }
    }

    // 3) Cross-file via registry
    if (project && project->isValid()) {
        for (auto& ns : project->registry().allModules()) {
            auto* sym = project->registry().findSymbol(ns, name);
            if (!sym) continue;
            if (sym->kind == ExportedSymbol::Enum) {
                if (auto* ed = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl)) {
                    if (resolvedName) *resolvedName = name;
                    return ed;
                }
            }
            // Resolve through cross-file type aliases
            if (sym->kind == ExportedSymbol::TypeAlias) {
                if (auto* ta = dynamic_cast<LucisParser::TypeAliasDeclContext*>(sym->decl)) {
                    auto base = extractBaseName(ta->typeSpec());
                    return findEnum(base, tree, project, resolvedName);
                }
            }
        }
    }

    return nullptr;
}

std::string TypeInferrer::resolveMethodReturnTypeViaStructField(
    const std::string& receiverType,
    const std::string& methodName,
    LucisParser::ProgramContext* tree,
    const ProjectContext* project) {

    auto* sd = findStruct(receiverType, tree, project);
    if (!sd) return "";

    auto* fieldTs = findFieldTypeSpec(sd, methodName);
    if (!fieldTs) return "";

    return functionReturnType(fieldTs);
}
