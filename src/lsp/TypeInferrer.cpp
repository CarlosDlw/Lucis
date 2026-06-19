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
