#include "checkers/Checker.h"
#include "checkers/Checker_helpers.h"

#include "generated/LucisLexer.h"
#include "ffi/CBindings.h"
#include "ffi/CMacroEval.h"
#include "attributes/CfgPredicate.h"
#include "attributes/TargetInfo.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <filesystem>
#include <functional>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>



semantic::SourceLocation Checker::toSemanticLoc(
    antlr4::ParserRuleContext* ctx) const {
    semantic::SourceLocation loc;
    if (ctx) {
        loc.file   = currentFile_;
        loc.line   = static_cast<unsigned>(ctx->getStart()->getLine());
        loc.column = static_cast<unsigned>(ctx->getStart()->getCharPositionInLine());
    }
    return loc;
}



semantic::DeclKind Checker::toSemanticKind(TypeKind tk) const {
    switch (tk) {
    case TypeKind::Integer:  return semantic::DeclKind::Integer;
    case TypeKind::Float:    return semantic::DeclKind::Float;
    case TypeKind::Bool:     return semantic::DeclKind::Bool;
    case TypeKind::Char:     return semantic::DeclKind::Char;
    case TypeKind::Void:     return semantic::DeclKind::Void;
    case TypeKind::String:   return semantic::DeclKind::String;
    case TypeKind::Struct:   return semantic::DeclKind::Struct;
    case TypeKind::Union:    return semantic::DeclKind::Union;
    case TypeKind::Enum:     return semantic::DeclKind::Enum;
    case TypeKind::Pointer:  return semantic::DeclKind::Pointer;
    case TypeKind::Function: return semantic::DeclKind::Function;
    case TypeKind::Extended: return semantic::DeclKind::Extended;
    case TypeKind::Tuple:    return semantic::DeclKind::Tuple;
    case TypeKind::VAList:   return semantic::DeclKind::VAList;
    case TypeKind::Fallible: return semantic::DeclKind::Fallible;
    case TypeKind::BitStruct: return semantic::DeclKind::BitStruct;
    }
    return semantic::DeclKind::Void;
}



semantic::FieldInfo Checker::toSemanticField(const ::FieldInfo& f) const {
    semantic::FieldInfo sf;
    sf.name       = f.name;
    sf.arrayDims  = f.arrayDims;
    sf.arraySizes = f.arraySizes;
    sf.autoFill   = f.autoFill;
    if (f.typeInfo && semanticDB_)
        sf.type = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(f.typeInfo->name));
    return sf;
}



semantic::VariantInfo Checker::toSemanticVariant(const EnumVariantInfo& v) const {
    semantic::VariantInfo sv;
    sv.name         = v.name;
    sv.discriminant = v.discriminant;
    switch (v.payloadKind) {
    case EnumPayloadKind::Unit:  sv.payloadKind = semantic::VariantPayloadKind::Unit;  break;
    case EnumPayloadKind::Tuple: sv.payloadKind = semantic::VariantPayloadKind::Tuple; break;
    case EnumPayloadKind::Named: sv.payloadKind = semantic::VariantPayloadKind::Named; break;
    }
    for (const auto& pf : v.payloadFields)
        sv.payloadFields.push_back(toSemanticField(pf));
    return sv;
}



semantic::MethodInfo Checker::toSemanticMethod(const StructMethodInfo& m) const {
    semantic::MethodInfo sm;
    sm.name     = m.name;
    sm.isStatic = m.isStatic;
    if (m.returnType && semanticDB_)
        sm.returnType = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(m.returnType->name));
    for (auto* pt : m.paramTypes) {
        semantic::ParamInfo sp;
        if (pt && semanticDB_)
            sp.type = const_cast<semantic::Decl*>(
                semanticDB_->lookupAny(pt->name));
        sm.params.push_back(sp);
    }
    return sm;
}



std::unique_ptr<semantic::Decl> Checker::typeInfoToDecl(const TypeInfo& ti) {
    if (!semanticDB_) return nullptr;

    switch (ti.kind) {
    case TypeKind::Struct: {
        auto sd = std::make_unique<semantic::StructDecl>();
        sd->name       = ti.name;
        sd->modulePath = currentModulePath_;
        sd->dropTracked = ti.dropTracked;
        sd->moveOnly    = ti.moveOnly;
        if (ti.parentType)
            sd->parentName = ti.parentType->name;
        for (const auto& f : ti.fields)
            sd->fields.push_back(toSemanticField(f));
        return sd;
    }
    case TypeKind::Union: {
        auto ud = std::make_unique<semantic::UnionDecl>();
        ud->name       = ti.name;
        ud->modulePath = currentModulePath_;
        ud->dropTracked = ti.dropTracked;
        ud->moveOnly    = ti.moveOnly;
        for (const auto& f : ti.fields)
            ud->fields.push_back(toSemanticField(f));
        return ud;
    }
    case TypeKind::Enum: {
        auto ed = std::make_unique<semantic::EnumDecl>();
        ed->name       = ti.name;
        ed->modulePath = currentModulePath_;
        ed->dropTracked = ti.dropTracked;
        ed->moveOnly    = ti.moveOnly;
        for (const auto& v : ti.enumVariantInfos)
            ed->variants.push_back(toSemanticVariant(v));
        return ed;
    }
    default:
        return nullptr;
    }
}



void Checker::syncToSemanticDB_Struct(const TypeInfo& ti,
                                       const std::string& modulePath,
                                       antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Struct,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Union(const TypeInfo& ti,
                                      const std::string& modulePath,
                                      antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Union,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Enum(const TypeInfo& ti,
                                     const std::string& modulePath,
                                     antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    semanticDB_->forwardDeclare(ti.name, semantic::DeclKind::Enum,
                                modulePath, loc);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_TypeAlias(const TypeInfo& ti,
                                          const std::string& modulePath,
                                          antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto loc = toSemanticLoc(ctx);
    auto decl = typeInfoToDecl(ti);
    if (decl) {
        decl->modulePath = modulePath;
        decl->loc = loc;
        semanticDB_->registerType(std::move(decl));
    }
}

void Checker::syncToSemanticDB_Function(const std::string& name,
                                         const TypeInfo& funcType,
                                         const std::string& modulePath,
                                         antlr4::ParserRuleContext* ctx) {
    if (!semanticDB_) return;
    auto fd = std::make_unique<semantic::FunctionDecl>();
    fd->name       = name;
    fd->modulePath = modulePath;
    fd->loc        = toSemanticLoc(ctx);
    fd->isVariadic = funcType.isVariadic;
    if (funcType.returnType && semanticDB_)
        fd->returnType = const_cast<semantic::Decl*>(
            semanticDB_->lookupAny(funcType.returnType->name));
    for (auto* pt : funcType.paramTypes) {
        semantic::ParamInfo sp;
        if (pt && semanticDB_)
            sp.type = const_cast<semantic::Decl*>(
                semanticDB_->lookupAny(pt->name));
        fd->params.push_back(sp);
    }
    semanticDB_->registerFunction(std::move(fd));
}

void Checker::syncToSemanticDB_Extend(const std::string& structName,
                                       const std::vector<StructMethodInfo>& methods) {
    if (!semanticDB_) return;
    std::vector<semantic::MethodInfo> smethods;
    for (const auto& m : methods)
        smethods.push_back(toSemanticMethod(m));
    semanticDB_->mergeExtendMethods(structName, std::move(smethods));
}

void Checker::syncToSemanticDB_GenericStruct(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::StructDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::StructDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericUnion(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::UnionDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::UnionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericEnum(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::EnumDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::EnumDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericFunc(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::FunctionDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::FunctionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericExtend(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::ExtendDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::FunctionDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericInstantiation(
    const std::string& mangledName,
    const TypeInfo& concreteTI) {
    if (!semanticDB_) return;
    auto decl = typeInfoToDecl(concreteTI);
    if (decl) {
        decl->name = mangledName;
        semanticDB_->registerType(std::move(decl));
    }
}

// ── Trait / Impl SemanticDB synchronization ──────────────────────────────────

void Checker::syncToSemanticDB_GenericTrait(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::TraitDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::TraitDecl>();
    pattern->name = name;
    pattern->genericParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_GenericImpl(const std::string& name,
    const std::vector<std::string>& typeParams,
    LucisParser::ImplDeclContext* decl) {
    if (!semanticDB_) return;
    auto tmpl = std::make_unique<semantic::GenericTemplateDecl>();
    tmpl->name       = name;
    tmpl->modulePath = currentModulePath_;
    tmpl->loc        = toSemanticLoc(decl);
    tmpl->typeParams = typeParams;
    auto pattern = std::make_unique<semantic::ImplDecl>();
    pattern->name = name;
    pattern->implTypeParams = typeParams;
    tmpl->pattern = std::move(pattern);
    semanticDB_->registerGeneric(std::move(tmpl));
}

void Checker::syncToSemanticDB_Trait(const std::string& name,
                                      LucisParser::TraitDeclContext* decl) {
    if (!semanticDB_) return;
    auto td = std::make_unique<semantic::TraitDecl>();
    td->name       = name;
    td->modulePath = currentModulePath_;
    td->loc        = toSemanticLoc(decl);

    // Convert trait methods to semantic MethodInfo
    for (auto* m : decl->traitMethod()) {
        semantic::MethodInfo mi;
        mi.name     = m->name->getText();
        mi.isStatic = (m->AMPERSAND() == nullptr);

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(m->typeSpec(), retDims);
        if (retType) {
            auto decl = typeInfoToDecl(*retType);
            if (decl)
                mi.returnType = decl.get();
        }

        if (mi.isStatic) {
            if (auto* pl = m->paramList()) {
                for (auto* p : pl->param()) {
                    if (p->SPREAD()) continue;
                    semantic::ParamInfo pi;
                    pi.name = p->IDENTIFIER()->getText();
                    unsigned dims = 0;
                    auto* pt = resolveTypeSpec(p->typeSpec(), dims);
                    if (pt) {
                        auto pdecl = typeInfoToDecl(*pt);
                        if (pdecl) pi.type = pdecl.get();
                    }
                    mi.params.push_back(pi);
                }
            }
        } else {
            for (auto* p : m->param()) {
                if (p->SPREAD()) continue;
                semantic::ParamInfo pi;
                pi.name = p->IDENTIFIER()->getText();
                unsigned dims = 0;
                auto* pt = resolveTypeSpec(p->typeSpec(), dims);
                if (pt) {
                    auto pdecl = typeInfoToDecl(*pt);
                    if (pdecl) pi.type = pdecl.get();
                }
                mi.params.push_back(pi);
            }
        }
        td->methods.push_back(std::move(mi));
    }

    semanticDB_->registerTrait(std::move(td));
}

void Checker::syncToSemanticDB_Impl(const std::string& traitName,
                                     const std::string& typeName,
                                     LucisParser::ImplDeclContext* decl) {
    if (!semanticDB_) return;
    auto id = std::make_unique<semantic::ImplDecl>();
    id->name       = traitName + ":" + typeName;
    id->traitName  = traitName;
    id->typeName   = typeName;
    id->modulePath = currentModulePath_;
    id->loc        = toSemanticLoc(decl);

    for (auto* m : decl->implMethod()) {
        semantic::MethodInfo mi;
        mi.name     = m->name->getText();
        mi.isStatic = (m->AMPERSAND() == nullptr);

        unsigned retDims = 0;
        auto* retType = resolveTypeSpec(m->typeSpec(), retDims);
        if (retType) {
            auto rdecl = typeInfoToDecl(*retType);
            if (rdecl) mi.returnType = rdecl.get();
        }

        if (mi.isStatic) {
            if (auto* pl = m->paramList()) {
                for (auto* p : pl->param()) {
                    if (p->SPREAD()) continue;
                    semantic::ParamInfo pi;
                    pi.name = p->IDENTIFIER()->getText();
                    unsigned dims = 0;
                    auto* pt = resolveTypeSpec(p->typeSpec(), dims);
                    if (pt) {
                        auto pdecl = typeInfoToDecl(*pt);
                        if (pdecl) pi.type = pdecl.get();
                    }
                    mi.params.push_back(pi);
                }
            }
        } else {
            for (auto* p : m->param()) {
                if (p->SPREAD()) continue;
                semantic::ParamInfo pi;
                pi.name = p->IDENTIFIER()->getText();
                unsigned dims = 0;
                auto* pt = resolveTypeSpec(p->typeSpec(), dims);
                if (pt) {
                    auto pdecl = typeInfoToDecl(*pt);
                    if (pdecl) pi.type = pdecl.get();
                }
                mi.params.push_back(pi);
            }
        }
        id->methods.push_back(std::move(mi));
    }

    semanticDB_->registerImpl(std::move(id));
}

