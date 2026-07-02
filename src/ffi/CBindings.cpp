#include "ffi/CBindings.h"

void CBindings::addFunction(CFunction func) {
    functions_.emplace(func.name, std::move(func));
}

void CBindings::addStruct(CStruct s) {
    structs_.emplace(s.name, std::move(s));
}

void CBindings::addEnum(CEnum e) {
    enums_.emplace(e.name, std::move(e));
}

void CBindings::addTypedef(CTypedef t) {
    typedefs_.emplace(t.name, std::move(t));
}

void CBindings::addMacro(CMacro m) {
    macros_.emplace(m.name, std::move(m));
}

void CBindings::addStructMacro(CStructMacro m) {
    structMacros_.emplace(m.name, std::move(m));
}

void CBindings::addGlobal(CGlobalVar g) {
    globals_.emplace(g.name, std::move(g));
}

void CBindings::addFunctionLikeMacro(CFunctionLikeMacro m) {
    functionLikeMacros_.emplace(m.name, std::move(m));
}

const CFunctionLikeMacro* CBindings::findFunctionLikeMacro(const std::string& name) const {
    auto it = functionLikeMacros_.find(name);
    return it != functionLikeMacros_.end() ? &it->second : nullptr;
}

void CBindings::addRequiredLib(const std::string& flag,
                               const std::string& header) {
    for (auto& [f, h] : requiredLibs_)
        if (f == flag) return;   // avoid duplicates
    requiredLibs_.emplace_back(flag, header);
}

const CFunction* CBindings::findFunction(const std::string& name) const {
    auto it = functions_.find(name);
    return it != functions_.end() ? &it->second : nullptr;
}

const CStruct* CBindings::findStruct(const std::string& name) const {
    auto it = structs_.find(name);
    return it != structs_.end() ? &it->second : nullptr;
}

const CEnum* CBindings::findEnum(const std::string& name) const {
    auto it = enums_.find(name);
    return it != enums_.end() ? &it->second : nullptr;
}

const CTypedef* CBindings::findTypedef(const std::string& name) const {
    auto it = typedefs_.find(name);
    return it != typedefs_.end() ? &it->second : nullptr;
}

const CMacro* CBindings::findMacro(const std::string& name) const {
    auto it = macros_.find(name);
    return it != macros_.end() ? &it->second : nullptr;
}

const CStructMacro* CBindings::findStructMacro(const std::string& name) const {
    auto it = structMacros_.find(name);
    return it != structMacros_.end() ? &it->second : nullptr;
}

const CGlobalVar* CBindings::findGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    return it != globals_.end() ? &it->second : nullptr;
}

bool CBindings::hasSymbol(const std::string& name) const {
    return functions_.count(name)
        || structs_.count(name)
        || enums_.count(name)
        || typedefs_.count(name)
        || macros_.count(name)
        || structMacros_.count(name)
        || globals_.count(name)
        || functionLikeMacros_.count(name);
}

const TypeInfo* CBindings::internType(std::unique_ptr<TypeInfo> ti) {
    const TypeInfo* raw = ti.get();
    ownedTypes_.push_back(std::move(ti));
    return raw;
}

void CBindings::setStructLocation(const std::string& name,
                                   const std::string& file, unsigned ln) {
    auto it = structs_.find(name);
    if (it != structs_.end()) {
        it->second.sourceFile = file;
        it->second.line       = ln;
    }
}

void CBindings::addHeaderPath(const std::string& headerName,
                               const std::string& absolutePath) {
    headerPaths_.emplace(headerName, absolutePath);
}

std::string CBindings::resolveHeaderPath(const std::string& headerName) const {
    auto it = headerPaths_.find(headerName);
    return it != headerPaths_.end() ? it->second : std::string{};
}
