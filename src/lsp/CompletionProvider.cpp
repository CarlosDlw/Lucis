#include "lsp/CompletionProvider.h"
#include "lsp/TypeInferrer.h"
#include "ffi/CHeaderResolver.h"
#include "imports/ImportResolver.h"
#include "lsp/DocComment.h"
#include "lsp/ProjectContext.h"
#include "namespace/ModuleRegistry.h"
#include "parser/Parser.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_set>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

CompletionProvider::CompletionProvider() : intrinsicRegistry_(typeRegistry_) {}

// Normalize lowercase native keywords (vec, map, set) to registry CamelCase
// (Vec, Map, Set)
static std::string normalizeExtBaseName(const std::string &name) {
  if (name == "vec")
    return "Vec";
  if (name == "map")
    return "Map";
  if (name == "set")
    return "Set";
  return name;
}

// ═══════════════════════════════════════════════════════════════════════
//  Static helpers
// ═══════════════════════════════════════════════════════════════════════

static bool cursorInsideNode(antlr4::ParserRuleContext *node,
                             size_t cursorLine0) {
  if (!node)
    return false;
  auto *start = node->getStart();
  auto *stop = node->getStop();
  if (!start || !stop)
    return false;
  size_t startLine = start->getLine() - 1;
  size_t stopLine = stop->getLine() - 1;
  return cursorLine0 >= startLine && cursorLine0 <= stopLine;
}

static std::string safeText(antlr4::tree::TerminalNode *n) {
    return n ? n->getText() : "";
}
static std::string safeText(antlr4::ParserRuleContext *ctx) {
    return ctx ? ctx->getText() : "";
}
template<typename T>
static std::string safeIdAt(T *ctx, size_t i) {
    if (!ctx) return "";
    if (i >= ctx->IDENTIFIER().size()) return "";
    auto *n = ctx->IDENTIFIER(i);
    return n ? n->getText() : "";
}

static std::string
substituteTypeParams(const std::string &type,
                     const std::unordered_map<std::string, std::string> &subst);

static std::string unwrapIndexedType(std::string typeName,
                                     unsigned indexDepth) {
  auto trim = [](std::string s) {
    size_t b = s.find_first_not_of(" \t\n\r");
    if (b == std::string::npos)
      return std::string{};
    size_t e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
  };

  typeName = trim(typeName);

  for (unsigned d = 0; d < indexDepth && !typeName.empty(); d++) {
    typeName = trim(typeName);

    if (typeName == "string") {
      typeName = "char";
      continue;
    }

    // Pointer to unsized array sugar: *[]T -> T (for one index)
    if (typeName.rfind("*[]", 0) == 0) {
      typeName = typeName.substr(3);
      continue;
    }

    // Generic pointer indexing: *T -> T
    if (!typeName.empty() && typeName[0] == '*') {
      typeName = typeName.substr(1);
      typeName = trim(typeName);

      // If pointee is an array type, consume one array dimension too.
      if (typeName.rfind("[]", 0) == 0) {
        typeName = typeName.substr(2);
      } else if (!typeName.empty() && typeName[0] == '[') {
        auto closeBracket = typeName.find(']');
        if (closeBracket != std::string::npos)
          typeName = typeName.substr(closeBracket + 1);
      }
      continue;
    }

    // Unsized/fixed array indexing: []T or [N]T -> T
    if (typeName.rfind("[]", 0) == 0) {
      typeName = typeName.substr(2);
      continue;
    }
    if (!typeName.empty() && typeName[0] == '[') {
      auto closeBracket = typeName.find(']');
      if (closeBracket != std::string::npos) {
        typeName = typeName.substr(closeBracket + 1);
        continue;
      }
    }

    // Unknown shape: cannot unwrap further.
    break;
  }

  return trim(typeName);
}

struct FuncLookupCtx {
  LucisParser::ProgramContext *tree = nullptr;
  const CBindings *bindings = nullptr;
  const BuiltinRegistry *builtinReg = nullptr;
  const IntrinsicRegistry *intrinsicReg = nullptr;
  const ExtendedTypeRegistry *extTypeReg = nullptr;
  const MethodRegistry *methodReg = nullptr;
  const ProjectContext *project = nullptr;
};

static std::string resolveIndexedElementType(const std::string& baseType) {
    if (baseType == "string") return "char";
    if (baseType.rfind("[]", 0) == 0) return baseType.substr(2);
    if (!baseType.empty() && baseType[0] == '[') {
        auto close = baseType.find(']');
        if (close != std::string::npos && close + 1 < baseType.size())
            return baseType.substr(close + 1);
    }
    if (baseType.rfind("Vec<", 0) == 0) {
        auto open = baseType.find('<');
        auto close = baseType.rfind('>');
        if (open != std::string::npos && close != std::string::npos)
            return baseType.substr(open + 1, close - open - 1);
    }
    return "";
}

static std::string inferExprTypeName(
    LucisParser::ExpressionContext *expr,
    const std::unordered_map<std::string, CompletionProvider::LocalVar> &locals,
    const FuncLookupCtx *flc);
static bool parseGenericInstance(const std::string &t, std::string &base,
                                 std::vector<std::string> &args);
static LucisParser::FunctionDeclContext *
findFunctionDeclForInference(const std::string &name, const FuncLookupCtx *flc);
static LucisParser::StructDeclContext *
findStructDeclForInference(const std::string &name, const FuncLookupCtx *flc);
static LucisParser::UnionDeclContext *
findUnionDeclForInference(const std::string &name, const FuncLookupCtx *flc);
static LucisParser::EnumDeclContext *
findEnumDeclForInference(const std::string &name, const FuncLookupCtx *flc);

static std::string inferIsBindingPayloadType(LucisParser::ExpressionContext *cond,
                                             LucisParser::ProgramContext *tree) {
  auto *isE = dynamic_cast<LucisParser::IsExprContext *>(cond);
  if (!isE || !isE->SCOPE() || !isE->LPAREN() || !isE->IDENTIFIER(1))
    return "";

  auto *rhsType = isE->typeSpec();
  if (!rhsType || !rhsType->IDENTIFIER())
    return "";

  auto enumName = safeText(rhsType->IDENTIFIER());
  LucisParser::EnumDeclContext *enumDecl = nullptr;
  if (tree) {
    for (auto *tld : tree->topLevelDecl()) {
      if (auto *ed = tld->enumDecl();
          ed && ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == enumName) {
        enumDecl = ed;
        break;
      }
    }
  }
  if (!enumDecl)
    return "";

  auto variantName = isE->IDENTIFIER(0) ? safeIdAt(isE, 0) : "";
  if (variantName.empty())
    return "";

  for (auto *variant : enumDecl->enumVariant()) {
    auto *vId = variant->IDENTIFIER();
    if (!vId || vId->getText() != variantName)
      continue;
    if (!variant->LPAREN() || variant->typeSpec().size() != 1)
      return "";

    auto payloadType = variant->typeSpec(0)->getText();
    if (rhsType->LT() && enumDecl->typeParamList()) {
      std::unordered_map<std::string, std::string> subst;
      auto tps = enumDecl->typeParamList()->typeParam();
      auto args = rhsType->typeSpec();
      for (size_t i = 0; i < tps.size() && i < args.size(); i++) {
        auto ids = tps[i]->IDENTIFIER();
        if (!ids.empty())
          subst[ids[0]->getText()] = args[i]->getText();
      }
      payloadType = substituteTypeParams(payloadType, subst);
    }
    return payloadType;
  }

  return "";
}

// Resolve a type name through type aliases to find the underlying enum decl.
// Handles both local and cross-file lookups.  Updates baseName/sourceArgs/sourceType
// in-place when a type alias is resolved (e.g. DivideResult → Result<int32>).
static LucisParser::EnumDeclContext *resolveEnumForCatch(
    std::string &baseName,
    std::vector<std::string> &sourceArgs,
    std::string &sourceType,
    LucisParser::ProgramContext *tree,
    const FuncLookupCtx *flc) {
  // 1) Try direct enum lookup
  auto *ed = findEnumDeclForInference(baseName, flc);
  if (ed) return ed;
  if (tree) {
    for (auto *tld : tree->topLevelDecl()) {
      if (auto *e = tld->enumDecl();
          e && e->IDENTIFIER() && safeText(e->IDENTIFIER()) == baseName)
        return e;
    }
  }

  // 2) Resolve through type aliases (local + cross-file)
  LucisParser::TypeAliasDeclContext *alias = nullptr;
  if (tree) {
    for (auto *tld : tree->topLevelDecl()) {
      if (auto *ta = tld->typeAliasDecl();
          ta && safeText(ta->IDENTIFIER()) == baseName) {
        alias = ta; break;
      }
    }
  }
  if (!alias && flc && flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, baseName);
      if (!sym || sym->kind != ExportedSymbol::TypeAlias) continue;
      alias = dynamic_cast<LucisParser::TypeAliasDeclContext *>(sym->decl);
      if (alias) break;
    }
  }
  if (!alias || !alias->typeSpec()) return nullptr;

  // Parse the aliased type: "Result<int32>"
  auto aliased = safeText(alias->typeSpec());
  auto newBase = aliased;
  sourceArgs.clear();
  parseGenericInstance(aliased, newBase, sourceArgs);

  // 3) Look up the real enum
  ed = findEnumDeclForInference(newBase, flc);
  if (!ed && tree) {
    for (auto *tld : tree->topLevelDecl()) {
      if (auto *e = tld->enumDecl();
          e && e->IDENTIFIER() && safeText(e->IDENTIFIER()) == newBase)
        return e;
    }
  }
  if (ed) {
    sourceType = aliased;
    baseName = newBase;
  }
  return ed;
}

static std::string inferCatchUnwrapSuccessType(
    LucisParser::ExpressionContext *sourceExpr, LucisParser::ProgramContext *tree,
    const std::unordered_map<std::string, CompletionProvider::LocalVar> &locals,
    const FuncLookupCtx *flc) {
  if (!sourceExpr)
    return "";

  auto enumTypeName = inferExprTypeName(sourceExpr, locals, flc);
  if (enumTypeName.empty())
    return "";

  auto baseName = enumTypeName;
  std::vector<std::string> sourceArgs;
  parseGenericInstance(enumTypeName, baseName, sourceArgs);

  auto *enumDecl = resolveEnumForCatch(baseName, sourceArgs,
                                        enumTypeName, tree, flc);
  if (!enumDecl)
    return "";

  std::unordered_map<std::string, std::string> subst;
  if (enumDecl->typeParamList()) {
    std::vector<std::string> sourceArgs;
    std::string parsedBase = enumTypeName;
    parseGenericInstance(enumTypeName, parsedBase, sourceArgs);
    auto tps = enumDecl->typeParamList()->typeParam();
    for (size_t i = 0; i < std::min(tps.size(), sourceArgs.size()); i++) {
      auto ids = tps[i]->IDENTIFIER();
      if (!ids.empty())
        subst[ids[0]->getText()] = sourceArgs[i];
    }
  }

  std::string successType;
  bool seenError = false;
  for (auto *variant : enumDecl->enumVariant()) {
    if (!variant || variant->typeSpec().size() != 1)
      return "";
    auto payloadType =
        substituteTypeParams(variant->typeSpec(0)->getText(), subst);
    auto varName = variant->IDENTIFIER()
                       ? variant->IDENTIFIER()->getText() : "";
    if (varName == "Err" || payloadType == "Error") {
      if (seenError)
        return "";
      seenError = true;
    } else {
      if (!successType.empty())
        return "";
      successType = payloadType;
    }
  }

  if (!seenError || successType.empty())
    return "";
  return successType;
}

static std::string inferCatchUnwrapItType(
    LucisParser::ExpressionContext *sourceExpr, LucisParser::ProgramContext *tree,
    const std::unordered_map<std::string, CompletionProvider::LocalVar> &locals,
    const FuncLookupCtx *flc) {
  if (!sourceExpr) return "";

  auto enumTypeName = inferExprTypeName(sourceExpr, locals, flc);
  if (enumTypeName.empty()) return "";

  auto baseName = enumTypeName;
  std::vector<std::string> sourceArgs;
  parseGenericInstance(enumTypeName, baseName, sourceArgs);

  auto *enumDecl = resolveEnumForCatch(baseName, sourceArgs,
                                        enumTypeName, tree, flc);
  if (!enumDecl) return "";

  std::unordered_map<std::string, std::string> subst;
  if (enumDecl->typeParamList()) {
    std::vector<std::string> sourceArgs;
    std::string parsedBase = enumTypeName;
    parseGenericInstance(enumTypeName, parsedBase, sourceArgs);
    auto tps = enumDecl->typeParamList()->typeParam();
    for (size_t i = 0; i < std::min(tps.size(), sourceArgs.size()); i++) {
      auto ids = tps[i]->IDENTIFIER();
      if (!ids.empty())
        subst[ids[0]->getText()] = sourceArgs[i];
    }
  }

  std::string errType;
  for (auto *variant : enumDecl->enumVariant()) {
    if (!variant || variant->typeSpec().size() != 1) continue;
    auto payloadName = variant->typeSpec(0)->getText();
    auto variantName = variant->IDENTIFIER() ? safeText(variant->IDENTIFIER()) : "";
    bool isErrName = variantName == "Err" || variantName == "Error" ||
                     variantName == "Failure" || variantName == "Fail" ||
                     variantName == "None";
    if (isErrName || payloadName == "Error") {
      errType = substituteTypeParams(payloadName, subst);
    }
  }

  return errType.empty() ? "Error" : errType;
}

static void collectLocalsFromBlock(
    LucisParser::BlockContext *block, size_t beforeLine,
    std::unordered_map<std::string, CompletionProvider::LocalVar> &out,
    const FuncLookupCtx *flc = nullptr);

static std::string trimCopy(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\n\r");
  if (b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\n\r");
  return s.substr(b, e - b + 1);
}

static std::vector<std::string> splitTopLevelComma(const std::string &s) {
  std::vector<std::string> out;
  int depthAngle = 0;
  int depthBracket = 0;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '<')
      depthAngle++;
    else if (c == '>')
      depthAngle--;
    else if (c == '[')
      depthBracket++;
    else if (c == ']')
      depthBracket--;
    else if (c == ',' && depthAngle == 0 && depthBracket == 0) {
      out.push_back(trimCopy(s.substr(start, i - start)));
      start = i + 1;
    }
  }
  out.push_back(trimCopy(s.substr(start)));
  return out;
}

static bool parseGenericInstance(const std::string &t, std::string &base,
                                 std::vector<std::string> &args) {
  auto lt = t.find('<');
  auto gt = t.rfind('>');
  if (lt == std::string::npos || gt == std::string::npos || gt <= lt)
    return false;
  base = trimCopy(t.substr(0, lt));
  args = splitTopLevelComma(t.substr(lt + 1, gt - lt - 1));
  return !base.empty();
}

static LucisParser::FunctionDeclContext *
findFunctionDeclForInference(const std::string &name,
                             const FuncLookupCtx *flc) {
  if (!flc)
    return nullptr;
  if (flc->tree) {
    for (auto *tld : flc->tree->topLevelDecl()) {
      auto *fd = tld->functionDecl();
      if (fd && !fd->IDENTIFIER().empty() && safeIdAt(fd, 0) == name)
        return fd;
    }
  }
  if (flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, name);
      if (!sym || sym->kind != ExportedSymbol::Function)
        continue;
      if (auto *fd = dynamic_cast<LucisParser::FunctionDeclContext *>(sym->decl))
        return fd;
    }
  }
  return nullptr;
}

static LucisParser::EnumDeclContext *
findEnumDeclForInference(const std::string &name, const FuncLookupCtx *flc) {
  if (!flc)
    return nullptr;
  if (flc->tree) {
    for (auto *tld : flc->tree->topLevelDecl()) {
      auto *ed = tld->enumDecl();
      if (ed && ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name)
        return ed;
    }
  }
  if (flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, name);
      if (!sym || sym->kind != ExportedSymbol::Enum)
        continue;
      if (auto *ed = dynamic_cast<LucisParser::EnumDeclContext *>(sym->decl))
        return ed;
    }
  }
  return nullptr;
}

static LucisParser::StructDeclContext *
findStructDeclForInference(const std::string &name, const FuncLookupCtx *flc) {
  if (!flc)
    return nullptr;
  if (flc->tree) {
    for (auto *tld : flc->tree->topLevelDecl()) {
      auto *sd = tld->structDecl();
      if (sd && sd->IDENTIFIER() && safeText(sd->IDENTIFIER()) == name)
        return sd;
    }
  }
  if (flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, name);
      if (!sym || sym->kind != ExportedSymbol::Struct)
        continue;
      if (auto *sd = dynamic_cast<LucisParser::StructDeclContext *>(sym->decl))
        return sd;
    }
  }
  return nullptr;
}

static LucisParser::UnionDeclContext *
findUnionDeclForInference(const std::string &name, const FuncLookupCtx *flc) {
  if (!flc)
    return nullptr;
  if (flc->tree) {
    for (auto *tld : flc->tree->topLevelDecl()) {
      auto *ud = tld->unionDecl();
      if (ud && ud->IDENTIFIER() && safeText(ud->IDENTIFIER()) == name)
        return ud;
    }
  }
  if (flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, name);
      if (!sym || sym->kind != ExportedSymbol::Union)
        continue;
      if (auto *ud = dynamic_cast<LucisParser::UnionDeclContext *>(sym->decl))
        return ud;
    }
  }
  return nullptr;
}

static std::string substituteTypeParams(
    const std::string &type,
    const std::unordered_map<std::string, std::string> &subst) {
  std::string t = trimCopy(type);
  auto it = subst.find(t);
  if (it != subst.end())
    return it->second;

  if (t.size() > 1 && t[0] == '*')
    return "*" + substituteTypeParams(t.substr(1), subst);

  if (!t.empty() && t[0] == '[') {
    auto rb = t.find(']');
    if (rb != std::string::npos && rb + 1 < t.size())
      return t.substr(0, rb + 1) +
             substituteTypeParams(t.substr(rb + 1), subst);
  }

  std::string base;
  std::vector<std::string> args;
  if (parseGenericInstance(t, base, args)) {
    std::string out = base + "<";
    for (size_t i = 0; i < args.size(); i++) {
      if (i)
        out += ", ";
      out += substituteTypeParams(args[i], subst);
    }
    out += ">";
    return out;
  }

  return t;
}

static bool
unifyGenericType(const std::string &formalRaw, const std::string &actualRaw,
                 const std::unordered_set<std::string> &typeParams,
                 std::unordered_map<std::string, std::string> &inferred) {
  std::string formal = trimCopy(formalRaw);
  std::string actual = trimCopy(actualRaw);
  if (formal.empty() || actual.empty())
    return false;

  if (typeParams.count(formal)) {
    auto it = inferred.find(formal);
    if (it == inferred.end()) {
      inferred[formal] = actual;
      return true;
    }
    return it->second == actual;
  }

  if (formal.size() > 1 && formal[0] == '*') {
    if (actual.size() <= 1 || actual[0] != '*')
      return false;
    return unifyGenericType(formal.substr(1), actual.substr(1), typeParams,
                            inferred);
  }

  if (!formal.empty() && formal[0] == '[') {
    auto frb = formal.find(']');
    auto arb = actual.find(']');
    if (frb == std::string::npos || arb == std::string::npos)
      return false;
    return unifyGenericType(formal.substr(frb + 1), actual.substr(arb + 1),
                            typeParams, inferred);
  }

  std::string fBase, aBase;
  std::vector<std::string> fArgs, aArgs;
  bool fGen = parseGenericInstance(formal, fBase, fArgs);
  bool aGen = parseGenericInstance(actual, aBase, aArgs);
  if (fGen || aGen) {
    if (!fGen || !aGen)
      return false;
    if (normalizeExtBaseName(fBase) != normalizeExtBaseName(aBase))
      return false;
    if (fArgs.size() != aArgs.size())
      return false;
    for (size_t i = 0; i < fArgs.size(); i++) {
      if (!unifyGenericType(fArgs[i], aArgs[i], typeParams, inferred))
        return false;
    }
    return true;
  }

  return formal == actual;
}

// Look up the return type of a function by name.
static std::string lookupFuncReturnType(const std::string &funcName,
                                        const FuncLookupCtx *flc) {
  if (!flc)
    return "";

  static const std::unordered_map<std::string, std::string>
      globalBuiltinReturns = {
          {"typeof", "string"},      {"toString", "string"},
          {"fromCStr", "string"},    {"fromCStrCopy", "string"},
          {"fromCStrLen", "string"}, {"sprintf", "string"},
          {"sizeof", "int64"},       {"toInt", "int64"},
          {"toFloat", "float64"},    {"toBool", "bool"}};
  if (auto it = globalBuiltinReturns.find(funcName);
      it != globalBuiltinReturns.end())
    return it->second;

  if (flc->bindings) {
    auto *cfunc = flc->bindings->findFunction(funcName);
    if (cfunc && cfunc->returnType)
      return cfunc->returnType->name;
  }
  if (flc->tree) {
    for (auto *tld : flc->tree->topLevelDecl()) {
      if (auto *fd = tld->functionDecl()) {
        if (!fd->IDENTIFIER().empty() && safeIdAt(fd, 0) == funcName &&
            fd->typeSpec())
          return safeText(fd->typeSpec());
      }
      if (auto *ext = tld->externDecl()) {
        if (ext->IDENTIFIER() && safeText(ext->IDENTIFIER()) == funcName &&
            ext->typeSpec()) {
          auto ret = safeText(ext->typeSpec());
          if (ret != "auto")
            return ret;
        }
      }
    }
  }
  // Cross-file functions (imported via `use`)
  if (flc->project && flc->project->isValid()) {
    for (auto &ns : flc->project->registry().allModules()) {
      auto *sym = flc->project->registry().findSymbol(ns, funcName);
      if (!sym)
        continue;
      if (sym->kind == ExportedSymbol::Function) {
        auto *fd = dynamic_cast<LucisParser::FunctionDeclContext *>(sym->decl);
        if (fd)
          return safeText(fd->typeSpec());
      }
    }
  }
  if (flc->builtinReg) {
    auto *builtin = flc->builtinReg->lookup(funcName);
    if (builtin)
      return builtin->returnType;
  }
  return "";
}

// Infer the type name of an expression for auto variable resolution.
static std::string inferExprTypeName(
    LucisParser::ExpressionContext *expr,
    const std::unordered_map<std::string, CompletionProvider::LocalVar> &locals,
    const FuncLookupCtx *flc = nullptr) {
  if (!expr)
    return "";
  // Suffixed literals
  auto suffixedTypeName = [](const std::string& text) -> std::string {
    static const std::unordered_map<std::string, std::string> kSuf = {
      {"i8","int8"},{"i16","int16"},{"i32","int32"},{"i64","int64"},
      {"i128","int128"},{"iinf","intinf"},{"isize","isize"},
      {"u8","uint8"},{"u16","uint16"},{"u32","uint32"},{"u64","uint64"},
      {"u128","uint128"},{"usize","usize"},
      {"f32","float32"},{"f64","float64"},{"f80","float80"},{"f128","float128"}
    };
    for (auto& [suf, tn] : kSuf) {
      if (text.size() > suf.size() &&
          text.compare(text.size() - suf.size(), suf.size(), suf) == 0)
        return tn;
    }
    return "";
  };
  if (auto* si = dynamic_cast<LucisParser::SuffixedIntLitExprContext*>(expr)) {
    auto r = suffixedTypeName(si->SUFFIXED_INT()->getText());
    if (!r.empty()) return r;
  }
  if (auto* sh = dynamic_cast<LucisParser::SuffixedHexLitExprContext*>(expr)) {
    auto r = suffixedTypeName(sh->SUFFIXED_HEX()->getText());
    if (!r.empty()) return r;
  }
  if (auto* so = dynamic_cast<LucisParser::SuffixedOctLitExprContext*>(expr)) {
    auto r = suffixedTypeName(so->SUFFIXED_OCT()->getText());
    if (!r.empty()) return r;
  }
  if (auto* sb = dynamic_cast<LucisParser::SuffixedBinLitExprContext*>(expr)) {
    auto r = suffixedTypeName(sb->SUFFIXED_BIN()->getText());
    if (!r.empty()) return r;
  }
  if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatLitExprContext*>(expr)) {
    auto r = suffixedTypeName(sf->SUFFIXED_FLOAT()->getText());
    if (!r.empty()) return r;
  }
  if (auto* sd = dynamic_cast<LucisParser::SuffixedLeadingDotFloatExprContext*>(expr)) {
    auto r = suffixedTypeName(sd->SUFFIXED_DOT_FLOAT()->getText());
    if (!r.empty()) return r;
  }
  if (auto* si = dynamic_cast<LucisParser::SuffixedIntFloatExprContext*>(expr)) {
    auto r = suffixedTypeName(si->SUFFIXED_INT_FLOAT()->getText());
    if (!r.empty()) return r;
  }
  if (auto* sf = dynamic_cast<LucisParser::SuffixedFloatIntExprContext*>(expr)) {
    auto r = suffixedTypeName(sf->SUFFIXED_FLOAT_INT()->getText());
    if (!r.empty()) return r;
  }
  if (dynamic_cast<LucisParser::IntLitExprContext *>(expr) ||
      dynamic_cast<LucisParser::HexLitExprContext *>(expr) ||
      dynamic_cast<LucisParser::OctLitExprContext *>(expr) ||
      dynamic_cast<LucisParser::BinLitExprContext *>(expr))
    return "int32";
  if (dynamic_cast<LucisParser::FloatLitExprContext *>(expr))
    return "float64";
  if (dynamic_cast<LucisParser::BoolLitExprContext *>(expr))
    return "bool";
  if (dynamic_cast<LucisParser::CharLitExprContext *>(expr))
    return "char";
  if (dynamic_cast<LucisParser::StrLitExprContext *>(expr))
    return "string";
  if (dynamic_cast<LucisParser::CStrLitExprContext *>(expr))
    return "*char";

  // ── Inline assembly expression ────────────────────────────
  if (auto* asmE = dynamic_cast<LucisParser::AsmExprContext*>(expr)) {
    auto* outList = asmE->asmOutputList();
    if (outList && !outList->asmOutput().empty()) {
      auto* output = outList->asmOutput()[0];
      if (auto* ident = output->IDENTIFIER()) {
        auto it = locals.find(safeText(ident));
        if (it != locals.end()) return it->second.typeName;
      }
      return "int64";
    }
    return "void";
  }

  if (auto *id = dynamic_cast<LucisParser::IdentExprContext *>(expr)) {
    auto it = locals.find(safeText(id->IDENTIFIER()));
    if (it != locals.end())
      return it->second.typeName;
    return "";
  }

  auto resolveFieldType = [&](const std::string &ownerType,
                              const std::string &fieldName) -> std::string {
    if (ownerType.empty())
      return "";

    std::string lookupType = ownerType;
    std::unordered_map<std::string, std::string> subst;
    std::string base;
    std::vector<std::string> args;
    if (parseGenericInstance(ownerType, base, args)) {
      lookupType = base;
    }

    if (auto *sd = findStructDeclForInference(lookupType, flc)) {
      if (!args.empty() && sd->typeParamList()) {
        auto tps = sd->typeParamList()->typeParam();
        for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
          auto ids = tps[i]->IDENTIFIER();
          if (!ids.empty())
            subst[ids[0]->getText()] = args[i];
        }
      }
      for (auto *f : sd->structField()) {
        if (safeText(f->IDENTIFIER()) == fieldName)
          return substituteTypeParams(safeText(f->typeSpec()), subst);
      }
    }

    if (auto *ud = findUnionDeclForInference(lookupType, flc)) {
      if (!args.empty() && ud->typeParamList()) {
        auto tps = ud->typeParamList()->typeParam();
        for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
          auto ids = tps[i]->IDENTIFIER();
          if (!ids.empty())
            subst[ids[0]->getText()] = args[i];
        }
      }
      for (auto *f : ud->unionField()) {
        if (safeText(f->IDENTIFIER()) == fieldName)
          return substituteTypeParams(safeText(f->typeSpec()), subst);
      }
    }

    if (flc && flc->bindings) {
      if (auto *cs = flc->bindings->findStruct(lookupType)) {
        for (auto &f : cs->fields) {
          if (f.name == fieldName && f.typeInfo)
            return f.typeInfo->name;
        }
      }
    }

    return "";
  };

  if (auto *fa = dynamic_cast<LucisParser::FieldAccessExprContext *>(expr)) {
    auto baseType = inferExprTypeName(fa->expression(), locals, flc);
    if (baseType.empty())
      return "";
    return resolveFieldType(baseType, safeText(fa->IDENTIFIER()));
  }

  if (auto *aa = dynamic_cast<LucisParser::ArrowAccessExprContext *>(expr)) {
    auto baseType = inferExprTypeName(aa->expression(), locals, flc);
    if (baseType.empty() || baseType[0] != '*')
      return "";
    return resolveFieldType(baseType.substr(1), safeText(aa->IDENTIFIER()));
  }
  if (auto *addr = dynamic_cast<LucisParser::AddrOfExprContext *>(expr)) {
    if (auto *ident =
            dynamic_cast<LucisParser::IdentExprContext *>(addr->expression())) {
      auto it = locals.find(safeText(ident->IDENTIFIER()));
      if (it != locals.end())
        return "*" + it->second.typeName;
    }
    return "";
  }
  // Function call: resolve return type
  if (auto *fn = dynamic_cast<LucisParser::FnCallExprContext *>(expr)) {
    if (auto *ident =
            dynamic_cast<LucisParser::IdentExprContext *>(fn->expression())) {
      auto funcName = safeText(ident->IDENTIFIER());
      if (auto *fd = findFunctionDeclForInference(funcName, flc)) {
        // Non-generic function: direct return type.
        if (!fd->typeParamList()) {
          auto ret = safeText(fd->typeSpec());
          if (!ret.empty())
            return ret;
        } else {
          // Generic function: infer type params from call arguments.
          std::unordered_set<std::string> tps;
          for (auto *tp : fd->typeParamList()->typeParam()) {
            auto ids = tp->IDENTIFIER();
            if (!ids.empty())
              tps.insert(ids[0]->getText());
          }

          std::vector<std::string> actualArgTypes;
          if (auto *al = fn->argList()) {
            for (auto *a : al->expression())
              actualArgTypes.push_back(inferExprTypeName(a, locals, flc));
          }

          std::vector<LucisParser::ParamContext *> formalParams;
          if (auto *pl = fd->paramList())
            formalParams = pl->param();
          if (actualArgTypes.size() == formalParams.size()) {
            std::unordered_map<std::string, std::string> inferred;
            bool ok = true;
            for (size_t i = 0; i < formalParams.size(); i++) {
              auto formal = safeText(formalParams[i]->typeSpec());
              auto actual = actualArgTypes[i];
              if (actual.empty()) {
                ok = false;
                break;
              }
              if (!unifyGenericType(formal, actual, tps, inferred)) {
                ok = false;
                break;
              }
            }

            for (auto &tp : tps) {
              if (!inferred.count(tp)) {
                ok = false;
                break;
              }
            }

            if (ok) {
              auto ret =
                  substituteTypeParams(safeText(fd->typeSpec()), inferred);
              if (!ret.empty())
                return ret;
            }
          }
        }
      }

      auto ret = lookupFuncReturnType(funcName, flc);
      if (!ret.empty())
        return ret;
    }
    return "";
  }

  // Generic function call: d<string>(...) → substitute explicit type args
  if (auto *gfc = dynamic_cast<LucisParser::GenericFnCallExprContext *>(expr)) {
    auto *fnId = gfc->IDENTIFIER();
    if (!fnId)
      return "";
    std::string funcName = fnId->getText();

    if (auto *fd = findFunctionDeclForInference(funcName, flc)) {
      if (!fd->typeParamList()) {
        auto ret = safeText(fd->typeSpec());
        if (!ret.empty())
          return ret;
      } else {
        std::unordered_map<std::string, std::string> subst;
        auto tps = fd->typeParamList()->typeParam();
        auto args = gfc->typeSpec();
        for (size_t i = 0; i < std::min(tps.size(), args.size()); i++) {
          auto ids = tps[i]->IDENTIFIER();
          if (!ids.empty())
            subst[ids[0]->getText()] = args[i]->getText();
        }

        auto ret = substituteTypeParams(safeText(fd->typeSpec()), subst);
        if (!ret.empty())
          return ret;
      }
    }

    auto ret = lookupFuncReturnType(funcName, flc);
    if (!ret.empty())
      return ret;
    return "";
  }
  // Enum access: Direction::North → "Direction"
  if (auto *ea = dynamic_cast<LucisParser::EnumAccessExprContext *>(expr)) {
    auto ids = ea->IDENTIFIER();
    if (!ids.empty())
      return ids[0]->getText();
    return "";
  }
  // Generic enum access: Result<int32>::Ok → "Result<int32>"
  if (auto *gea = dynamic_cast<LucisParser::GenericEnumAccessExprContext *>(expr)) {
    auto ids = gea->IDENTIFIER();
    if (ids.empty()) return "";
    std::string outType = ids[0]->getText() + "<";
    auto args = gea->typeSpec();
    for (size_t i = 0; i < args.size(); i++) {
      if (i > 0) outType += ",";
      outType += args[i]->getText();
    }
    outType += ">";
    return outType;
  }

  // Generic static method call: Result<int32>::Ok(42) -> "Result<int32>"
  if (auto *gsmc = dynamic_cast<LucisParser::GenericStaticMethodCallExprContext *>(expr)) {
    auto ids = gsmc->IDENTIFIER();
    if (ids.empty()) return "";
    std::string outType = ids[0]->getText() + "<";
    auto args = gsmc->typeSpec();
    for (size_t i = 0; i < args.size(); i++) {
      if (i > 0) outType += ",";
      outType += args[i]->getText();
    }
    outType += ">";
    return outType;
  }
  // Static method call: Type::method(...) → return enum type or method type
  if (auto *smc =
          dynamic_cast<LucisParser::StaticMethodCallExprContext *>(expr)) {
    auto ids = smc->IDENTIFIER();
    if (ids.size() >= 2) {
      std::string ownerPath;
      for (size_t i = 0; i + 1 < ids.size(); ++i) {
        if (!ownerPath.empty())
          ownerPath += "::";
        ownerPath += ids[i]->getText();
      }

      // Check if owner is an enum type (e.g. Result::Ok(42))
      auto typeName = ownerPath;
      auto lastScope = typeName.rfind("::");
      if (lastScope != std::string::npos)
        typeName = typeName.substr(lastScope + 2);
      if (flc && flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
          if (auto* e = tld->enumDecl();
              e && e->IDENTIFIER() && safeText(e->IDENTIFIER()) == typeName)
            return typeName;
        }
      }
      // Cross-file enum check
      if (flc && flc->project && flc->project->isValid()) {
        for (auto& ns : flc->project->registry().allModules()) {
          auto* sym = flc->project->registry().findSymbol(ns, typeName);
          if (sym && sym->kind == ExportedSymbol::Enum) return typeName;
        }
      }

      std::string methodName = ids.back()->getText();

      if (ImportResolver::isStdModule(ownerPath) && flc && flc->builtinReg) {
        if (auto *sig = flc->builtinReg->lookup(methodName))
          return sig->returnType;
      }

      if (flc && flc->tree) {
        for (auto *tld : flc->tree->topLevelDecl()) {
          auto *ext = tld->extendDecl();
          if (!ext)
            continue;
          if (safeText(ext->IDENTIFIER()) != typeName)
            continue;
          for (auto *m : ext->extendMethod()) {
            if (safeIdAt(m, 0) != methodName)
              continue;

            if (!ext->typeParamList())
              return safeText(m->typeSpec());

            std::unordered_set<std::string> tps;
            for (auto *tp : ext->typeParamList()->typeParam()) {
              auto tpIds = tp->IDENTIFIER();
              if (!tpIds.empty())
                tps.insert(tpIds[0]->getText());
            }

            std::vector<std::string> actualArgTypes;
            if (auto *al = smc->argList()) {
              for (auto *a : al->expression())
                actualArgTypes.push_back(inferExprTypeName(a, locals, flc));
            }

            std::vector<LucisParser::ParamContext *> formalParams;
            if (auto *pl = m->paramList())
              formalParams = pl->param();
            if (actualArgTypes.size() != formalParams.size())
              return safeText(m->typeSpec());

            std::unordered_map<std::string, std::string> inferred;
            bool ok = true;
            for (size_t i = 0; i < formalParams.size(); i++) {
              auto formal = safeText(formalParams[i]->typeSpec());
              auto actual = actualArgTypes[i];
              if (actual.empty()) {
                ok = false;
                break;
              }
              if (!unifyGenericType(formal, actual, tps, inferred)) {
                ok = false;
                break;
              }
            }
            for (auto &tp : tps) {
              if (!inferred.count(tp)) {
                ok = false;
                break;
              }
            }

            if (ok)
              return substituteTypeParams(safeText(m->typeSpec()), inferred);
            return safeText(m->typeSpec());
          }
        }
      }
      if (flc && flc->project && flc->project->isValid()) {
        for (auto &ns : flc->project->registry().allModules()) {
          auto syms = flc->project->registry().getModuleSymbols(ns);
          for (auto *sym : syms) {
            if (sym->kind != ExportedSymbol::ExtendBlock)
              continue;
            auto *ext = dynamic_cast<LucisParser::ExtendDeclContext *>(sym->decl);
            if (!ext || safeText(ext->IDENTIFIER()) != typeName)
              continue;
            for (auto *m : ext->extendMethod()) {
              if (safeIdAt(m, 0) == methodName)
                return safeText(m->typeSpec());
            }
          }
        }
      }
      if (flc && flc->extTypeReg) {
        auto *desc = flc->extTypeReg->lookup(typeName);
        if (desc) {
          for (auto &md : desc->methods) {
            if (md.name == methodName)
              return md.returnType;
          }
        }
      }
    }
    return "";
  }
  // Method call: resolve receiver type, then look up method return type
  if (auto *mc = dynamic_cast<LucisParser::MethodCallExprContext *>(expr)) {
    auto exprBase = mc->expression();
    std::string receiverType = inferExprTypeName(exprBase, locals, flc);

    // If base expression is an array access, resolve to element type
    if (auto* idx = dynamic_cast<LucisParser::IndexExprContext*>(exprBase)) {
        auto baseType = inferExprTypeName(idx->expression(0), locals, flc);
        receiverType = resolveIndexedElementType(baseType);
    }

    if (!receiverType.empty()) {
      std::string methodName = safeText(mc->IDENTIFIER());

      if (!receiverType.empty() && receiverType[0] == '[' && flc &&
          flc->methodReg) {
        auto *md = flc->methodReg->lookupArrayMethod(methodName);
        if (md) {
          auto arrayElemType = [](const std::string &t) -> std::string {
            if (t.rfind("[]", 0) == 0)
              return t.substr(2);
            if (!t.empty() && t[0] == '[') {
              auto close = t.find(']');
              if (close != std::string::npos && close + 1 < t.size())
                return t.substr(close + 1);
            }
            return "";
          };

          if (md->returnType == "_self")
            return receiverType;
          if (md->returnType == "_elem")
            return arrayElemType(receiverType);
          return md->returnType;
        }
      }

      // 1) Check user-defined extend methods
      if (flc && flc->tree) {
        for (auto *tld : flc->tree->topLevelDecl()) {
          auto *ext = tld->extendDecl();
          if (!ext)
            continue;
          if (safeText(ext->IDENTIFIER()) != receiverType)
            continue;
          for (auto *m : ext->extendMethod()) {
            if (safeIdAt(m, 0) == methodName && m->typeSpec())
              return safeText(m->typeSpec());
          }
        }
      }

      // 1b) Check cross-file extend methods
      if (flc && flc->project && flc->project->isValid()) {
        for (auto &ns : flc->project->registry().allModules()) {
          auto syms = flc->project->registry().getModuleSymbols(ns);
          for (auto *sym : syms) {
            if (sym->kind != ExportedSymbol::ExtendBlock)
              continue;
            auto *ext = dynamic_cast<LucisParser::ExtendDeclContext *>(sym->decl);
            if (!ext || safeText(ext->IDENTIFIER()) != receiverType)
              continue;
            for (auto *m : ext->extendMethod()) {
              if (safeIdAt(m, 0) == methodName && m->typeSpec())
                return safeText(m->typeSpec());
            }
          }
        }
      }

      // 1c) Check struct function-pointer fields
      if (flc && (flc->tree || flc->project)) {
        auto structRet = TypeInferrer::resolveMethodReturnTypeViaStructField(
            receiverType, methodName, flc->tree, flc->project);
        if (!structRet.empty()) return structRet;
      }

      // 2) Check builtin extended type methods (Vec, Map, Set, etc.)
      if (flc && flc->extTypeReg) {
        std::string baseName = receiverType;
        std::vector<std::string> typeArgs;
        auto ltPos = receiverType.find('<');
        if (ltPos != std::string::npos) {
          baseName = receiverType.substr(0, ltPos);
          auto gtPos = receiverType.rfind('>');
          if (gtPos != std::string::npos) {
            std::string inner =
                receiverType.substr(ltPos + 1, gtPos - ltPos - 1);
            int depth = 0;
            size_t start = 0;
            for (size_t i = 0; i <= inner.size(); ++i) {
              if (i == inner.size() || (inner[i] == ',' && depth == 0)) {
                auto arg = inner.substr(start, i - start);
                size_t b = arg.find_first_not_of(' ');
                size_t e = arg.find_last_not_of(' ');
                if (b != std::string::npos)
                  typeArgs.push_back(arg.substr(b, e - b + 1));
                start = i + 1;
              } else if (inner[i] == '<')
                ++depth;
              else if (inner[i] == '>')
                --depth;
            }
          }
        }

        auto *desc = flc->extTypeReg->lookup(normalizeExtBaseName(baseName));
        if (desc) {
          for (auto &md : desc->methods) {
            if (md.name != methodName)
              continue;
            auto &rt = md.returnType;
            if (rt == "_self")
              return receiverType;
            if (rt == "_elem" && !typeArgs.empty())
              return typeArgs[0];
            if (rt == "_key" && !typeArgs.empty())
              return typeArgs[0];
            if (rt == "_val" && typeArgs.size() >= 2)
              return typeArgs[1];
            if (rt == "_val" && typeArgs.size() == 1)
              return typeArgs[0];
            if (rt == "_vec_key" && !typeArgs.empty())
              return "vec<" + typeArgs[0] + ">";
            if (rt == "_vec_val" && typeArgs.size() >= 2)
              return "vec<" + typeArgs[1] + ">";
            return rt;
          }
        }
      }
    }
    return "";
  }
  if (auto *deref = dynamic_cast<LucisParser::DerefExprContext *>(expr)) {
    auto inner = inferExprTypeName(deref->expression(), locals, flc);
    if (inner.size() > 1 && inner[0] == '*')
      return inner.substr(1);
    return "";
  }
  if (auto *neg = dynamic_cast<LucisParser::NegExprContext *>(expr))
    return inferExprTypeName(neg->expression(), locals, flc);
  if (dynamic_cast<LucisParser::LogicalNotExprContext *>(expr))
    return "bool";
  if (dynamic_cast<LucisParser::RelExprContext *>(expr))
    return "bool";
  if (dynamic_cast<LucisParser::EqExprContext *>(expr))
    return "bool";
  if (dynamic_cast<LucisParser::LogicalAndExprContext *>(expr))
    return "bool";
  if (dynamic_cast<LucisParser::LogicalOrExprContext *>(expr))
    return "bool";
  if (auto *mul = dynamic_cast<LucisParser::MulExprContext *>(expr))
    return inferExprTypeName(mul->expression(0), locals, flc);
  if (auto *add = dynamic_cast<LucisParser::AddSubExprContext *>(expr))
    return inferExprTypeName(add->expression(0), locals, flc);
  if (auto *paren = dynamic_cast<LucisParser::ParenExprContext *>(expr))
    return inferExprTypeName(paren->expression(), locals, flc);
  if (auto *cast = dynamic_cast<LucisParser::CastExprContext *>(expr))
    return cast->typeSpec() ? safeText(cast->typeSpec()) : "";
  if (auto *tern = dynamic_cast<LucisParser::TernaryExprContext *>(expr))
    return inferExprTypeName(tern->expression(1), locals, flc);
  if (auto *cu = dynamic_cast<LucisParser::CatchUnwrapExprContext *>(expr)) {
    auto *tree = flc ? flc->tree : nullptr;
    auto inferred =
        inferCatchUnwrapSuccessType(cu->expression(), tree, locals, flc);
    if (!inferred.empty())
      return inferred;
    return "";
  }
  // ── Propagate operator: expr? — success payload type ──────────────
  if (auto* pe = dynamic_cast<LucisParser::PropagateExprContext*>(expr)) {
    auto* tree = flc ? flc->tree : nullptr;
    auto inferred = inferCatchUnwrapSuccessType(pe->expression(), tree, locals, flc);
    if (!inferred.empty()) return inferred;
    return "";
  }
  // ── Try expression: try expr or fallback — unwrap Ok payload ──────
  if (auto* te = dynamic_cast<LucisParser::TryExprContext*>(expr)) {
    auto* tree = flc ? flc->tree : nullptr;
    auto inferred = inferCatchUnwrapSuccessType(te->expression(0), tree, locals, flc);
    if (!inferred.empty()) return inferred;
    return inferExprTypeName(te->expression(0), locals, flc);
  }

  // ── Match expression: match expr { ... } — type from enum ─────────
  if (auto* me = dynamic_cast<LucisParser::MatchExprContext*>(expr)) {
    auto matchedType = inferExprTypeName(me->expression(), locals, flc);
    if (!matchedType.empty() && flc) {
      std::string baseName = matchedType;
      std::vector<std::string> sourceArgs;
      parseGenericInstance(matchedType, baseName, sourceArgs);
      auto* ed = findEnumDeclForInference(baseName, flc);
      if (!ed && flc->tree) {
        for (auto* tld : flc->tree->topLevelDecl()) {
          if (auto* e = tld->enumDecl();
              e && e->IDENTIFIER() && safeText(e->IDENTIFIER()) == baseName)
            { ed = e; break; }
        }
      }
      if (ed) {
        for (auto* variant : ed->enumVariant()) {
          if (!variant || variant->typeSpec().empty()) continue;
          auto varName = variant->IDENTIFIER()
                             ? variant->IDENTIFIER()->getText() : "";
          auto payloadType = safeText(variant->typeSpec(0));
          if (varName != "Err" && payloadType != "Error") {
            if (!sourceArgs.empty() && ed->typeParamList()) {
              std::unordered_map<std::string, std::string> subst;
              auto tps = ed->typeParamList()->typeParam();
              for (size_t i = 0; i < std::min(tps.size(), sourceArgs.size()); i++) {
                auto ids = tps[i]->IDENTIFIER();
                if (!ids.empty()) subst[ids[0]->getText()] = sourceArgs[i];
              }
              payloadType = substituteTypeParams(payloadType, subst);
            }
            return payloadType;
          }
        }
      }
    }
    return "";
  }

  if (auto *arr = dynamic_cast<LucisParser::ArrayLitExprContext *>(expr)) {
    auto elems = arr->expression();
    if (!elems.empty()) {
      auto elemType = inferExprTypeName(elems[0], locals, flc);
      if (!elemType.empty())
        return "[]" + elemType;
    }
    return "";
  }
  if (dynamic_cast<LucisParser::SizeofExprContext *>(expr))
    return "int64";
  if (dynamic_cast<LucisParser::TypeofExprContext *>(expr))
    return "string";

  // Index: expr[i] → element type
  if (auto *idx = dynamic_cast<LucisParser::IndexExprContext *>(expr)) {
    auto exprs = idx->expression();
    if (!exprs.empty()) {
      auto baseType = inferExprTypeName(exprs[0], locals, flc);
      if (baseType == "string")
        return "char";
      // Array type [N]T or []T: strip leading [...]
      if (!baseType.empty() && baseType[0] == '[') {
        auto close = baseType.find(']');
        if (close != std::string::npos && close + 1 < baseType.size())
          return baseType.substr(close + 1);
      }
    }
    return "";
  }

  // Positional struct/union init: Person { "Carlos" }
  if (auto *spi = dynamic_cast<LucisParser::StructPosInitExprContext *>(expr)) {
    if (spi->IDENTIFIER()) return spi->IDENTIFIER()->getText();
    return "";
  }

  // Qualified positional init: mod::Person { "Carlos" }
  if (auto *qpi = dynamic_cast<LucisParser::QualifiedStructPosInitExprContext *>(expr)) {
    auto ids = qpi->IDENTIFIER();
    if (ids.size() >= 2) return ids[0]->getText();
    return "";
  }

  // Struct literal: Point { x: 10, y: 20 } → "Point"
  if (auto *sl = dynamic_cast<LucisParser::StructLitExprContext *>(expr)) {
    auto ids = sl->IDENTIFIER();
    if (!ids.empty())
      return ids[0]->getText();
    return "";
  }

  // Qualified named init: mod::Person { name: "Carlos" }
  if (auto *qni = dynamic_cast<LucisParser::QualifiedStructNamedInitExprContext *>(expr)) {
    auto ids = qni->IDENTIFIER();
    if (ids.size() >= 2) return ids[0]->getText();
    return "";
  }

  // Generic positional init: Result<int32> { 42 }
  if (auto *gpi = dynamic_cast<LucisParser::GenericStructPosInitExprContext *>(expr)) {
    if (gpi->IDENTIFIER()) {
      std::string base = gpi->IDENTIFIER()->getText();
      std::string outType = base + "<";
      auto args = gpi->typeSpec();
      for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) outType += ",";
        outType += args[i]->getText();
      }
      outType += ">";
      return outType;
    }
    return "";
  }

  // Generic struct/union literal: Result<int32, string> { ... } →
  // "Result<int32,string>"
  if (auto *gsl =
          dynamic_cast<LucisParser::GenericStructLitExprContext *>(expr)) {
    if (!gsl->IDENTIFIER().empty()) {
      std::string base = safeIdAt(gsl, 0);
      std::string outType = base + "<";
      auto args = gsl->typeSpec();
      for (size_t i = 0; i < args.size(); i++) {
        if (i > 0)
          outType += ",";
        outType += args[i]->getText();
      }
      outType += ">";
      return outType;
    }
    return "";
  }
  return "";
}

static void collectLocalsFromStmts(
    const std::vector<LucisParser::StatementContext *> &stmts, size_t beforeLine,
    std::unordered_map<std::string, CompletionProvider::LocalVar> &out,
    const FuncLookupCtx *flc = nullptr) {
  for (auto *stmt : stmts) {
    auto *start = stmt->getStart();
    if (start && start->getLine() > beforeLine + 1)
      break;

    if (auto *vd = stmt->varDeclStmt()) {
      std::string typeName;
      if (vd->typeSpec())
        typeName = safeText(vd->typeSpec());

      if (vd->LPAREN()) {
        for (auto* id : vd->IDENTIFIER()) {
          std::string varName = id->getText();
          out[varName] = {typeName, 0};
        }
      } else {
        // Resolve auto type from last init declarator for propagation
        std::string resolvedAutoType;
        if (typeName == "auto") {
          for (auto it = vd->varDeclarator().rbegin();
               it != vd->varDeclarator().rend(); ++it) {
            if ((*it)->expression()) {
              resolvedAutoType = inferExprTypeName((*it)->expression(), out, flc);
              break;
            }
          }
        }
        for (auto* d : vd->varDeclarator()) {
          std::string varType = typeName;
          std::string varName = safeText(d->IDENTIFIER());
          if (varType == "auto" && d->expression()) {
            auto inferred = inferExprTypeName(d->expression(), out, flc);
            if (!inferred.empty())
              varType = inferred;
          } else if (varType == "auto" && !resolvedAutoType.empty()) {
            varType = resolvedAutoType;
          }
          if (!varName.empty())
            out[varName] = {varType, 0};
        }
      }

      if (auto *cu = dynamic_cast<LucisParser::CatchUnwrapExprContext *>(
              vd->expression())) {
        if (cursorInsideNode(cu->block(), beforeLine)) {
          auto itType = inferCatchUnwrapItType(cu->expression(), flc ? flc->tree : nullptr, out, flc);
          out["it"] = {itType, 0};
          collectLocalsFromBlock(cu->block(), beforeLine, out, flc);
        }
      }

      // Match expression: inject pattern bindings
      if (auto* me = dynamic_cast<LucisParser::MatchExprContext*>(vd->expression())) {
        auto matchedType = inferExprTypeName(me->expression(), out, flc);
        if (!matchedType.empty() && flc && flc->tree) {
          std::string baseName = matchedType;
          auto lt = baseName.find('<');
          if (lt != std::string::npos) baseName = baseName.substr(0, lt);
          auto* ed = findEnumDeclForInference(baseName, flc);
          if (ed) {
            for (auto* arm : me->matchArm()) {
              bool inArm = false;
              if (arm->block() && cursorInsideNode(arm->block(), beforeLine)) {
                inArm = true;
              } else if (!arm->block()) {
                auto armExprs = arm->expression();
                auto* bodyExpr = armExprs.empty() ? nullptr : armExprs.back();
                if (bodyExpr && cursorInsideNode(bodyExpr, beforeLine))
                  inArm = true;
              }
              if (inArm) {
                std::unordered_map<std::string, std::string> subst;
                if (!matchedType.empty() && ed->typeParamList()) {
                  std::string base;
                  std::vector<std::string> args;
                  if (parseGenericInstance(matchedType, base, args)) {
                    auto tps = ed->typeParamList()->typeParam();
                    for (size_t i = 0; i < std::min(tps.size(), args.size()); i++) {
                      auto tids = tps[i]->IDENTIFIER();
                      if (!tids.empty()) subst[tids[0]->getText()] = args[i];
                    }
                  }
                }
                for (size_t pi = 0; pi < arm->pattern().size(); pi++) {
                  auto* p = arm->pattern(pi);
                  if (p->LPAREN() && p->IDENTIFIER().size() >= 1) {
                    std::string bindName = p->IDENTIFIER().back()->getText();
                    if (bindName != "_" && !bindName.empty()) {
                      std::string vname;
                      if (p->SCOPE() && p->IDENTIFIER().size() >= 2)
                        vname = p->IDENTIFIER(1)->getText();
                      else
                        vname = p->IDENTIFIER(0)->getText();
                      for (auto* v : ed->enumVariant()) {
                        if (safeText(v->IDENTIFIER()) == vname && !v->typeSpec().empty()) {
                          std::string payloadType = safeText(v->typeSpec(0));
                          if (!subst.empty())
                            payloadType = substituteTypeParams(payloadType, subst);
                          out[bindName] = {payloadType, 0};
                          break;
                        }
                      }
                    }
                  }
                }
                if (arm->block())
                  collectLocalsFromBlock(arm->block(), beforeLine, out, flc);
                break;
              }
            }
          }
        }
      }
    }

    // Structural blocks
    if (auto *nb = stmt->nakedBlockStmt()) {
      // Lexical scope: only visible while cursor is inside the block.
      if (cursorInsideNode(nb, beforeLine)) {
        auto scoped = out;
        collectLocalsFromStmts(nb->statement(), beforeLine, scoped, flc);
        out = std::move(scoped);
      }
    }
    if (auto *ib = stmt->inlineBlockStmt()) {
      // Inline block injects declarations into parent scope.
      collectLocalsFromStmts(ib->statement(), beforeLine, out, flc);
    }
    if (auto *sb = stmt->scopeBlockStmt()) {
      // Scope block is lexical: descend only when cursor is inside.
      if (cursorInsideNode(sb, beforeLine)) {
        auto scoped = out;
        collectLocalsFromStmts(sb->statement(), beforeLine, scoped, flc);
        out = std::move(scoped);
      }
    }

    if (auto *ifS = stmt->ifStmt()) {
      if (auto *body = ifS->ifBody()) {
        if (auto *b = body->block()) {
          if (cursorInsideNode(b, beforeLine)) {
            if (auto *isE =
                    dynamic_cast<LucisParser::IsExprContext *>(ifS->expression());
                isE && isE->IDENTIFIER(1)) {
              auto bindType = inferIsBindingPayloadType(
                  ifS->expression(), flc ? flc->tree : nullptr);
              out[safeIdAt(isE, 1)] = {
                  bindType.empty() ? "auto" : bindType, 0};
            }
            collectLocalsFromBlock(b, beforeLine, out, flc);
          }
        } else if (auto *s = body->statement()) {
          if (cursorInsideNode(s, beforeLine)) {
            if (auto *isE =
                    dynamic_cast<LucisParser::IsExprContext *>(ifS->expression());
                isE && isE->IDENTIFIER(1)) {
              auto bindType = inferIsBindingPayloadType(
                  ifS->expression(), flc ? flc->tree : nullptr);
              out[safeIdAt(isE, 1)] = {
                  bindType.empty() ? "auto" : bindType, 0};
            }
            collectLocalsFromStmts({s}, beforeLine, out, flc);
          }
        }
      }
      for (auto *elif : ifS->elseIfClause()) {
        if (auto *body = elif->ifBody()) {
          if (auto *b = body->block()) {
            if (cursorInsideNode(b, beforeLine)) {
              if (auto *isE = dynamic_cast<LucisParser::IsExprContext *>(
                      elif->expression());
                  isE && isE->IDENTIFIER(1)) {
                auto bindType = inferIsBindingPayloadType(
                    elif->expression(), flc ? flc->tree : nullptr);
                out[safeIdAt(isE, 1)] = {
                    bindType.empty() ? "auto" : bindType, 0};
              }
              collectLocalsFromBlock(b, beforeLine, out, flc);
            }
          } else if (auto *s = body->statement()) {
            if (cursorInsideNode(s, beforeLine)) {
              if (auto *isE = dynamic_cast<LucisParser::IsExprContext *>(
                      elif->expression());
                  isE && isE->IDENTIFIER(1)) {
                auto bindType = inferIsBindingPayloadType(
                    elif->expression(), flc ? flc->tree : nullptr);
                out[safeIdAt(isE, 1)] = {
                    bindType.empty() ? "auto" : bindType, 0};
              }
              collectLocalsFromStmts({s}, beforeLine, out, flc);
            }
          }
        }
      }
      if (ifS->elseClause()) {
        if (auto *body = ifS->elseClause()->ifBody()) {
          if (auto *b = body->block()) {
            if (cursorInsideNode(b, beforeLine))
              collectLocalsFromBlock(b, beforeLine, out, flc);
          } else if (auto *s = body->statement()) {
            if (cursorInsideNode(s, beforeLine))
              collectLocalsFromStmts({s}, beforeLine, out, flc);
          }
        }
      }
    }
    if (auto *forS = stmt->forStmt()) {
      if (auto *fin = dynamic_cast<LucisParser::ForInStmtContext *>(forS)) {
        if (cursorInsideNode(fin, beforeLine)) {
          if (fin->IDENTIFIER() && fin->typeSpec())
            out[safeText(fin->IDENTIFIER())] = {safeText(fin->typeSpec()), 0};
          collectLocalsFromBlock(fin->block(), beforeLine, out, flc);
        }
      }
      if (auto *fc = dynamic_cast<LucisParser::ForClassicStmtContext *>(forS)) {
        if (cursorInsideNode(fc, beforeLine)) {
          if (fc->IDENTIFIER() && fc->typeSpec())
            out[safeText(fc->IDENTIFIER())] = {safeText(fc->typeSpec()), 0};
          collectLocalsFromBlock(fc->block(), beforeLine, out, flc);
        }
      }
    }
    if (auto *ws = stmt->whileStmt())
      if (cursorInsideNode(ws->block(), beforeLine))
        collectLocalsFromBlock(ws->block(), beforeLine, out, flc);
    if (auto *dw = stmt->doWhileStmt())
      if (cursorInsideNode(dw->block(), beforeLine))
        collectLocalsFromBlock(dw->block(), beforeLine, out, flc);
    if (auto *ls = stmt->loopStmt())
      if (cursorInsideNode(ls->block(), beforeLine))
        collectLocalsFromBlock(ls->block(), beforeLine, out, flc);
    if (auto *sw = stmt->switchStmt()) {
      for (auto *c : sw->caseClause())
        if (cursorInsideNode(c->block(), beforeLine))
          collectLocalsFromBlock(c->block(), beforeLine, out, flc);
      if (sw->defaultClause())
        if (cursorInsideNode(sw->defaultClause()->block(), beforeLine))
          collectLocalsFromBlock(sw->defaultClause()->block(), beforeLine, out,
                                 flc);
    }
    if (auto *tc = stmt->tryCatchStmt()) {
      if (cursorInsideNode(tc->block(), beforeLine))
        collectLocalsFromBlock(tc->block(), beforeLine, out, flc);
      for (auto *cc : tc->catchClause()) {
        if (cursorInsideNode(cc, beforeLine)) {
          if (cc->IDENTIFIER() && cc->typeSpec())
            out[safeText(cc->IDENTIFIER())] = {safeText(cc->typeSpec()), 0};
          collectLocalsFromBlock(cc->block(), beforeLine, out, flc);
        }
      }
      if (tc->finallyClause())
        if (cursorInsideNode(tc->finallyClause()->block(), beforeLine))
          collectLocalsFromBlock(tc->finallyClause()->block(), beforeLine, out,
                                 flc);
    }
  }
}

static void collectLocalsFromBlock(
    LucisParser::BlockContext *block, size_t beforeLine,
    std::unordered_map<std::string, CompletionProvider::LocalVar> &out,
    const FuncLookupCtx *flc) {
  if (!block)
    return;
  collectLocalsFromStmts(block->statement(), beforeLine, out, flc);
}

// ═══════════════════════════════════════════════════════════════════════
//  Static helpers for method completion (needed by collectors below)
// ═══════════════════════════════════════════════════════════════════════

// Resolve special type placeholders in method descriptors.
static std::string resolveTypePlaceholder(const std::string &raw,
                                          const std::string &selfType,
                                          const std::string &elemType,
                                          const std::string &keyType,
                                          const std::string &valType) {
  if (raw == "_self")
    return selfType;
  if (raw == "_elem")
    return elemType;
  if (raw == "_key")
    return keyType.empty() ? raw : keyType;
  if (raw == "_val")
    return valType.empty() ? raw : valType;
  if (raw == "_vec_key")
    return keyType.empty() ? raw : "vec<" + keyType + ">";
  if (raw == "_vec_val")
    return valType.empty() ? raw : "vec<" + valType + ">";
  return raw;
}

// Build a human-readable parameter name from its type.
static std::string paramNameFromType(const std::string &type) {
  if (type == "usize" || type == "isize")
    return "index";
  if (type == "bool")
    return "flag";
  if (type == "string")
    return "str";
  if (type == "char")
    return "ch";
  if (type == "uint32" || type == "int32")
    return "n";
  if (type == "float64" || type == "float32")
    return "x";
  if (type == "void")
    return "";
  // For generic/self/elem types, derive short name
  if (!type.empty() && type.front() == '[')
    return "arr";
  if (type.find('<') != std::string::npos)
    return "val";
  return "value";
}

// Build a CompletionItem from a MethodDescriptor with full signatures &
// snippets.
static CompletionItem buildMethodItem(const MethodDescriptor &md,
                                      const std::string &selfType,
                                      const std::string &elemType,
                                      const std::string &keyType = "",
                                      const std::string &valType = "") {
  CompletionItem ci;
  ci.label = md.name;
  ci.kind = CompletionKind::Method;

  // Resolve return type
  std::string retType = resolveTypePlaceholder(md.returnType, selfType,
                                               elemType, keyType, valType);

  // Resolve parameter types
  std::vector<std::string> resolvedParams;
  resolvedParams.reserve(md.paramTypes.size());
  for (auto &pt : md.paramTypes) {
    resolvedParams.push_back(
        resolveTypePlaceholder(pt, selfType, elemType, keyType, valType));
  }

  // detail: short signature shown inline — e.g. "(usize index, int32 value) ->
  // void"
  std::string detail = "(";
  for (size_t i = 0; i < resolvedParams.size(); i++) {
    if (i > 0)
      detail += ", ";
    detail += resolvedParams[i];
    std::string pname = paramNameFromType(resolvedParams[i]);
    if (!pname.empty())
      detail += " " + pname;
  }
  detail += ") -> " + retType;
  ci.detail = detail;

  // documentation: markdown with full method signature
  std::string doc = "```lucis\nfn " + md.name + "(";
  for (size_t i = 0; i < resolvedParams.size(); i++) {
    if (i > 0)
      doc += ", ";
    std::string pname = paramNameFromType(resolvedParams[i]);
    if (pname.empty())
      pname = "p" + std::to_string(i);
    doc += resolvedParams[i] + " " + pname;
  }
  doc += ") -> " + retType + "\n```";
  ci.documentation = doc;

  // insertText: snippet with parameter placeholders
  if (resolvedParams.empty()) {
    ci.insertText = md.name + "()";
    ci.insertTextFormat = InsertTextFormat::PlainText;
  } else {
    std::string snippet = md.name + "(";
    for (size_t i = 0; i < resolvedParams.size(); i++) {
      if (i > 0)
        snippet += ", ";
      std::string pname = paramNameFromType(resolvedParams[i]);
      if (pname.empty())
        pname = "p" + std::to_string(i);
      snippet += "${" + std::to_string(i + 1) + ":" + pname + "}";
    }
    snippet += ")";
    ci.insertText = snippet;
    ci.insertTextFormat = InsertTextFormat::Snippet;
  }

  // filterText: just the method name so typing filters correctly
  ci.filterText = md.name;

  return ci;
}

// ═══════════════════════════════════════════════════════════════════════
//  Public entry point
// ═══════════════════════════════════════════════════════════════════════

std::vector<CompletionItem>
CompletionProvider::complete(const std::string &source, size_t line, size_t col,
                             const std::string &filePath,
                             const ProjectContext *project,
                             ParseResult* preParsed) {

  // 1) Analyze context from raw text (works even with incomplete code)
  auto req = analyzeContext(source, line, col, nullptr);

  // Short-circuit for #include header completion — no parsing needed
  if (req.context == CompletionContext::IncludeHeader) {
    std::vector<CompletionItem> items;
    addHeaderSuggestions(items, req.prefix, req.closingCharPresent,
                         filePath, project);
    return items;
  }

  // Short-circuit for `use` import completion — no parsing needed
  if (req.context == CompletionContext::UseImport) {
    std::vector<CompletionItem> items;
    addUseCompletions(items, req.modulePath, req.prefix, project);
    return items;
  }

  // Short-circuit for doc-comment tag completion — no parsing needed
  if (req.context == CompletionContext::DocComment) {
    std::vector<CompletionItem> items;
    addDocTagCompletions(items, req.prefix);
    return items;
  }

  // 2) Parse (use cached result if available)
  ParseResult localParseStorage;
  ParseResult* parsed;
  if (preParsed) {
    parsed = preParsed;
  } else {
    localParseStorage = Parser::parseString(source);
    parsed = &localParseStorage;
    if (!parsed->tree) {
      // Fallback: patch the incomplete line with a comment to avoid syntax errors
      std::string patchedSource;
      std::istringstream ss(source);
      std::string ln;
      size_t lineIdx = 0;
      while (std::getline(ss, ln)) {
        if (lineIdx == line)
          patchedSource += "// <completion>\n";
        else
          patchedSource += ln + "\n";
        lineIdx++;
      }
      localParseStorage = Parser::parseString(patchedSource);
      parsed = &localParseStorage;
      if (!parsed->tree)
        return {};
    }
  }

  // Resolve C headers — always per-file, not from the global project bindings.
  const CBindings *cBindingsPtr = nullptr;

  {
    auto includeFingerprint = buildIncludeFingerprint(parsed->tree);
    if (hasIncludeBindingsCache_ &&
        includeFingerprint == includeFingerprintCache_) {
      cBindingsPtr = &includeBindingsCache_;
    } else {
      CBindings localBindings;
      includeTypeRegCache_ = TypeRegistry();
      std::vector<LucisParser::IncludeDeclContext *> includes;
      for (auto *pre : parsed->tree->preambleDecl())
        if (auto *inc = pre->includeDecl())
          includes.push_back(inc);

      if (!includes.empty()) {
        CHeaderResolver resolver(includeTypeRegCache_, localBindings);
        for (auto *incl : includes) {
          auto text = incl->getText();
          if (incl->INCLUDE_SYS()) {
            auto header = CHeaderResolver::extractSystemHeader(text);
            if (!header.empty())
              resolver.resolveSystemHeader(header);
          } else if (incl->INCLUDE_LOCAL()) {
            auto header = CHeaderResolver::extractLocalHeader(text);
            if (!header.empty())
              resolver.resolveLocalHeader(header, fs::path(filePath).parent_path().string());
          }
        }
      }

      includeBindingsCache_ = std::move(localBindings);
      includeFingerprintCache_ = std::move(includeFingerprint);
      hasIncludeBindingsCache_ = true;
      cBindingsPtr = &includeBindingsCache_;
    }
    if (!cBindingsPtr)
      cBindingsPtr = &includeBindingsCache_;
  }

  // Re-infer receiver type now that we have a valid tree
  if (req.context == CompletionContext::DotAccess ||
      req.context == CompletionContext::ArrowAccess) {
    if (req.receiverType.empty()) {
      std::string varType;
      if (!req.receiverVar.empty()) {
        varType = inferVarType(req.receiverVar, parsed->tree, line, cBindingsPtr,
                               project);
      } else if (!req.receiverCall.empty()) {
        FuncLookupCtx flc;
        flc.tree = parsed->tree;
        flc.bindings = cBindingsPtr;
        flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
        flc.extTypeReg = &extTypeRegistry_;
        flc.methodReg = &methodRegistry_;
        flc.project = project;
        varType = lookupFuncReturnType(req.receiverCall, &flc);
      }

      // Unwrap receiver type when expression has [..] indexing
      if (req.indexDepth > 0 && !varType.empty()) {
        // First, resolve type alias to its underlying type string
        // by checking type alias declarations in the tree
        std::string resolved = varType;
        for (auto *tld : parsed->tree->topLevelDecl()) {
          if (auto *ta = tld->typeAliasDecl()) {
            if (ta->IDENTIFIER() && safeText(ta->IDENTIFIER()) == resolved &&
                ta->typeSpec()) {
              resolved = safeText(ta->typeSpec());
              break;
            }
          }
        }
        varType = unwrapIndexedType(resolved, req.indexDepth);
      }

      // Apply explicit dereference depth from parenthesized receivers,
      // e.g. `(*arr).` should complete methods on the pointee.
      for (unsigned d = 0; d < req.derefDepth && !varType.empty(); d++) {
        if (varType[0] == '*')
          varType = varType.substr(1);
        else
          break;
      }

      // Resolve member chain: x.field.method().| → walk each step
      if (!req.methodChain.empty() && !varType.empty()) {
        for (auto &memberName : req.methodChain) {
          // Unwrap pointer for field lookup (e.g. *Buffer → Buffer)
          std::string lookupType = varType;
          while (!lookupType.empty() && lookupType[0] == '*')
            lookupType = lookupType.substr(1);

          std::string retType = resolveMethodReturnType(lookupType, memberName,
                                                        parsed->tree, project);
          if (retType.empty()) {
            retType = resolveFieldType(lookupType, memberName, parsed->tree,
                                       cBindingsPtr, project);
          }
          if (retType.empty())
            break; // unknown member, stop
          varType = retType;
        }
      }

      req.receiverType = varType;
    }
  }
  std::vector<CompletionItem> items;

  switch (req.context) {
  case CompletionContext::DotAccess:
  case CompletionContext::ArrowAccess: {
    // Auto-dereference pointers for method/field lookup
    std::string receiverType = req.receiverType;
    while (!receiverType.empty() && receiverType[0] == '*')
      receiverType = receiverType.substr(1);
    addStructFields(items, receiverType, parsed->tree, *cBindingsPtr,
                    project);
    addExtendMethods(items, receiverType, parsed->tree, project);
    addTypeMethods(items, receiverType, req.prefix);
    break;
  }
  case CompletionContext::ScopeAccess: {
    addIntrinsics(items, req.scopeName, req.prefix);
    addEnumVariants(items, req.scopeName, parsed->tree, *cBindingsPtr, project,
                    req.prefix);
    addStaticMethods(items, req.scopeName, parsed->tree, project, req.prefix);
    break;
  }
  case CompletionContext::TypePosition: {
    addTypeNames(items, parsed->tree, *cBindingsPtr, project, req.prefix);
    break;
  }
  case CompletionContext::General: {
    // Match body context: only show enum variants
    if (!req.matchedVar.empty()) {
      addMatchArmCompletions(items, parsed->tree, line, req.prefix, project, source);
      break;
    }
    addLocals(items, parsed->tree, line, *cBindingsPtr, req.prefix, project);
    addLocalDecls(items, parsed->tree, req.prefix);
    addProjectSymbols(items, project, filePath, req.prefix);
    addImportedSymbols(items, parsed->tree, project, req.prefix);
    addEnumWildcardVariants(items, parsed->tree, project, req.prefix, line);
    addGlobalBuiltins(items, req.prefix);
    addCSymbols(items, *cBindingsPtr, req.prefix);
    addKeywords(items, req.prefix);
    addTypeNames(items, parsed->tree, *cBindingsPtr, project, req.prefix);
    addIntrinsicRoot(items, req.prefix);
    break;
  }
  }

  dedup(items);
  return items;
}

// ═══════════════════════════════════════════════════════════════════════
//  Context analysis (text-based, pre-parse)
// ═══════════════════════════════════════════════════════════════════════

CompletionProvider::CompletionRequest
CompletionProvider::analyzeContext(const std::string &source, size_t line,
                                   size_t col,
                                   LucisParser::ProgramContext *tree) {

  CompletionRequest req;

  // Get the current line text
  std::istringstream stream(source);
  std::string lineText;
  for (size_t i = 0; i <= line; i++) {
    if (!std::getline(stream, lineText)) {
      lineText.clear();
      break;
    }
  }

  // Clamp col
  if (col > lineText.size())
    col = lineText.size();

  // Extract the text before cursor on this line
  std::string before = lineText.substr(0, col);

  // ── Check for doc-comment context (/** ... */) ──────────────────
  if (isInsideDocComment(source, line, col)) {
    req.context = CompletionContext::DocComment;
    // Extract prefix: text after the last '@' on this line
    auto atPos = before.rfind('@');
    if (atPos != std::string::npos) {
      req.prefix = before.substr(atPos + 1);
    }
    return req;
  }

  // Check for #include directive completion
  {
    static const std::regex includeRegex(R"(^\s*#include\s*([<"]))", std::regex::optimize);
    std::smatch match;
    if (std::regex_search(before, match, includeRegex)) {
      char delimiter = match[1].str()[0];
      req.context = CompletionContext::IncludeHeader;
      
      // prefix is everything after the delimiter in 'before'
      size_t delimPos = match.position(1);
      req.prefix = before.substr(delimPos + 1);
      
      char closing = (delimiter == '<') ? '>' : '"';
      req.closingCharPresent = (col < lineText.size() && lineText[col] == closing);
      return req;
    }
  }

  // Check for `use` import path completion
  {
    std::string trimmed = before;
    size_t ws = trimmed.find_first_not_of(" \t");
    if (ws != std::string::npos)
      trimmed = trimmed.substr(ws);

    // Detect: use ... or just "use" (without space yet)
    bool isUse = (trimmed == "use") ||
                 (trimmed.substr(0, 4) == "use " && trimmed.size() >= 4);
    if (isUse) {
      std::string afterUse;
      if (trimmed == "use") {
        afterUse.clear();
      } else {
        afterUse = trimmed.substr(4);
        // Strip leading whitespace
        size_t start = afterUse.find_first_not_of(" \t");
        if (start != std::string::npos)
          afterUse = afterUse.substr(start);
        else
          afterUse.clear();
      }

      // Check for group import: use std::log::{println, |
      // Find last '{' — if present, we're completing inside a group
      auto bracePos = afterUse.rfind('{');
      if (bracePos != std::string::npos) {
        // Extract module path before ::{
        std::string pathBeforeBrace = afterUse.substr(0, bracePos);
        // Remove trailing ::
        while (pathBeforeBrace.size() >= 2 && pathBeforeBrace.back() == ':' &&
               pathBeforeBrace[pathBeforeBrace.size() - 2] == ':')
          pathBeforeBrace.resize(pathBeforeBrace.size() - 2);
        // strip trailing whitespace
        while (!pathBeforeBrace.empty() && pathBeforeBrace.back() == ' ')
          pathBeforeBrace.pop_back();

        // Extract prefix: text after last comma or brace
        std::string insideBrace = afterUse.substr(bracePos + 1);
        auto lastComma = insideBrace.rfind(',');
        std::string partial;
        if (lastComma != std::string::npos)
          partial = insideBrace.substr(lastComma + 1);
        else
          partial = insideBrace;
        // trim spaces
        size_t pws = partial.find_first_not_of(" \t");
        if (pws != std::string::npos)
          partial = partial.substr(pws);
        else
          partial.clear();

        req.context = CompletionContext::UseImport;
        req.modulePath = pathBeforeBrace;
        req.prefix = partial;
        return req;
      }

      // Not inside braces — check if cursor is right after ::
      // Find the last :: to split path vs prefix
      auto lastScope = afterUse.rfind("::");
      if (lastScope != std::string::npos) {
        std::string pathPart = afterUse.substr(0, lastScope);
        std::string partial = afterUse.substr(lastScope + 2);
        // trim partial
        while (!partial.empty() && partial.back() == ' ')
          partial.pop_back();

        req.context = CompletionContext::UseImport;
        req.modulePath = pathPart;
        req.prefix = partial;
        return req;
      }

      // Just "use " or "use st" — completing the root module name
      req.context = CompletionContext::UseImport;
      req.modulePath.clear();
      req.prefix = afterUse;
      // trim trailing
      while (!req.prefix.empty() && req.prefix.back() == ' ')
        req.prefix.pop_back();
      return req;
    }
  }

  // Check for scope access: Name::| or Name<T>::|
  {
    // Helper lambda: given a position at ':' in '::', extract base name
    // handling both plain "Name::" and generic "Name<T>::"
    auto extractScopeName = [&](size_t colonStart) -> std::string {
      // colonStart points to the first ':' of '::'
      size_t cursor = colonStart;
      std::vector<std::string> segments;

      while (cursor > 0) {
        while (cursor > 0 && before[cursor - 1] == ' ')
          --cursor;
        if (cursor == 0)
          break;

        size_t segEnd = cursor;
        size_t segStart = segEnd;

        // Generic segment, e.g. Node<int32>
        if (before[segEnd - 1] == '>') {
          size_t pos = segEnd - 1;
          int depth = 1;
          while (pos > 0 && depth > 0) {
            --pos;
            if (before[pos] == '>')
              depth++;
            else if (before[pos] == '<')
              depth--;
          }
          if (depth != 0)
            break;
          segStart = pos;
          while (segStart > 0 && before[segStart - 1] == ' ')
            --segStart;
          while (segStart > 0 && (std::isalnum(static_cast<unsigned char>(
                                      before[segStart - 1])) ||
                                  before[segStart - 1] == '_')) {
            --segStart;
          }
        } else {
          while (segStart > 0 && (std::isalnum(static_cast<unsigned char>(
                                      before[segStart - 1])) ||
                                  before[segStart - 1] == '_')) {
            --segStart;
          }
        }

        if (segStart == segEnd)
          break;
        segments.push_back(before.substr(segStart, segEnd - segStart));
        cursor = segStart;

        while (cursor > 0 && before[cursor - 1] == ' ')
          --cursor;
        if (cursor < 2 || before[cursor - 1] != ':' ||
            before[cursor - 2] != ':')
          break;
        cursor -= 2;
      }

      std::reverse(segments.begin(), segments.end());
      if (segments.empty())
        return "";

      std::string joined;
      for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0)
          joined += "::";
        joined += segments[i];
      }
      return joined;
    };

    size_t pos = before.size();
    if (pos >= 2 && before[pos - 1] == ':' && before[pos - 2] == ':') {
      std::string sname = extractScopeName(pos - 2);
      if (!sname.empty()) {
        req.context = CompletionContext::ScopeAccess;
        req.scopeName = sname;
        req.prefix.clear();
        return req;
      }
    }
    // Also handle Name::partial| or Name<T>::partial|
    if (pos >= 3) {
      size_t idEnd = pos;
      while (idEnd > 0 &&
             (std::isalnum(before[idEnd - 1]) || before[idEnd - 1] == '_'))
        --idEnd;
      if (idEnd >= 2 && before[idEnd - 1] == ':' && before[idEnd - 2] == ':') {
        std::string sname = extractScopeName(idEnd - 2);
        if (!sname.empty()) {
          req.context = CompletionContext::ScopeAccess;
          req.scopeName = sname;
          req.prefix = before.substr(idEnd);
          return req;
        }
      }
    }
  }

  // Check for arrow access: expr->| or expr->partial|
  {
    size_t pos = before.size();
    // First strip any partial identifier
    size_t idEnd = pos;
    while (idEnd > 0 &&
           (std::isalnum(before[idEnd - 1]) || before[idEnd - 1] == '_'))
      --idEnd;

    if (idEnd >= 2 && before[idEnd - 1] == '>' && before[idEnd - 2] == '-') {
      req.prefix = before.substr(idEnd);
      size_t arrowPos = idEnd - 2;
      size_t recEnd = arrowPos;

      std::vector<std::string> chain;
      unsigned indexDepth = 0;

      while (recEnd > 0) {
        if (before[recEnd - 1] == ')') {
          size_t parenEnd = recEnd - 1;
          int depth = 1;
          size_t p = parenEnd;
          while (p > 0 && depth > 0) {
            --p;
            if (before[p] == ')')
              ++depth;
            else if (before[p] == '(')
              --depth;
          }
          if (depth != 0)
            break;

          size_t nameEnd = p;
          size_t nameStart = nameEnd;
          while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(
                                       before[nameStart - 1])) ||
                                   before[nameStart - 1] == '_')) {
            --nameStart;
          }
          if (nameStart == nameEnd)
            break;

          std::string methodName =
              before.substr(nameStart, nameEnd - nameStart);
          recEnd = nameStart;
          if (recEnd > 0 && before[recEnd - 1] == '.') {
            chain.push_back(methodName);
            --recEnd;
          } else {
            req.receiverCall = methodName;
            break;
          }
        } else if (before[recEnd - 1] == ']') {
          size_t bracketEnd = recEnd - 1;
          int depth = 1;
          size_t p = bracketEnd;
          while (p > 0 && depth > 0) {
            --p;
            if (before[p] == ']')
              ++depth;
            else if (before[p] == '[')
              --depth;
          }
          if (depth != 0)
            break;
          recEnd = p;
          ++indexDepth;
        } else if (std::isalnum(
                       static_cast<unsigned char>(before[recEnd - 1])) ||
                   before[recEnd - 1] == '_') {
          size_t nameEnd = recEnd;
          size_t nameStart = nameEnd;
          while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(
                                       before[nameStart - 1])) ||
                                   before[nameStart - 1] == '_')) {
            --nameStart;
          }

          if (nameStart == nameEnd)
            break;

          if (nameStart > 0 && before[nameStart - 1] == '.') {
            chain.push_back(before.substr(nameStart, nameEnd - nameStart));
            recEnd = nameStart - 1;
          } else {
            break;
          }
        } else {
          break;
        }
      }

      size_t nameEnd = recEnd;
      while (recEnd > 0 &&
             (std::isalnum(static_cast<unsigned char>(before[recEnd - 1])) ||
              before[recEnd - 1] == '_')) {
        --recEnd;
      }
      req.receiverVar = before.substr(recEnd, nameEnd - recEnd);
      req.indexDepth = indexDepth;
      std::reverse(chain.begin(), chain.end());
      req.methodChain = std::move(chain);
      req.context = CompletionContext::ArrowAccess;
      return req;
    }
  }

  // Check for dot access: expr.| or expr.partial| or expr[...].| or
  // expr.method().| chains
  {
    size_t pos = before.size();
    size_t idEnd = pos;
    while (idEnd > 0 &&
           (std::isalnum(before[idEnd - 1]) || before[idEnd - 1] == '_'))
      --idEnd;

    if (idEnd > 0 && before[idEnd - 1] == '.') {
      req.prefix = before.substr(idEnd);
      size_t dotPos = idEnd - 1;
      size_t recEnd = dotPos;

      // Walk backwards through the expression, collecting member chain
      // Patterns to handle:
      //   .name(...)   → method call, record name in chain
      //   .field       → field access, record name in chain
      //   [...]        → array index, increment indexDepth
      //   identifier   → base variable, stop
      std::vector<std::string> chain;
      unsigned indexDepth = 0;

      while (recEnd > 0) {
        // Try: ...method_name(...)
        if (before[recEnd - 1] == ')') {
          // Walk back over balanced (...)
          size_t parenEnd = recEnd - 1;
          int depth = 1;
          size_t p = parenEnd;
          while (p > 0 && depth > 0) {
            --p;
            if (before[p] == ')')
              ++depth;
            else if (before[p] == '(')
              --depth;
          }
          if (depth != 0)
            break; // unbalanced
          // p is now at '(', walk back to get method name
          size_t nameEnd = p;
          size_t nameStart = nameEnd;
          while (nameStart > 0 && (std::isalnum(before[nameStart - 1]) ||
                                   before[nameStart - 1] == '_'))
            --nameStart;
          if (nameStart == nameEnd)
            break; // no method name before '('
          std::string methodName =
              before.substr(nameStart, nameEnd - nameStart);
          recEnd = nameStart;
          // Check for '.' before the method name
          if (recEnd > 0 && before[recEnd - 1] == '.') {
            chain.push_back(methodName);
            --recEnd; // skip the dot and continue chain walk
          } else {
            // Base receiver is a function call, e.g. foo().bar().|
            req.receiverCall = methodName;
            break;
          }
        }
        // Try: ...[...]
        else if (before[recEnd - 1] == ']') {
          size_t bracketEnd = recEnd - 1;
          int depth = 1;
          size_t p = bracketEnd;
          while (p > 0 && depth > 0) {
            --p;
            if (before[p] == ']')
              ++depth;
            else if (before[p] == '[')
              --depth;
          }
          if (depth != 0)
            break;
          recEnd = p;
          ++indexDepth;
        }
        // Try: ...field_name
        else if (std::isalnum(static_cast<unsigned char>(before[recEnd - 1])) ||
                 before[recEnd - 1] == '_') {
          size_t nameEnd = recEnd;
          size_t nameStart = nameEnd;
          while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(
                                       before[nameStart - 1])) ||
                                   before[nameStart - 1] == '_')) {
            --nameStart;
          }

          if (nameStart == nameEnd)
            break;

          // If preceded by a dot, this identifier is an intermediate
          // field access in the chain; otherwise it is the base variable.
          // Guard against `..` (range operator) which is not field access.
          if (nameStart > 0 && before[nameStart - 1] == '.' &&
              !(nameStart >= 2 && before[nameStart - 2] == '.')) {
            chain.push_back(before.substr(nameStart, nameEnd - nameStart));
            recEnd = nameStart - 1; // continue before the dot
          } else if (nameStart >= 2 && before[nameStart - 1] == '>' &&
                     before[nameStart - 2] == '-') {
            // `ptr->field` — same as `.field` for member completion
            chain.push_back(before.substr(nameStart, nameEnd - nameStart));
            recEnd = nameStart - 2; // continue to the left of `->`
          } else {
            break;
          }
        }
        // Otherwise: base variable identifier
        else {
          break;
        }
      }

      // Extract the base variable name
      size_t nameEnd = recEnd;
      while (recEnd > 0 &&
             (std::isalnum(before[recEnd - 1]) || before[recEnd - 1] == '_'))
        --recEnd;
      req.receiverVar = before.substr(recEnd, nameEnd - recEnd);
      if (req.receiverVar.empty() && req.receiverCall.empty() &&
          !chain.empty()) {
        // Fallback for malformed/incomplete text states.
        req.receiverCall = chain.front();
      }
      req.indexDepth = indexDepth;
      // Fallback for parenthesized deref receivers like `(*arr).`.
      if (req.receiverVar.empty() && req.receiverCall.empty()) {
        auto trim = [](std::string s) {
          size_t b = s.find_first_not_of(" \t\n\r");
          if (b == std::string::npos)
            return std::string{};
          size_t e = s.find_last_not_of(" \t\n\r");
          return s.substr(b, e - b + 1);
        };
        std::string recvExpr = trim(before.substr(0, dotPos));
        if (!recvExpr.empty() && recvExpr.back() == ')') {
          int depth = 1;
          size_t p = recvExpr.size() - 1;
          while (p > 0 && depth > 0) {
            --p;
            if (recvExpr[p] == ')')
              ++depth;
            else if (recvExpr[p] == '(')
              --depth;
          }
          if (depth == 0 && p + 1 < recvExpr.size() - 1) {
            std::string inner =
                trim(recvExpr.substr(p + 1, recvExpr.size() - p - 2));
            bool hadDeref = false;
            while (!inner.empty() && inner[0] == '*') {
              hadDeref = true;
              ++req.derefDepth;
              inner = trim(inner.substr(1));
            }
            if (!inner.empty()) {
              bool isIdent = true;
              for (char c : inner) {
                if (!(std::isalnum(static_cast<unsigned char>(c)) ||
                      c == '_')) {
                  isIdent = false;
                  break;
                }
              }
              if (isIdent)
                req.receiverVar = inner;
              else if (hadDeref)
                req.derefDepth = 0;
            }
          }
        }
      }
      // Reverse chain since we collected from right to left
      std::reverse(chain.begin(), chain.end());
      req.methodChain = std::move(chain);
      req.context = CompletionContext::DotAccess;
      return req;
    }
  }

  // Check for type position — heuristics:
  // After keywords that expect a type: let, var, fn params, return type
  // Simple heuristic: if line matches common type-position patterns
  {
    // Trim leading whitespace
    std::string trimmed = before;
    size_t ws = trimmed.find_first_not_of(" \t");
    if (ws != std::string::npos)
      trimmed = trimmed.substr(ws);

    // After ":" in struct literal field or after cast "as "
    if (trimmed.size() >= 3 && trimmed.substr(trimmed.size() - 3) == "as ") {
      req.context = CompletionContext::TypePosition;
      req.prefix.clear();
      return req;
    }
  }

  // Detect match body context: scan source backwards from cursor for "match <var> {"
  {
    std::istringstream ss(source);
    std::vector<std::string> lines;
    std::string ln;
    while (std::getline(ss, ln)) lines.push_back(ln);
    if (line < lines.size()) {
      // Check if cursor is in an expression arm (after "->" without "{")
      std::string currentLine = lines[line];
      std::string beforeCursor = currentLine.substr(0, col);
      auto arrowPos = beforeCursor.rfind("->");
      if (arrowPos != std::string::npos) {
        std::string afterArrow = beforeCursor.substr(arrowPos + 2);
        // If no "{" after "->", we're in an expression arm - don't set matchedVar
        if (afterArrow.find('{') == std::string::npos) {
          goto context_done;
        }
      }

      int depth = 0;
      for (int i = static_cast<int>(line); i >= 0; --i) {
        for (size_t j = 0; j < lines[i].size(); ++j) {
          if (lines[i][j] == '}') ++depth;
          else if (lines[i][j] == '{') {
            if (depth > 0) { --depth; continue; }
            // Check if this brace belongs to a match arm body (preceded by "->")
            std::string before = lines[i].substr(0, j);
            if (before.find("->") != std::string::npos) {
              // This is an arm body block, skip it
              ++depth;
              continue;
            }
            // Check previous line too (multi-line arm: "Ok(v) ->\n{")
            if (i > 0) {
              std::string prevLine = lines[i-1];
              size_t ws2 = prevLine.find_last_not_of(" \t\r\n");
              if (ws2 != std::string::npos && prevLine[ws2] == '>'
                  && ws2 > 0 && prevLine[ws2-1] == '-') {
                ++depth;
                continue;
              }
            }
            // Found opening brace at depth 0 — check if preceded by "match"
            if (i > 0) before = lines[i-1] + " " + before;
            auto mp = before.rfind("match");
            if (mp != std::string::npos) {
              // Extract identifier after "match"
              std::string after = before.substr(mp + 5); // len("match")
              size_t s2 = after.find_first_not_of(" \t\r\n");
              if (s2 != std::string::npos) {
                after = after.substr(s2);
                // Extract simple identifier
                std::string varName;
                for (char c : after) {
                  if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                    varName += c;
                  else break;
                }
                if (!varName.empty()) {
                  req.matchedVar = varName;
                  goto context_done;
                }
              }
            }
            goto context_done;
          }
        }
      }
    }
  }
context_done:

  // General completion: extract prefix
  {
    size_t pos = col;
    while (pos > 0 && (std::isalnum(before[pos - 1]) || before[pos - 1] == '_'))
      --pos;
    req.prefix = before.substr(pos);
    req.context = CompletionContext::General;
  }

  return req;
}

// ═══════════════════════════════════════════════════════════════════════
//  Collectors
// ═══════════════════════════════════════════════════════════════════════

void CompletionProvider::addLocals(std::vector<CompletionItem> &items,
                                   LucisParser::ProgramContext *tree,
                                   size_t cursorLine, const CBindings &bindings,
                                   const std::string &prefix,
                                   const ProjectContext *project) {
  // Check if inside a function
  auto *func = findEnclosingFunction(tree, cursorLine);
  if (func) {
    auto locals = collectLocals(func, cursorLine, tree, &bindings, project);
    for (auto &[name, var] : locals) {
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Variable;
      item.detail = var.typeName;
      items.push_back(std::move(item));
    }

    // Also add function parameters
    if (auto *params = func->paramList()) {
      for (auto *p : params->param()) {
        if (!p->IDENTIFIER())
          continue;
        std::string name = safeText(p->IDENTIFIER());
        if (!matchesPrefix(name, prefix))
          continue;
        CompletionItem item;
        item.label = name;
        item.kind = CompletionKind::Variable;
        item.detail = typeSpecToString(p->typeSpec());
        items.push_back(std::move(item));
      }
    }
    return;
  }

  // Check if inside an extend method
  auto *method = findEnclosingMethod(tree, cursorLine);
  if (method) {
    auto locals = collectLocalsFromMethod(method, cursorLine);
    for (auto &[name, var] : locals) {
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Variable;
      item.detail = var.typeName;
      items.push_back(std::move(item));
    }
  }
}

void CompletionProvider::addLocalDecls(std::vector<CompletionItem> &items,
                                       LucisParser::ProgramContext *tree,
                                       const std::string &prefix) {
  for (auto *tld : tree->topLevelDecl()) {
    // Functions
    if (auto *func = tld->functionDecl()) {
      if (func->IDENTIFIER().empty())
        continue;
      std::string name = safeIdAt(func, 0);
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Function;
      item.detail = formatFuncSignature(func);

      // Build snippet with parameter placeholders
      if (auto *params = func->paramList();
          params && !params->param().empty()) {
        std::string snippet = name + "(";
        auto paramList = params->param();
        for (size_t i = 0; i < paramList.size(); i++) {
          if (i > 0)
            snippet += ", ";
          snippet += "${" + std::to_string(i + 1) + ":" +
                     safeText(paramList[i]->IDENTIFIER()) + "}";
        }
        snippet += ")";
        item.insertText = snippet;
        item.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        item.insertText = name + "()";
      }
      // Documentation with full signature
      item.documentation = "```lucis\n" + item.detail + "\n```";
      items.push_back(std::move(item));
    }

    // Structs
    if (auto *sd = tld->structDecl()) {
      std::string name = safeText(sd->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Struct;
      if (sd->typeParamList()) {
        // Generic struct: show type params in detail
        std::string params;
        bool first = true;
        for (auto *tp : sd->typeParamList()->typeParam()) {
          auto ids = tp->IDENTIFIER();
          if (!first)
            params += ", ";
          if (!ids.empty())
            params += ids[0]->getText();
          if (tp->COLON() && ids.size() >= 2)
            params += ": " + ids[1]->getText();
          first = false;
        }
        item.detail = "struct " + name + "<" + params + ">";
        item.insertText = name + "<${1}>";
        item.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        item.detail = "struct " + name;
      }
      items.push_back(std::move(item));
    }

    // Enums
    if (auto *ed = tld->enumDecl()) {
      if (!ed->IDENTIFIER())
        continue;
      std::string name = safeText(ed->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Enum;
      item.detail = "enum " + name;
      items.push_back(std::move(item));
    }

    // Unions
    if (auto *ud = tld->unionDecl()) {
      if (!ud->IDENTIFIER())
        continue;
      std::string name = safeText(ud->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Struct;
      if (ud->typeParamList()) {
        std::string params;
        bool first = true;
        for (auto *tp : ud->typeParamList()->typeParam()) {
          auto ids = tp->IDENTIFIER();
          if (!first)
            params += ", ";
          if (!ids.empty())
            params += ids[0]->getText();
          if (tp->COLON() && ids.size() >= 2)
            params += ": " + ids[1]->getText();
          first = false;
        }
        item.detail = "union " + name + "<" + params + ">";
        item.insertText = name + "<${1}>";
        item.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        item.detail = "union " + name;
      }
      items.push_back(std::move(item));
    }

    // Type aliases
    if (auto *ta = tld->typeAliasDecl()) {
      if (!ta->IDENTIFIER())
        continue;
      std::string name = safeText(ta->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Class;
      item.detail = "type " + name + " = " + typeSpecToString(ta->typeSpec());
      items.push_back(std::move(item));
    }

    // Extern functions
    if (auto *ext = tld->externDecl()) {
      if (!ext->IDENTIFIER())
        continue;
      std::string name = safeText(ext->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Function;
      std::string detail = typeSpecToString(ext->typeSpec()) + " " + name + "(";
      if (auto *params = ext->externParamList()) {
        bool first = true;
        for (auto *p : params->externParam()) {
          if (!first)
            detail += ", ";
          first = false;
          detail += typeSpecToString(p->typeSpec());
          if (p->IDENTIFIER())
            detail += " " + safeText(p->IDENTIFIER());
        }
      }
      if (ext->SPREAD()) {
        if (ext->externParamList())
          detail += ", ";
        detail += "...";
      }
      detail += ")";
      item.detail = detail;

      // Build snippet for extern function
      if (auto *params = ext->externParamList();
          params && !params->externParam().empty()) {
        std::string snippet = name + "(";
        auto paramList = params->externParam();
        for (size_t i = 0; i < paramList.size(); i++) {
          if (i > 0)
            snippet += ", ";
          std::string pname = paramList[i]->IDENTIFIER()
                                  ? safeText(paramList[i]->IDENTIFIER())
                                  : "p" + std::to_string(i);
          snippet += "${" + std::to_string(i + 1) + ":" + pname + "}";
        }
        snippet += ")";
        item.insertText = snippet;
        item.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        item.insertText = name + "()";
      }
      item.documentation = "```lucis\n" + detail + "\n```";
      items.push_back(std::move(item));
    }
  }
}

void CompletionProvider::addImportedSymbols(std::vector<CompletionItem> &items,
                                            LucisParser::ProgramContext *tree,
                                            const ProjectContext *project,
                                            const std::string &prefix) {
  if (!project || !project->isValid())
    return;

  for (auto *pre : tree->preambleDecl()) {
    auto *useDecl = pre->useDecl();
    if (!useDecl)
      continue;
    std::string modulePath;
    std::vector<std::string> symbolNames;

    if (auto *root = dynamic_cast<LucisParser::UseRootContext *>(useDecl)) {
      if (!root->IDENTIFIER())
        continue;
      symbolNames.push_back(safeText(root->IDENTIFIER()));
    } else if (auto *item =
                   dynamic_cast<LucisParser::UseItemContext *>(useDecl)) {
      if (!item->modulePath() || !item->IDENTIFIER())
        continue;
      for (auto *id : item->modulePath()->IDENTIFIER()) {
        if (!modulePath.empty())
          modulePath += "::";
        modulePath += id->getText();
      }
      symbolNames.push_back(safeText(item->IDENTIFIER()));
    } else if (auto *group =
                   dynamic_cast<LucisParser::UseGroupContext *>(useDecl)) {
      if (!group->modulePath())
        continue;
      for (auto *id : group->modulePath()->IDENTIFIER()) {
        if (!modulePath.empty())
          modulePath += "::";
        modulePath += id->getText();
      }
      for (auto *id : group->IDENTIFIER()) {
        symbolNames.push_back(id->getText());
      }
    }

    for (auto &symName : symbolNames) {
      if (!matchesPrefix(symName, prefix))
        continue;

      std::string importedPath = modulePath;
      if (!importedPath.empty())
        importedPath += "::";
      importedPath += symName;

      bool isModule = modulePath.empty();
      if (!isModule && project && project->isValid()) {
        isModule = project->registry().hasModule(
            ModuleRegistry::usePathToModulePath(importedPath));
      }
      // For `use std::X;` imports, the imported symbol may be a std module.
      if (!isModule && modulePath == "std") {
        if (ImportResolver::isStdModule(importedPath))
          isModule = true;
      }

      if (isModule) {
        CompletionItem ci;
        ci.label = symName;
        ci.kind = CompletionKind::Module;
        ci.detail = importedPath;
        ci.insertText = symName + "::";
        items.push_back(std::move(ci));
        continue;
      }

      auto registryPath = ModuleRegistry::usePathToModulePath(modulePath);
      auto *sym = project->registry().findSymbol(registryPath, symName);

      // If not in project registry, try on-demand parse of stdlib module
      if (!sym) {
        static const std::vector<std::string> stdlibDirs3 = ImportResolver::stdlibSearchPaths();
        for (auto& dir : stdlibDirs3) {
          auto candidate = fs::path(dir) / (registryPath + ".lc");
          std::error_code ec;
          if (!fs::exists(candidate, ec) || ec) continue;
          auto parseResult = project ? project->getStdlibParse(candidate.string()) : nullptr;
          if (!parseResult) {
            try {
              parseResult = std::make_shared<ParseResult>(Parser::parse(candidate.string()));
            } catch (...) { continue; }
          }
          if (!parseResult->tree) continue;
          // Extract symbols directly from the parse tree
          for (auto* tld : parseResult->tree->topLevelDecl()) {
            if (auto* fd = tld->functionDecl()) {
              if (!fd->IDENTIFIER().empty() && safeText(fd->IDENTIFIER(0)) == symName) {
                CompletionItem ci; ci.label = symName;
                ci.kind = CompletionKind::Function; ci.detail = "function";
                items.push_back(std::move(ci)); return;
              }
            } else if (auto* sd = tld->structDecl()) {
              if (safeText(sd->IDENTIFIER()) == symName) {
                CompletionItem ci; ci.label = symName;
                ci.kind = CompletionKind::Struct; ci.detail = "struct";
                items.push_back(std::move(ci)); return;
              }
            } else if (auto* ed = tld->enumDecl()) {
              if (safeText(ed->IDENTIFIER()) == symName) {
                CompletionItem ci; ci.label = symName;
                ci.kind = CompletionKind::Enum; ci.detail = "enum";
                items.push_back(std::move(ci)); return;
              }
            } else if (auto* ud = tld->unionDecl()) {
              if (safeText(ud->IDENTIFIER()) == symName) {
                CompletionItem ci; ci.label = symName;
                ci.kind = CompletionKind::Struct; ci.detail = "union";
                items.push_back(std::move(ci)); return;
              }
            } else if (auto* ta = tld->typeAliasDecl()) {
              if (safeText(ta->IDENTIFIER()) == symName) {
                CompletionItem ci; ci.label = symName;
                ci.kind = CompletionKind::Class; ci.detail = "type alias";
                items.push_back(std::move(ci)); return;
              }
            }
          }
          break;
        }
      }

      if (!sym) {
        // Fallback: check stdlib builtins for std:: modules
        if (modulePath.rfind("std::", 0) == 0) {
          // Check if it's a builtin function
          const auto *sig = builtinRegistry_.lookup(symName);
          if (sig) {
            CompletionItem ci;
            ci.label = symName;
            ci.kind = CompletionKind::Function;

            // Build detail: (paramTypes) -> returnType
            std::string detail = "(";
            for (size_t i = 0; i < sig->paramTypes.size(); i++) {
              if (i > 0)
                detail += ", ";
              detail += sig->paramTypes[i];
            }
            if (sig->isVariadic) {
              if (!sig->paramTypes.empty())
                detail += ", ";
              detail += "...";
            }
            detail += ") -> " + sig->returnType;
            ci.detail = detail;

            ci.documentation = "```lucis\nfn " + symName + detail +
                               "\n```\n\nImported from `" + modulePath + "`";

            // Build snippet with parameter placeholders
            if (!sig->paramTypes.empty()) {
              std::string snippet = symName + "(";
              for (size_t i = 0; i < sig->paramTypes.size(); i++) {
                if (i > 0)
                  snippet += ", ";
                snippet += "${" + std::to_string(i + 1) + ":" +
                           paramNameFromType(sig->paramTypes[i]) + "}";
              }
              if (sig->isVariadic) {
                snippet += ", ${" + std::to_string(sig->paramTypes.size() + 1) +
                           ":...}";
              }
              snippet += ")";
              ci.insertText = snippet;
              ci.insertTextFormat = InsertTextFormat::Snippet;
            } else if (sig->isVariadic) {
              ci.insertText = symName + "(${1:...})";
              ci.insertTextFormat = InsertTextFormat::Snippet;
            } else {
              ci.insertText = symName + "()";
            }

            items.push_back(std::move(ci));
            continue;
          }

          // Check if it's a builtin constant (e.g. PI, E)
          const auto &constType = builtinRegistry_.lookupConstant(symName);
          if (!constType.empty()) {
            CompletionItem ci;
            ci.label = symName;
            ci.kind = CompletionKind::Constant;
            ci.detail = constType;
            ci.documentation = "```lucis\nconst " + symName + ": " + constType +
                               "\n```\n\nImported from `" + modulePath + "`";
            items.push_back(std::move(ci));
            continue;
          }
        }
        continue;
      }

      CompletionItem ci;
      ci.label = symName;

      switch (sym->kind) {
      case ExportedSymbol::Function: {
        ci.kind = CompletionKind::Function;
        auto *fd = dynamic_cast<LucisParser::FunctionDeclContext *>(sym->decl);
        if (fd) {
          ci.detail = formatFuncSignature(fd);
          ci.documentation = "```lucis\n" + ci.detail + "\n```";
          // Build snippet with parameter placeholders
          if (auto *params = fd->paramList();
              params && !params->param().empty()) {
            std::string snippet = symName + "(";
            auto pList = params->param();
            for (size_t i = 0; i < pList.size(); i++) {
              if (i > 0)
                snippet += ", ";
              snippet += "${" + std::to_string(i + 1) + ":" +
                         safeText(pList[i]->IDENTIFIER()) + "}";
            }
            snippet += ")";
            ci.insertText = snippet;
            ci.insertTextFormat = InsertTextFormat::Snippet;
          } else {
            ci.insertText = symName + "()";
          }
        }
        break;
      }
      case ExportedSymbol::Struct:
        ci.kind = CompletionKind::Struct;
        ci.detail = "struct " + symName;
        break;
      case ExportedSymbol::Enum:
        ci.kind = CompletionKind::Enum;
        ci.detail = "enum " + symName;
        break;
      case ExportedSymbol::Union:
        ci.kind = CompletionKind::Struct;
        ci.detail = "union " + symName;
        break;
      case ExportedSymbol::TypeAlias:
        ci.kind = CompletionKind::Class;
        ci.detail = "type " + symName;
        break;
      case ExportedSymbol::ExtendBlock:
        continue; // extend blocks not directly completable
      }

      items.push_back(std::move(ci));
    }
  }
}

static std::string extractBaseTypeName(LucisParser::TypeSpecContext* typeSpec) {
    auto text = typeSpec->getText();
    auto pos = text.find('<');
    if (pos != std::string::npos)
        text.resize(pos);
    while (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

void CompletionProvider::addEnumWildcardVariants(std::vector<CompletionItem>& items,
                                                  LucisParser::ProgramContext* tree,
                                                  const ProjectContext* project,
                                                  const std::string& prefix,
                                                  size_t cursorLine) {
    auto process = [&](LucisParser::UseDeclContext* useDecl) {
        auto* ew = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl);
        if (!ew) return;
        auto baseName = extractBaseTypeName(ew->typeSpec());
        if (baseName.empty()) return;

        auto addVariants = [&](LucisParser::EnumDeclContext* ed) {
            for (auto* variant : ed->enumVariant()) {
                auto* v = variant->IDENTIFIER();
                if (!v) continue;
                if (!matchesPrefix(v->getText(), prefix)) continue;
                CompletionItem ci;
                ci.label = v->getText();
                ci.kind = CompletionKind::EnumMember;
                ci.detail = baseName + "::" + v->getText();
                items.push_back(std::move(ci));
            }
        };

        std::string resolvedEnumName;
        auto* ed = TypeInferrer::findEnum(baseName, tree, project,
                                           &resolvedEnumName);
        if (!ed) {
            // Fallback: try registry with raw baseName
            if (project && project->isValid()) {
                for (auto& ns : project->registry().allModules()) {
                    auto* sym = project->registry().findSymbol(ns, baseName);
                    if (!sym || sym->kind != ExportedSymbol::Enum) continue;
                    auto* decl = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
                    if (!decl) continue;
                    addVariants(decl);
                    return;
                }
            }
            return;
        }
        addVariants(ed);
    };

    // Preamble-level use EnumType::*;
    for (auto* pre : tree->preambleDecl()) {
        auto* useDecl = pre->useDecl();
        if (!useDecl) continue;
        process(useDecl);
    }
    // Top-level use EnumType::*;
    for (auto* tld : tree->topLevelDecl()) {
        auto* useDecl = tld->useDecl();
        if (!useDecl) continue;
        process(useDecl);
    }
    // In-function use EnumType::*; statements
    auto* func = findEnclosingFunction(tree, cursorLine);
    if (!func || !func->block()) return;

    // Recursive lambda to walk statements for useEnumWildcard
    std::function<void(const std::vector<LucisParser::StatementContext*>&, size_t)>
        scanStmts;
    std::function<void(LucisParser::StatementContext*, size_t)> scanStmt;

    auto cursorInside = [](antlr4::ParserRuleContext* node, size_t beforeLine) {
        if (!node) return false;
        auto* s = node->getStart();
        auto* e = node->getStop();
        if (!s || !e) return false;
        return s->getLine() - 1 <= beforeLine && e->getLine() - 1 >= beforeLine;
    };

    scanStmt = [&](LucisParser::StatementContext* stmt, size_t bline) {
        auto* start = stmt->getStart();
        if (start && start->getLine() - 1 > bline) return;

        if (auto* useDecl = stmt->useDecl()) {
            process(useDecl);
        }

        if (auto* ns = stmt->nakedBlockStmt()) {
            if (cursorInside(ns, bline))
                scanStmts(ns->statement(), bline);
        }
        if (auto* is = stmt->inlineBlockStmt()) {
            if (cursorInside(is, bline))
                scanStmts(is->statement(), bline);
        }
        if (auto* sb = stmt->scopeBlockStmt()) {
            if (cursorInside(sb, bline))
                scanStmts(sb->statement(), bline);
        }
        if (auto* ifs = stmt->ifStmt()) {
            if (ifs->ifBody()) {
                if (auto* b = ifs->ifBody()->block()) {
                    if (cursorInside(b, bline))
                        scanStmts(b->statement(), bline);
                } else if (auto* s = ifs->ifBody()->statement()) {
                    if (cursorInside(s, bline))
                        scanStmt(s, bline);
                }
            }
            for (auto* elif : ifs->elseIfClause()) {
                if (elif->ifBody()) {
                    if (auto* b = elif->ifBody()->block()) {
                        if (cursorInside(b, bline))
                            scanStmts(b->statement(), bline);
                    } else if (auto* s = elif->ifBody()->statement()) {
                        if (cursorInside(s, bline))
                            scanStmt(s, bline);
                    }
                }
            }
            if (ifs->elseClause()) {
                if (auto* eb = ifs->elseClause()->ifBody()) {
                    if (auto* b = eb->block()) {
                        if (cursorInside(b, bline))
                            scanStmts(b->statement(), bline);
                    } else if (auto* s = eb->statement()) {
                        if (cursorInside(s, bline))
                            scanStmt(s, bline);
                    }
                }
            }
        }
        if (auto* fs = stmt->forStmt()) {
            if (auto* fin = dynamic_cast<LucisParser::ForInStmtContext*>(fs)) {
                if (cursorInside(fin->block(), bline))
                    scanStmts(fin->block()->statement(), bline);
            }
            if (auto* fc = dynamic_cast<LucisParser::ForClassicStmtContext*>(fs)) {
                if (cursorInside(fc->block(), bline))
                    scanStmts(fc->block()->statement(), bline);
            }
        }
        if (auto* ws = stmt->whileStmt()) {
            if (cursorInside(ws->block(), bline))
                scanStmts(ws->block()->statement(), bline);
        }
        if (auto* dw = stmt->doWhileStmt()) {
            if (cursorInside(dw->block(), bline))
                scanStmts(dw->block()->statement(), bline);
        }
        if (auto* ls = stmt->loopStmt()) {
            if (cursorInside(ls->block(), bline))
                scanStmts(ls->block()->statement(), bline);
        }
        if (auto* sw = stmt->switchStmt()) {
            for (auto* cc : sw->caseClause()) {
                if (cursorInside(cc->block(), bline))
                    scanStmts(cc->block()->statement(), bline);
            }
            if (sw->defaultClause() && cursorInside(sw->defaultClause()->block(), bline))
                scanStmts(sw->defaultClause()->block()->statement(), bline);
        }
        if (auto* tc = stmt->tryCatchStmt()) {
            if (cursorInside(tc->block(), bline))
                scanStmts(tc->block()->statement(), bline);
            for (auto* cc : tc->catchClause()) {
                if (cursorInside(cc->block(), bline))
                    scanStmts(cc->block()->statement(), bline);
            }
            if (tc->finallyClause() && cursorInside(tc->finallyClause()->block(), bline))
                scanStmts(tc->finallyClause()->block()->statement(), bline);
        }
    };

    scanStmts = [&](const std::vector<LucisParser::StatementContext*>& stmts, size_t bline) {
        for (auto* stmt : stmts)
            scanStmt(stmt, bline);
    };

    scanStmts(func->block()->statement(), cursorLine);
}

void CompletionProvider::addProjectSymbols(std::vector<CompletionItem> &items,
                                           const ProjectContext *project,
                                           const std::string &filePath,
                                           const std::string &prefix) {
  if (!project || !project->isValid())
    return;

  std::string currentModPath = project->modulePathFor(filePath);

  // Same-module symbols declared in OTHER files.
  // This is what makes symbols from another file in the same module
  // appear in general completion without requiring `use`.
  if (!currentModPath.empty()) {
    auto sameNsExternal =
        project->registry().getExternalSymbols(currentModPath, filePath);
    for (auto *sym : sameNsExternal) {
      if (sym->kind == ExportedSymbol::ExtendBlock)
        continue;
      if (!matchesPrefix(sym->name, prefix))
        continue;

      CompletionItem ci;
      ci.label = sym->name;

      switch (sym->kind) {
      case ExportedSymbol::Function: {
        ci.kind = CompletionKind::Function;
        auto *fd = dynamic_cast<LucisParser::FunctionDeclContext *>(sym->decl);
        if (fd) {
          ci.detail = formatFuncSignature(fd) + " [" + currentModPath + "]";
          ci.documentation = "```lucis\n" + formatFuncSignature(fd) + "\n```";

          if (auto *params = fd->paramList();
              params && !params->param().empty()) {
            std::string snippet = sym->name + "(";
            auto pList = params->param();
            for (size_t i = 0; i < pList.size(); i++) {
              if (i > 0)
                snippet += ", ";
              snippet += "${" + std::to_string(i + 1) + ":" +
                         safeText(pList[i]->IDENTIFIER()) + "}";
            }
            snippet += ")";
            ci.insertText = snippet;
            ci.insertTextFormat = InsertTextFormat::Snippet;
          } else {
            ci.insertText = sym->name + "()";
          }
        } else {
          ci.detail = "function [" + currentModPath + "]";
        }
        break;
      }
      case ExportedSymbol::Struct:
        ci.kind = CompletionKind::Struct;
        ci.detail = "struct " + sym->name + " [" + currentModPath + "]";
        break;
      case ExportedSymbol::Enum:
        ci.kind = CompletionKind::Enum;
        ci.detail = "enum " + sym->name + " [" + currentModPath + "]";
        break;
      case ExportedSymbol::Union:
        ci.kind = CompletionKind::Struct;
        ci.detail = "union " + sym->name + " [" + currentModPath + "]";
        break;
      case ExportedSymbol::TypeAlias:
        ci.kind = CompletionKind::Class;
        ci.detail = "type " + sym->name + " [" + currentModPath + "]";
        break;
      default:
        continue;
      }

      items.push_back(std::move(ci));
    }
  }
}

void CompletionProvider::addCSymbols(std::vector<CompletionItem> &items,
                                     const CBindings &bindings,
                                     const std::string &prefix) {
  // C functions
  for (auto &[name, cf] : bindings.functions()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Function;
    std::string detail = "(C) ";
    if (cf.returnType)
      detail += cf.returnType->name;
    detail += " " + name + "(";
    for (size_t i = 0; i < cf.paramTypes.size(); i++) {
      if (i > 0)
        detail += ", ";
      if (cf.paramTypes[i])
        detail += cf.paramTypes[i]->name;
    }
    if (cf.isVariadic) {
      if (!cf.paramTypes.empty())
        detail += ", ";
      detail += "...";
    }
    detail += ")";
    ci.detail = detail;
    ci.documentation = "```c\n" + detail.substr(4) + "\n```"; // strip "(C) "
    // Snippet for C functions
    if (!cf.paramTypes.empty()) {
      std::string snippet = name + "(";
      for (size_t i = 0; i < cf.paramTypes.size(); i++) {
        if (i > 0)
          snippet += ", ";
        std::string pname = cf.paramTypes[i]
                                ? paramNameFromType(cf.paramTypes[i]->name)
                                : "p" + std::to_string(i);
        if (pname.empty())
          pname = "p" + std::to_string(i);
        snippet += "${" + std::to_string(i + 1) + ":" + pname + "}";
      }
      snippet += ")";
      ci.insertText = snippet;
      ci.insertTextFormat = InsertTextFormat::Snippet;
    } else {
      ci.insertText = name + "()";
    }
    items.push_back(std::move(ci));
  }

  // C structs
  for (auto &[name, cs] : bindings.structs()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Struct;
    ci.detail = "(C) struct " + name;
    items.push_back(std::move(ci));
  }

  // C enums
  for (auto &[name, ce] : bindings.enums()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Enum;
    ci.detail = "(C) enum " + name;
    items.push_back(std::move(ci));
  }

  // C macros
  for (auto &[name, cm] : bindings.macros()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Constant;
    ci.detail = "(C) #define " + name;
    items.push_back(std::move(ci));
  }

  // C struct macros
  for (auto &[name, sm] : bindings.structMacros()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Constant;
    ci.detail = "(C) " + sm.structType + " " + name;
    items.push_back(std::move(ci));
  }

  // C typedefs
  for (auto &[name, ct] : bindings.typedefs()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Class;
    ci.detail = "(C) typedef " + name;
    items.push_back(std::move(ci));
  }

  // C globals
  for (auto &[name, gv] : bindings.globals()) {
    if (!matchesPrefix(name, prefix))
      continue;
    CompletionItem ci;
    ci.label = name;
    ci.kind = CompletionKind::Variable;
    ci.detail = "(C) ";
    if (gv.type)
      ci.detail += gv.type->name;
    ci.detail += " " + name;
    items.push_back(std::move(ci));
  }

  // C enum constants (flat — not behind EnumName::)
  for (auto &[ename, ce] : bindings.enums()) {
    for (auto &[valName, val] : ce.values) {
      if (!matchesPrefix(valName, prefix))
        continue;
      CompletionItem ci;
      ci.label = valName;
      ci.kind = CompletionKind::EnumMember;
      ci.detail = "(C) " + ename + " = " + std::to_string(val);
      items.push_back(std::move(ci));
    }
  }
}

void CompletionProvider::addStructFields(std::vector<CompletionItem> &items,
                                         const std::string &structName,
                                         LucisParser::ProgramContext *tree,
                                         const CBindings &bindings,
                                         const ProjectContext *project) {
  if (structName.empty())
    return;

  std::string lookupName = structName;
  std::unordered_map<std::string, std::string> subst;
  std::string base;
  std::vector<std::string> args;
  if (parseGenericInstance(structName, base, args)) {
    lookupName = base;
  }

  // Same-file struct
  auto *sd = findStructDecl(tree, lookupName);
  if (sd) {
    if (!args.empty() && sd->typeParamList()) {
      auto tps = sd->typeParamList()->typeParam();
      for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
        auto ids = tps[i]->IDENTIFIER();
        if (!ids.empty())
          subst[ids[0]->getText()] = args[i];
      }
    }
    for (auto *field : sd->structField()) {
      CompletionItem ci;
      ci.label = safeText(field->IDENTIFIER());
      ci.kind = CompletionKind::Field;
      ci.detail =
          substituteTypeParams(typeSpecToString(field->typeSpec()), subst);
      items.push_back(std::move(ci));
    }
    // Walk parent chain for inherited fields
    {
      std::string current = lookupName;
      std::unordered_set<std::string> seenParents;
      while (true) {
        std::string parentName;
        for (auto* tld2 : tree->topLevelDecl()) {
          auto* psd = tld2->structDecl();
          if (psd && safeText(psd->IDENTIFIER()) == current &&
              psd->COLON() && psd->typeSpec()) {
            parentName = typeSpecToString(psd->typeSpec());
            break;
          }
        }
        if (parentName.empty() && project && project->isValid()) {
          for (auto& mod : project->registry().allModules()) {
            auto* psym = project->registry().findSymbol(mod, current);
            if (psym && psym->kind == ExportedSymbol::Struct) {
              auto* psd = dynamic_cast<LucisParser::StructDeclContext*>(psym->decl);
              if (psd && psd->COLON() && psd->typeSpec())
                parentName = typeSpecToString(psd->typeSpec());
              break;
            }
          }
        }
        if (parentName.empty() || !seenParents.insert(parentName).second) break;
        current = parentName;

        auto* psd = findStructDecl(tree, parentName);
        if (psd) {
          for (auto* field : psd->structField()) {
            CompletionItem ci2;
            ci2.label = safeText(field->IDENTIFIER());
            ci2.kind = CompletionKind::Field;
            ci2.detail = typeSpecToString(field->typeSpec());
            items.push_back(std::move(ci2));
          }
        } else if (project && project->isValid()) {
          for (auto& mod : project->registry().allModules()) {
            auto* psym = project->registry().findSymbol(mod, parentName);
            if (psym && psym->kind == ExportedSymbol::Struct) {
              auto* pdecl = dynamic_cast<LucisParser::StructDeclContext*>(psym->decl);
              if (pdecl) {
                for (auto* field : pdecl->structField()) {
                  CompletionItem ci2;
                  ci2.label = safeText(field->IDENTIFIER());
                  ci2.kind = CompletionKind::Field;
                  ci2.detail = typeSpecToString(field->typeSpec());
                  items.push_back(std::move(ci2));
                }
              }
              break;
            }
          }
        }
      }
    }
    return;
  }

  // Same-file union
  auto *ud = findUnionDecl(tree, lookupName);
  if (ud) {
    if (!args.empty() && ud->typeParamList()) {
      auto tps = ud->typeParamList()->typeParam();
      for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
        auto ids = tps[i]->IDENTIFIER();
        if (!ids.empty())
          subst[ids[0]->getText()] = args[i];
      }
    }
    for (auto *field : ud->unionField()) {
      CompletionItem ci;
      ci.label = safeText(field->IDENTIFIER());
      ci.kind = CompletionKind::Field;
      ci.detail =
          substituteTypeParams(typeSpecToString(field->typeSpec()), subst);
      items.push_back(std::move(ci));
    }
    return;
  }

  // Cross-file struct
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto *sym = project->registry().findSymbol(ns, lookupName);
      if (!sym)
        continue;
      if (sym->kind == ExportedSymbol::Struct) {
        auto *decl = dynamic_cast<LucisParser::StructDeclContext *>(sym->decl);
        if (!decl)
          continue;
        if (!args.empty() && decl->typeParamList()) {
          auto tps = decl->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = args[i];
          }
        }
        for (auto *field : decl->structField()) {
          CompletionItem ci;
          ci.label = safeText(field->IDENTIFIER());
          ci.kind = CompletionKind::Field;
          ci.detail =
              substituteTypeParams(typeSpecToString(field->typeSpec()), subst);
          items.push_back(std::move(ci));
        }
        // Walk parent chain for inherited fields
        {
          std::string current = lookupName;
          std::unordered_set<std::string> seenParents;
          while (project && project->isValid()) {
            std::string parentName;
            for (auto& mod : project->registry().allModules()) {
              auto* psym = project->registry().findSymbol(mod, current);
              if (psym && psym->kind == ExportedSymbol::Struct) {
                auto* psd = dynamic_cast<LucisParser::StructDeclContext*>(psym->decl);
                if (psd && psd->COLON() && psd->typeSpec())
                  parentName = typeSpecToString(psd->typeSpec());
                break;
              }
            }
            if (parentName.empty() || !seenParents.insert(parentName).second) break;
            current = parentName;
            for (auto& mod : project->registry().allModules()) {
              auto* psym = project->registry().findSymbol(mod, parentName);
              if (psym && psym->kind == ExportedSymbol::Struct) {
                auto* pdecl = dynamic_cast<LucisParser::StructDeclContext*>(psym->decl);
                if (pdecl) {
                  for (auto* field : pdecl->structField()) {
                    CompletionItem ci2;
                    ci2.label = safeText(field->IDENTIFIER());
                    ci2.kind = CompletionKind::Field;
                    ci2.detail = typeSpecToString(field->typeSpec());
                    items.push_back(std::move(ci2));
                  }
                }
                break;
              }
            }
          }
        }
        return;
      }
      if (sym->kind == ExportedSymbol::Union) {
        auto *decl = dynamic_cast<LucisParser::UnionDeclContext *>(sym->decl);
        if (!decl)
          continue;
        if (!args.empty() && decl->typeParamList()) {
          auto tps = decl->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = args[i];
          }
        }
        for (auto *field : decl->unionField()) {
          CompletionItem ci;
          ci.label = safeText(field->IDENTIFIER());
          ci.kind = CompletionKind::Field;
          ci.detail =
              substituteTypeParams(typeSpecToString(field->typeSpec()), subst);
          items.push_back(std::move(ci));
        }
        return;
      }
    }
  }

  // Built-in struct from TypeRegistry (e.g. Error)
  if (auto *ti = typeRegistry_.lookup(structName)) {
    if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
      for (auto &field : ti->fields) {
        CompletionItem ci;
        ci.label = field.name;
        ci.kind = CompletionKind::Field;
        if (field.typeInfo)
          ci.detail = field.typeInfo->name;
        items.push_back(std::move(ci));
      }
      return;
    }
  }

  // C struct
  auto *cs = bindings.findStruct(structName);
  if (cs) {
    for (auto &field : cs->fields) {
      CompletionItem ci;
      ci.label = field.name;
      ci.kind = CompletionKind::Field;
      if (field.typeInfo)
        ci.detail = field.typeInfo->name;
      items.push_back(std::move(ci));
    }
  }
}

void CompletionProvider::addExtendMethods(std::vector<CompletionItem> &items,
                                          const std::string &typeName,
                                          LucisParser::ProgramContext *tree,
                                          const ProjectContext *project) {
  if (typeName.empty())
    return;

  std::string lookupName = typeName;
  std::unordered_map<std::string, std::string> subst;
  std::string base;
  std::vector<std::string> args;
  if (parseGenericInstance(typeName, base, args))
    lookupName = base;

  // Same-file extend blocks
  for (auto *tld : tree->topLevelDecl()) {
    auto *ext = tld->extendDecl();
    if (!ext || safeText(ext->IDENTIFIER()) != lookupName)
      continue;
    if (!args.empty() && ext->typeParamList()) {
      auto tps = ext->typeParamList()->typeParam();
      for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
        auto ids = tps[i]->IDENTIFIER();
        if (!ids.empty())
          subst[ids[0]->getText()] = args[i];
      }
    }
    for (auto *m : ext->extendMethod()) {
      // Skip static methods (no &self parameter)
      bool isInstance = (m->AMPERSAND() != nullptr);
      if (!isInstance)
        continue;
      CompletionItem ci;
      std::string mName = safeIdAt(m, 0);
      ci.label = mName;
      ci.kind = CompletionKind::Method;
      ci.detail = substituteTypeParams(formatMethodSignature(m), subst);
      ci.documentation = "```lucis\n" + ci.detail + "\n```";
      // Snippet with parameter placeholder for instance method params
      auto methodParams = m->param();
      if (!methodParams.empty()) {
        std::string snippet = mName + "(";
        for (size_t i = 0; i < methodParams.size(); i++) {
          if (i > 0)
            snippet += ", ";
          snippet += "${" + std::to_string(i + 1) + ":" +
                     safeText(methodParams[i]->IDENTIFIER()) + "}";
        }
        snippet += ")";
        ci.insertText = snippet;
        ci.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        ci.insertText = mName + "()";
      }
      items.push_back(std::move(ci));
    }
  }

  // Cross-file extend blocks
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto syms = project->registry().getModuleSymbols(ns);
      for (auto *sym : syms) {
        if (sym->kind != ExportedSymbol::ExtendBlock)
          continue;
        auto *ext = dynamic_cast<LucisParser::ExtendDeclContext *>(sym->decl);
        if (!ext || safeText(ext->IDENTIFIER()) != lookupName)
          continue;
        if (!args.empty() && ext->typeParamList()) {
          auto tps = ext->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(args.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = args[i];
          }
        }
        for (auto *m : ext->extendMethod()) {
          bool isInstance = (m->AMPERSAND() != nullptr);
          if (!isInstance)
            continue;
          CompletionItem ci;
          std::string mName = safeIdAt(m, 0);
          ci.label = mName;
          ci.kind = CompletionKind::Method;
          ci.detail = substituteTypeParams(formatMethodSignature(m), subst);
          ci.documentation = "```lucis\n" + ci.detail + "\n```";
          auto methodParams = m->param();
          if (!methodParams.empty()) {
            std::string snippet = mName + "(";
            for (size_t i = 0; i < methodParams.size(); i++) {
              if (i > 0)
                snippet += ", ";
              snippet += "${" + std::to_string(i + 1) + ":" +
                         safeText(methodParams[i]->IDENTIFIER()) + "}";
            }
            snippet += ")";
            ci.insertText = snippet;
            ci.insertTextFormat = InsertTextFormat::Snippet;
          } else {
            ci.insertText = mName + "()";
          }
          items.push_back(std::move(ci));
        }
      }
    }
  }

  // Walk parent chain: collect methods from parent types too
  if (project && project->isValid()) {
    std::string current = lookupName;
    std::unordered_set<std::string> seen;

    while (true) {
      // Find struct declaration to get parent name
      std::string parentName;
      const ExportedSymbol* sym = nullptr;
      for (auto& mod : project->registry().allModules()) {
        sym = project->registry().findSymbol(mod, current);
        if (sym && sym->kind == ExportedSymbol::Struct) break;
      }
      if (sym && sym->kind == ExportedSymbol::Struct) {
        auto* sd = dynamic_cast<LucisParser::StructDeclContext*>(sym->decl);
        if (sd && sd->COLON() && sd->typeSpec())
          parentName = sd->typeSpec()->getText();
      }
      // Also check same-file for struct with parent
      if (parentName.empty()) {
        for (auto* tld : tree->topLevelDecl()) {
          auto* sd = tld->structDecl();
          if (sd && safeText(sd->IDENTIFIER()) == current &&
              sd->COLON() && sd->typeSpec()) {
            parentName = sd->typeSpec()->getText();
            break;
          }
        }
      }
      if (parentName.empty() || !seen.insert(parentName).second)
        break;

      current = parentName;

      // Same-file extend blocks for parent
      for (auto* tld : tree->topLevelDecl()) {
        auto* ext = tld->extendDecl();
        if (!ext || safeText(ext->IDENTIFIER()) != parentName) continue;
        for (auto* m : ext->extendMethod()) {
          if (m->AMPERSAND() == nullptr) continue;
          CompletionItem ci;
          std::string mName = safeIdAt(m, 0);
          ci.label = mName;
          ci.kind = CompletionKind::Method;
          ci.detail = formatMethodSignature(m);
          ci.documentation = "```lucis\n" + ci.detail + "\n```";
          auto methodParams = m->param();
          if (!methodParams.empty()) {
            std::string snippet = mName + "(";
            for (size_t i = 0; i < methodParams.size(); i++) {
              if (i > 0) snippet += ", ";
              snippet += "${" + std::to_string(i + 1) + ":" +
                         safeText(methodParams[i]->IDENTIFIER()) + "}";
            }
            snippet += ")";
            ci.insertText = snippet;
            ci.insertTextFormat = InsertTextFormat::Snippet;
          } else {
            ci.insertText = mName + "()";
          }
          items.push_back(std::move(ci));
        }
      }

      // Cross-file extend blocks for parent
      for (auto& ns : project->registry().allModules()) {
        auto syms = project->registry().getModuleSymbols(ns);
        for (auto* psym : syms) {
          if (psym->kind != ExportedSymbol::ExtendBlock) continue;
          auto* ext = dynamic_cast<LucisParser::ExtendDeclContext*>(psym->decl);
          if (!ext || safeText(ext->IDENTIFIER()) != parentName) continue;
          for (auto* m : ext->extendMethod()) {
            if (m->AMPERSAND() == nullptr) continue;
            CompletionItem ci;
            std::string mName = safeIdAt(m, 0);
            ci.label = mName;
            ci.kind = CompletionKind::Method;
            ci.detail = formatMethodSignature(m);
            ci.documentation = "```lucis\n" + ci.detail + "\n```";
            auto methodParams = m->param();
            if (!methodParams.empty()) {
              std::string snippet = mName + "(";
              for (size_t i = 0; i < methodParams.size(); i++) {
                if (i > 0) snippet += ", ";
                snippet += "${" + std::to_string(i + 1) + ":" +
                           safeText(methodParams[i]->IDENTIFIER()) + "}";
              }
              snippet += ")";
              ci.insertText = snippet;
              ci.insertTextFormat = InsertTextFormat::Snippet;
            } else {
              ci.insertText = mName + "()";
            }
            items.push_back(std::move(ci));
          }
        }
      }
    }
  }
}

// ─── Built-in type methods (int.abs, string.len, vec.push, etc.) ────

void CompletionProvider::addTypeMethods(std::vector<CompletionItem> &items,
                                        const std::string &typeName,
                                        const std::string &prefix) {
  if (typeName.empty())
    return;

  // 1) Check for array type: []T or [N]T
  bool isArray = false;
  std::string elemTypeName;
  if (typeName.size() > 2 && typeName[0] == '[') {
    isArray = true;
    auto closeBracket = typeName.find(']');
    if (closeBracket != std::string::npos && closeBracket + 1 < typeName.size())
      elemTypeName = typeName.substr(closeBracket + 1);
  }

  if (isArray) {
    auto arrayMethods = methodRegistry_.arrayMethods();
    for (auto *md : arrayMethods) {
      if (!matchesPrefix(md->name, prefix))
        continue;
      if (md->requireNumeric) {
        auto *elemTI = typeRegistry_.lookup(elemTypeName);
        if (!elemTI || (elemTI->kind != TypeKind::Integer &&
                        elemTI->kind != TypeKind::Float))
          continue;
      }
      items.push_back(buildMethodItem(*md, typeName, elemTypeName));
    }
    return;
  }

  // 2) Check for extended type: Vec<T>, Map<K,V>, Set<T>
  auto angleBracket = typeName.find('<');
  if (angleBracket != std::string::npos) {
    std::string baseName = typeName.substr(0, angleBracket);
    std::string innerType;
    std::string keyType, valType;
    if (angleBracket + 1 < typeName.size()) {
      auto closeAngle = typeName.rfind('>');
      if (closeAngle != std::string::npos && closeAngle > angleBracket)
        innerType =
            typeName.substr(angleBracket + 1, closeAngle - angleBracket - 1);
    }
    // For Map<K,V>, split inner on comma
    auto comma = innerType.find(',');
    if (comma != std::string::npos) {
      keyType = innerType.substr(0, comma);
      valType = innerType.substr(comma + 1);
      // Trim whitespace
      while (!valType.empty() && valType[0] == ' ')
        valType.erase(0, 1);
    } else {
      // Vec<T> / Set<T> — element type is the inner
      keyType = innerType;
      valType = innerType;
    }

    auto *extDesc = extTypeRegistry_.lookup(normalizeExtBaseName(baseName));
    if (extDesc) {
      for (auto &md : extDesc->methods) {
        if (!matchesPrefix(md.name, prefix))
          continue;
        if (md.requireNumeric) {
          auto *elemTI = typeRegistry_.lookup(innerType);
          if (!elemTI || (elemTI->kind != TypeKind::Integer &&
                          elemTI->kind != TypeKind::Float))
            continue;
        }
        items.push_back(
            buildMethodItem(md, typeName, innerType, keyType, valType));
      }
      return;
    }
  }

  // 3) Built-in primitive type methods via TypeRegistry + MethodRegistry
  auto *typeInfo = typeRegistry_.lookup(typeName);
  if (!typeInfo)
    return;

  auto methods = methodRegistry_.methodsFor(typeInfo->kind);
  for (auto *md : methods) {
    if (!matchesPrefix(md->name, prefix))
      continue;
    if (md->requireSigned && typeInfo->kind == TypeKind::Integer &&
        !typeInfo->isSigned)
      continue;
    if (md->requireUnsigned && typeInfo->kind == TypeKind::Integer &&
        typeInfo->isSigned)
      continue;
    items.push_back(buildMethodItem(*md, typeName, ""));
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  Use import path completions
// ═══════════════════════════════════════════════════════════════════════

// Scan a directory for .lc files and collect their stem names.
// Also discover subdirectory packages (dirs containing .lc files).
static void scanLcFiles(const fs::path& dir,
                         std::unordered_set<std::string>& out,
                         const std::string& prefix) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".lc") {
            auto name = entry.path().stem().string();
            if (name.empty() || name == "main") continue;
            out.insert(name);
        } else if (entry.is_directory()) {
            // If the subdirectory contains at least one .lc file,
            // it's a package module (e.g. sd/ → module "sd").
            auto dname = entry.path().filename().string();
            if (dname.empty() || dname[0] == '.') continue;
            std::error_code ec2;
            bool hasLc = false;
            for (auto& sub : fs::directory_iterator(entry.path(), ec2)) {
                if (ec2) break;
                if (sub.is_regular_file() && sub.path().extension() == ".lc") {
                    hasLc = true;
                    break;
                }
                if (sub.is_directory()) { hasLc = true; break; }
            }
            if (hasLc) out.insert(dname);
        }
    }
}

void CompletionProvider::addUseCompletions(std::vector<CompletionItem> &items,
                                           const std::string &modulePath,
                                           const std::string &prefix,
                                           const ProjectContext *project) {

  // Helper: known std modules from ImportResolver (single source of truth).
  auto& stdModules = ImportResolver::knownModules();

  // Collect submodule names from knownModules.
  static const std::vector<std::string> stdSubmodules = [] {
    std::vector<std::string> result;
    for (auto &[fullPath, _] : ImportResolver::knownModules()) {
      auto pos = fullPath.find("::");
      if (pos != std::string::npos)
        result.push_back(fullPath.substr(pos + 2));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }();

  // Case 1: modulePath is empty — completing right after "use "
  // Suggest "std" + project namespaces
  if (modulePath.empty()) {
    if (matchesPrefix("std", prefix)) {
      CompletionItem ci;
      ci.label = "std";
      ci.kind = CompletionKind::Module;
      ci.detail = "standard library";
      ci.insertText = "std::";
      items.push_back(std::move(ci));
    }

    // Project namespaces — always scan filesystem even if project not built yet
    {
      std::unordered_set<std::string> seenTopLevel;

      // 1) Collect top-level modules from the project registry (only if built).
      if (project && project->isValid()) {
        for (auto &ns : project->registry().allModules()) {
          if (ns.substr(0, 5) == "std::")
            continue;
          auto topLevel = ns;
          auto sep = ns.find("::");
          if (sep == std::string::npos) sep = ns.find('/');
          if (sep != std::string::npos)
            topLevel = ns.substr(0, sep);
          seenTopLevel.insert(topLevel);
        }
      }

      // 2) Scan stdlib directories for .lc files.
      for (auto& dir : ImportResolver::stdlibSearchPaths()) {
        scanLcFiles(fs::path(dir), seenTopLevel, prefix);
      }

      // 3) Scan the project source directories for .lc files
      //    (even if project is not yet built/isValid).
      if (project) {
        auto root = fs::path(project->projectRoot());
        scanLcFiles(root, seenTopLevel, prefix);
        for (auto& sp : project->sourcePaths()) {
          scanLcFiles(root / sp, seenTopLevel, prefix);
        }
      }

      // 4) Emit items for all collected names matching the prefix.
      {
        auto& known = ImportResolver::knownModules();
        for (auto& name : seenTopLevel) {
          if (!matchesPrefix(name, prefix)) continue;
          CompletionItem ci;
          ci.label = name;
          ci.kind = CompletionKind::Module;
          bool inStdlib = known.count("std::" + name);
          bool inRegistry = project && project->isValid()
            && project->registry().hasModule(
                 ModuleRegistry::usePathToModulePath(name));
          if (inStdlib)
            ci.detail = "stdlib";
          else if (inRegistry)
            ci.detail = "module";
          else
            ci.detail = "module (file)";
          ci.insertText = name + "::";
          items.push_back(std::move(ci));
        }
      }
    }

    // #[error] attribute — marks an enum variant as the error variant
    bool matchAttrError = (!prefix.empty() && prefix[0] == '#' &&
                            matchesPrefix("[error]", prefix.substr(1))) ||
                           matchesPrefix("#[error]", prefix);
    if (matchAttrError) {
      CompletionItem ci;
      ci.label = "#[error]";
      ci.kind = CompletionKind::Keyword;
      ci.detail = "Marks enum variant as the error variant for ? and catch";
      ci.insertText = "#[error]";
      ci.filterText = "#[error]";
      items.push_back(std::move(ci));
    }

    return;
  }

  // Case 2: modulePath is "std" — completing after "use std::"
  // Suggest submodule names (log, math, collections, etc.)
  if (modulePath == "std") {
    for (auto &sub : stdSubmodules) {
      if (!matchesPrefix(sub, prefix))
        continue;
      CompletionItem ci;
      ci.label = sub;
      ci.kind = CompletionKind::Module;
      ci.detail = "std::" + sub;
      ci.insertText = sub + "::";
      items.push_back(std::move(ci));
    }
    return;
  }

  // Case 2.5: user module root — suggest submodules
  if (project && !modulePath.empty()) {
    std::string slashedPath = ModuleRegistry::usePathToModulePath(modulePath);
    std::unordered_set<std::string> submodules;
    // Only built projects have a populated registry.
    if (project->isValid()) {
      for (auto& mod : project->registry().allModules()) {
        if (mod.substr(0, slashedPath.size() + 1) != slashedPath + "/")
          continue;
        auto rest = mod.substr(slashedPath.size() + 1);
        auto nextSep = rest.find('/');
        auto sub = (nextSep != std::string::npos) ? rest.substr(0, nextSep) : rest;
        if (!matchesPrefix(sub, prefix)) continue;
        if (!submodules.insert(sub).second) continue;
        CompletionItem ci;
        ci.label = sub;
        ci.kind = CompletionKind::Module;
        ci.detail = modulePath + "::" + sub;
        ci.insertText = sub + "::";
        items.push_back(std::move(ci));
      }
    }
    // Filesystem fallback: scan project directories for the subdirectory.
    {
      auto root = fs::path(project->projectRoot());
      std::vector<fs::path> searchDirs = { root };
      for (auto& sp : project->sourcePaths())
        searchDirs.push_back(root / fs::path(sp));
      for (auto& dir : searchDirs) {
        auto subdir = dir / slashedPath;
        std::error_code ec;
        if (!fs::is_directory(subdir, ec)) continue;
        for (auto& entry : fs::directory_iterator(subdir, ec)) {
          if (ec) break;
          std::string name;
          if (entry.is_regular_file() && entry.path().extension() == ".lc") {
            name = entry.path().stem().string();
          } else if (entry.is_directory()) {
            name = entry.path().filename().string();
          }
          if (name.empty() || name == "main") continue;
          if (!matchesPrefix(name, prefix)) continue;
          if (!submodules.insert(name).second) continue;
          CompletionItem ci;
          ci.label = name;
          ci.kind = CompletionKind::Module;
          ci.detail = modulePath + "::" + name;
          ci.insertText = name + "::";
          items.push_back(std::move(ci));
        }
      }
    }
    // Don't return — fall through to show module symbols too.
  }

  // Case 3: modulePath is a full std module (e.g. "std::log") — suggest symbols
  auto it = stdModules.find(modulePath);
  if (it != stdModules.end()) {
    for (auto &sym : it->second) {
      if (!matchesPrefix(sym, prefix))
        continue;
      CompletionItem ci;
      ci.label = sym;
      // Heuristic: uppercase = type/constant, lowercase = function
      if (!sym.empty() && std::isupper(sym[0])) {
        ci.kind = CompletionKind::Class;
        ci.detail = "type";
      } else {
        ci.kind = CompletionKind::Function;
        ci.detail = "function";
      }
      items.push_back(std::move(ci));
    }
    return;
  }

  // Case 4: User module — suggest exported symbols
  if (project) {
    auto registryPath = ModuleRegistry::usePathToModulePath(modulePath);

    // Only built projects have a populated registry.
    std::vector<const ExportedSymbol*> syms;
    if (project->isValid())
      syms = project->registry().getModuleSymbols(registryPath);

    // Fallback: if not in project registry, try parsing stdlib directly
    if (syms.empty()) {
      static const std::vector<std::string> stdlibDirs2 = ImportResolver::stdlibSearchPaths();
      for (auto& dir : stdlibDirs2) {
        auto candidate = fs::path(dir) / (registryPath + ".lc");
        std::error_code ec;
        if (!fs::exists(candidate, ec) || ec) continue;
        auto parseResult = project->getStdlibParse(candidate.string());
        if (!parseResult) {
          try {
            parseResult = std::make_shared<ParseResult>(Parser::parse(candidate.string()));
          } catch (...) { continue; }
        }
        if (!parseResult->tree) continue;
        // Extract symbols directly from parse tree (avoids dangling decl pointers)
        for (auto* tld : parseResult->tree->topLevelDecl()) {
          std::string name;
          int kind = -1; // 0=fn, 1=struct, 2=enum, 3=union, 4=typealias
          if (auto* fd = tld->functionDecl()) {
            if (!fd->IDENTIFIER().empty()) name = safeText(fd->IDENTIFIER(0)); kind = 0;
          } else if (auto* sd = tld->structDecl()) {
            name = safeText(sd->IDENTIFIER()); kind = 1;
          } else if (auto* ed = tld->enumDecl()) {
            name = safeText(ed->IDENTIFIER()); kind = 2;
          } else if (auto* ud = tld->unionDecl()) {
            name = safeText(ud->IDENTIFIER()); kind = 3;
          } else if (auto* ta = tld->typeAliasDecl()) {
            name = safeText(ta->IDENTIFIER()); kind = 4;
          } else if (auto* ext = tld->extendDecl()) {
            continue; // skip extend blocks in completion
          }
          if (name.empty() || !matchesPrefix(name, prefix)) continue;
          CompletionItem ci;
          ci.label = name;
          switch (kind) {
          case 0: ci.kind = CompletionKind::Function; ci.detail = "function"; break;
          case 1: ci.kind = CompletionKind::Struct;   ci.detail = "struct";   break;
          case 2: ci.kind = CompletionKind::Enum;     ci.detail = "enum";     break;
          case 3: ci.kind = CompletionKind::Struct;   ci.detail = "union";    break;
          case 4: ci.kind = CompletionKind::Class;    ci.detail = "type alias"; break;
          default: continue;
          }
          items.push_back(std::move(ci));
        }
        return;
      }

      // Project file fallback: search project directories for .lc file
      {
        auto root = fs::path(project->projectRoot());
        std::vector<fs::path> searchDirs = { root };
        for (auto& sp : project->sourcePaths())
          searchDirs.push_back(root / fs::path(sp));
        for (auto& dir : searchDirs) {
          auto candidate = dir / (registryPath + ".lc");
          std::error_code ec;
          if (!fs::exists(candidate, ec) || ec) continue;
          try {
            auto parseResult = Parser::parse(candidate.string());
            if (!parseResult.tree) continue;
            for (auto* tld : parseResult.tree->topLevelDecl()) {
              std::string name;
              int kind = -1;
              if (auto* fd = tld->functionDecl()) {
                if (!fd->IDENTIFIER().empty()) name = safeText(fd->IDENTIFIER(0)); kind = 0;
              } else if (auto* sd = tld->structDecl()) {
                name = safeText(sd->IDENTIFIER()); kind = 1;
              } else if (auto* ed = tld->enumDecl()) {
                name = safeText(ed->IDENTIFIER()); kind = 2;
              } else if (auto* ud = tld->unionDecl()) {
                name = safeText(ud->IDENTIFIER()); kind = 3;
              } else if (auto* ta = tld->typeAliasDecl()) {
                name = safeText(ta->IDENTIFIER()); kind = 4;
              } else if (auto* ext = tld->extendDecl()) {
                continue;
              }
              if (name.empty() || !matchesPrefix(name, prefix)) continue;
              CompletionItem ci;
              ci.label = name;
              switch (kind) {
              case 0: ci.kind = CompletionKind::Function; ci.detail = "function"; break;
              case 1: ci.kind = CompletionKind::Struct;   ci.detail = "struct";   break;
              case 2: ci.kind = CompletionKind::Enum;     ci.detail = "enum";     break;
              case 3: ci.kind = CompletionKind::Struct;   ci.detail = "union";    break;
              case 4: ci.kind = CompletionKind::Class;    ci.detail = "type alias"; break;
              default: continue;
              }
              items.push_back(std::move(ci));
            }
            return;
          } catch (...) { continue; }
        }
      }
    }

    for (auto *sym : syms) {
      if (!matchesPrefix(sym->name, prefix))
        continue;
      CompletionItem ci;
      ci.label = sym->name;
      switch (sym->kind) {
      case ExportedSymbol::Function:
        ci.kind = CompletionKind::Function;
        ci.detail = "function";
        break;
      case ExportedSymbol::Struct:
        ci.kind = CompletionKind::Struct;
        ci.detail = "struct";
        break;
      case ExportedSymbol::Enum:
        ci.kind = CompletionKind::Enum;
        ci.detail = "enum";
        break;
      case ExportedSymbol::Union:
        ci.kind = CompletionKind::Struct;
        ci.detail = "union";
        break;
      case ExportedSymbol::TypeAlias:
        ci.kind = CompletionKind::Class;
        ci.detail = "type alias";
        break;
      case ExportedSymbol::ExtendBlock:
        continue;
      }
      items.push_back(std::move(ci));
    }
  }
}

void CompletionProvider::addEnumVariants(std::vector<CompletionItem> &items,
                                         const std::string &enumName,
                                         LucisParser::ProgramContext *tree,
                                         const CBindings &bindings,
                                         const ProjectContext *project,
                                         const std::string &prefix) {
  // Resolve enum name through type aliases (local + cross-file).
  // e.g. type DivideResult = Result<int32> → "Result"
  auto resolveEnumAlias = [&](const std::string& name) -> std::string {
    // Local type alias
    for (auto* tld : tree->topLevelDecl()) {
      if (auto* ta = tld->typeAliasDecl()) {
        if (safeText(ta->IDENTIFIER()) == name) {
          auto aliased = safeText(ta->typeSpec());
          auto base = aliased;
          auto lt = base.find('<');
          if (lt != std::string::npos) base = base.substr(0, lt);
          return base;
        }
      }
    }
    // Cross-file type alias
    if (project && project->isValid()) {
      for (auto& ns : project->registry().allModules()) {
        auto* sym = project->registry().findSymbol(ns, name);
        if (!sym || sym->kind != ExportedSymbol::TypeAlias) continue;
        auto* ta = dynamic_cast<LucisParser::TypeAliasDeclContext*>(sym->decl);
        if (!ta || !ta->typeSpec()) continue;
        auto aliased = safeText(ta->typeSpec());
        auto base = aliased;
        auto lt = base.find('<');
        if (lt != std::string::npos) base = base.substr(0, lt);
        return base;
      }
    }
    return "";
  };
  std::string realEnumName = enumName;
  auto resolved = resolveEnumAlias(enumName);
  if (!resolved.empty()) realEnumName = resolved;

  // Strip generic type arguments: Result<int32> → Result
  {
    auto lt = realEnumName.find('<');
    if (lt != std::string::npos)
      realEnumName = realEnumName.substr(0, lt);
  }

  // Same-file enum
  auto *ed = findEnumDecl(tree, realEnumName);
  if (ed) {
    for (auto *variant : ed->enumVariant()) {
      auto *v = variant->IDENTIFIER();
      if (!v)
        continue;
      if (!matchesPrefix(v->getText(), prefix))
        continue;
      CompletionItem ci;
      ci.label = v->getText();
      ci.kind = CompletionKind::EnumMember;
      ci.detail = enumName + "::" + v->getText();
      items.push_back(std::move(ci));
    }
    return;
  }

  // Cross-file enum
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto *sym = project->registry().findSymbol(ns, realEnumName);
      if (!sym || sym->kind != ExportedSymbol::Enum)
        continue;
      auto *decl = dynamic_cast<LucisParser::EnumDeclContext *>(sym->decl);
      if (!decl)
        continue;
      for (auto *variant : decl->enumVariant()) {
        auto *v = variant->IDENTIFIER();
        if (!v)
          continue;
        if (!matchesPrefix(v->getText(), prefix))
          continue;
        CompletionItem ci;
        ci.label = v->getText();
        ci.kind = CompletionKind::EnumMember;
        ci.detail = enumName + "::" + v->getText();
        items.push_back(std::move(ci));
      }
      return;
    }
  }

  // C enum
  auto *ce = bindings.findEnum(enumName);
  if (ce) {
    for (auto &[valName, val] : ce->values) {
      if (!matchesPrefix(valName, prefix))
        continue;
      CompletionItem ci;
      ci.label = valName;
      ci.kind = CompletionKind::EnumMember;
      ci.detail = enumName + "::" + valName + " = " + std::to_string(val);
      items.push_back(std::move(ci));
    }
  }
}

static std::optional<std::string>
resolveImportedModuleAlias(const std::string &alias,
                           LucisParser::ProgramContext *tree,
                           const ProjectContext *project) {
  if (!tree)
    return std::nullopt;

  for (auto *pre : tree->preambleDecl()) {
    auto *useDecl = pre->useDecl();
    if (!useDecl)
      continue;

    // use std::log::println; (alias is println, modulePath is std::log)
    if (auto *item = dynamic_cast<LucisParser::UseItemContext *>(useDecl)) {
      if (!item->modulePath() || !item->IDENTIFIER())
        continue;
      if (safeText(item->IDENTIFIER()) != alias)
        continue;

      std::string modulePath;
      for (auto *id : item->modulePath()->IDENTIFIER()) {
        if (!modulePath.empty())
          modulePath += "::";
        modulePath += id->getText();
      }

      auto fullPath = modulePath + "::" + alias;
      if (modulePath == "std") {
        return fullPath;
      }
      if (project && project->isValid() &&
          project->registry().hasModule(
              ModuleRegistry::usePathToModulePath(fullPath))) {
        return fullPath;
      }
      return std::nullopt;
    }

    // use std; (simple root import, alias is the module name itself)
    if (auto *root = dynamic_cast<LucisParser::UseRootContext *>(useDecl)) {
      if (!root->IDENTIFIER())
        continue;
      if (safeText(root->IDENTIFIER()) == alias) {
        return alias;
      }
    }
  }
  return std::nullopt;
}

void CompletionProvider::addStaticMethods(std::vector<CompletionItem> &items,
                                          const std::string &typeName,
                                          LucisParser::ProgramContext *tree,
                                          const ProjectContext *project,
                                          const std::string &prefix) {
  // Resolve imported module aliases like `use std::log;` → `log`
  if (auto aliasedPath = resolveImportedModuleAlias(typeName, tree, project)) {
    std::vector<CompletionItem> moduleItems;
    addUseCompletions(moduleItems, *aliasedPath, prefix, project);
    for (auto &ci : moduleItems) {
      ci.insertText = ci.label;
      items.push_back(std::move(ci));
    }
    return;
  }

  // std scope chains (std::, std::log::, etc.)
  if (typeName == "std" || typeName.rfind("std::", 0) == 0) {
    std::vector<CompletionItem> moduleItems;
    addUseCompletions(moduleItems, typeName, prefix, project);
    for (auto &ci : moduleItems) {
      // In expression scope access we only want the symbol/module name,
      // never extra scope separators in the inserted text.
      ci.insertText = ci.label;
      items.push_back(std::move(ci));
    }
    return;
  }

  std::string baseTypeName = typeName;
  auto lastScope = baseTypeName.rfind("::");
  if (lastScope != std::string::npos)
    baseTypeName = baseTypeName.substr(lastScope + 2);

  // Same-file extend blocks
  for (auto *tld : tree->topLevelDecl()) {
    auto *ext = tld->extendDecl();
    if (!ext || safeText(ext->IDENTIFIER()) != baseTypeName)
      continue;
    for (auto *m : ext->extendMethod()) {
      bool isStatic = (m->AMPERSAND() == nullptr);
      if (!isStatic)
        continue;
      CompletionItem ci;
      std::string mName = safeIdAt(m, 0);
      if (!matchesPrefix(mName, prefix))
        continue;
      ci.label = mName;
      ci.kind = CompletionKind::Function;
      ci.detail = formatMethodSignature(m);
      ci.documentation = "```lucis\n" + ci.detail + "\n```";
      if (auto *pl = m->paramList(); pl && !pl->param().empty()) {
        auto pList = pl->param();
        std::string snippet = mName + "(";
        for (size_t i = 0; i < pList.size(); i++) {
          if (i > 0)
            snippet += ", ";
          snippet += "${" + std::to_string(i + 1) + ":" +
                     safeText(pList[i]->IDENTIFIER()) + "}";
        }
        snippet += ")";
        ci.insertText = snippet;
        ci.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        ci.insertText = mName + "()";
      }
      items.push_back(std::move(ci));
    }
  }

  // Cross-file extend blocks
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto syms = project->registry().getModuleSymbols(ns);
      for (auto *sym : syms) {
        if (sym->kind != ExportedSymbol::ExtendBlock)
          continue;
        auto *ext = dynamic_cast<LucisParser::ExtendDeclContext *>(sym->decl);
        if (!ext || safeText(ext->IDENTIFIER()) != baseTypeName)
          continue;
        for (auto *m : ext->extendMethod()) {
          bool isStatic = (m->AMPERSAND() == nullptr);
          if (!isStatic)
            continue;
          CompletionItem ci;
          std::string mName = safeIdAt(m, 0);
          if (!matchesPrefix(mName, prefix))
            continue;
          ci.label = mName;
          ci.kind = CompletionKind::Function;
          ci.detail = formatMethodSignature(m);
          ci.documentation = "```lucis\n" + ci.detail + "\n```";
          if (auto *pl = m->paramList(); pl && !pl->param().empty()) {
            auto pList = pl->param();
            std::string snippet = mName + "(";
            for (size_t i = 0; i < pList.size(); i++) {
              if (i > 0)
                snippet += ", ";
              snippet += "${" + std::to_string(i + 1) + ":" +
                         safeText(pList[i]->IDENTIFIER()) + "}";
            }
            snippet += ")";
            ci.insertText = snippet;
            ci.insertTextFormat = InsertTextFormat::Snippet;
          } else {
            ci.insertText = mName + "()";
          }
          items.push_back(std::move(ci));
        }
      }
    }
  }
}

void CompletionProvider::addTypeNames(std::vector<CompletionItem> &items,
                                      LucisParser::ProgramContext *tree,
                                      const CBindings &bindings,
                                      const ProjectContext *project,
                                      const std::string &prefix) {
  // Primitive types
  static const char *primitives[] = {
      "auto",    "int1",  "int8",    "int16",   "int32",   "int64",    "int128",
      "intinf",  "isize", "uint1",   "uint8",   "uint16",  "uint32",   "uint64",
      "uint128", "usize", "float32", "float64", "float80", "float128", "double",
      "bool",    "char",  "void",    "string",  "cstring",
  };
  std::unordered_set<std::string> addedPrimitives;
  for (auto *p : primitives) {
    if (!matchesPrefix(p, prefix))
      continue;
    addedPrimitives.insert(p);
    CompletionItem ci;
    ci.label = p;
    ci.kind = CompletionKind::Keyword;
    ci.detail = "primitive type";
    items.push_back(std::move(ci));
  }

  // Types registered by intrinsics (e.g. va_list) or other dynamic sources
  for (auto &typeName : typeRegistry_.allTypes()) {
    if (addedPrimitives.count(typeName))
      continue;
    if (!matchesPrefix(typeName.c_str(), prefix))
      continue;
    CompletionItem ci;
    ci.label = typeName;
    ci.kind = CompletionKind::Keyword;
    ci.detail = "intrinsic type";
    items.push_back(std::move(ci));
  }

  // Built-in non-primitive language types.
  if (matchesPrefix("Error", prefix)) {
    CompletionItem ci;
    ci.label = "Error";
    ci.kind = CompletionKind::Struct;
    ci.detail = "built-in struct Error";
    items.push_back(std::move(ci));
  }

  // Collection / generic types (vec, map, set)
  {
    struct GenericType {
      const char *lower;
      const char *snippet;
      const char *detail;
    };
    static const GenericType generics[] = {
        {"vec", "vec<${1:T}>", "generic collection — dynamic array"},
        {"map", "map<${1:K}, ${2:V}>", "generic collection — hash map"},
        {"set", "set<${1:T}>", "generic collection — hash set"},
    };
    for (auto &g : generics) {
      if (!matchesPrefix(g.lower, prefix))
        continue;
      CompletionItem ci;
      ci.label = g.lower;
      ci.kind = CompletionKind::Class;
      ci.detail = g.detail;
      ci.insertText = g.snippet;
      ci.insertTextFormat = InsertTextFormat::Snippet;
      items.push_back(std::move(ci));
    }
  }

  // Same-file structs, enums, unions, type aliases
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *sd = tld->structDecl()) {
      std::string name = safeText(sd->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      items.push_back({name, CompletionKind::Struct, "struct " + name});
    }
    if (auto *ed = tld->enumDecl()) {
      if (!ed->IDENTIFIER())
        continue;
      std::string name = safeText(ed->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      items.push_back({name, CompletionKind::Enum, "enum " + name});
    }
    if (auto *ud = tld->unionDecl()) {
      std::string name = safeText(ud->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      CompletionItem item;
      item.label = name;
      item.kind = CompletionKind::Struct;
      if (ud->typeParamList()) {
        std::string params;
        bool first = true;
        for (auto *tp : ud->typeParamList()->typeParam()) {
          auto ids = tp->IDENTIFIER();
          if (!first)
            params += ", ";
          if (!ids.empty())
            params += ids[0]->getText();
          if (tp->COLON() && ids.size() >= 2)
            params += ": " + ids[1]->getText();
          first = false;
        }
        item.detail = "union " + name + "<" + params + ">";
        item.insertText = name + "<${1}>";
        item.insertTextFormat = InsertTextFormat::Snippet;
      } else {
        item.detail = "union " + name;
      }
      items.push_back(std::move(item));
    }
    if (auto *ta = tld->typeAliasDecl()) {
      std::string name = safeText(ta->IDENTIFIER());
      if (!matchesPrefix(name, prefix))
        continue;
      items.push_back({name, CompletionKind::Class, "type " + name});
    }
  }

  // C structs/enums/typedefs
  for (auto &[name, _] : bindings.structs()) {
    if (!matchesPrefix(name, prefix))
      continue;
    items.push_back({name, CompletionKind::Struct, "(C) struct " + name});
  }
  for (auto &[name, _] : bindings.enums()) {
    if (!matchesPrefix(name, prefix))
      continue;
    items.push_back({name, CompletionKind::Enum, "(C) enum " + name});
  }
  for (auto &[name, _] : bindings.typedefs()) {
    if (!matchesPrefix(name, prefix))
      continue;
    items.push_back({name, CompletionKind::Class, "(C) typedef " + name});
  }

  // Cross-file types
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto syms = project->registry().getModuleSymbols(ns);
      for (auto *sym : syms) {
        if (sym->kind != ExportedSymbol::Struct &&
            sym->kind != ExportedSymbol::Enum &&
            sym->kind != ExportedSymbol::Union &&
            sym->kind != ExportedSymbol::TypeAlias)
          continue;
        if (!matchesPrefix(sym->name, prefix))
          continue;
        CompletionKind k = CompletionKind::Class;
        if (sym->kind == ExportedSymbol::Struct)
          k = CompletionKind::Struct;
        else if (sym->kind == ExportedSymbol::Enum)
          k = CompletionKind::Enum;
        items.push_back({sym->name, k, "[" + ns + "] " + sym->name});
      }
    }
  }
}

void CompletionProvider::addMatchArmCompletions(
    std::vector<CompletionItem>& items,
    LucisParser::ProgramContext* tree, size_t cursorLine,
    const std::string& prefix, const ProjectContext* project,
    const std::string& source) {
  if (!tree) return;

  // Parse source into lines for scanning
  std::istringstream ss(source);
  std::vector<std::string> lines;
  std::string ln;
  while (std::getline(ss, ln)) lines.push_back(ln);
  if (cursorLine >= lines.size()) return;

  // Check if cursor is in an expression arm (after "->" without "{")
  std::string currentLine = lines[cursorLine];
  auto arrowPos = currentLine.rfind("->");
  if (arrowPos != std::string::npos) {
    std::string afterArrow = currentLine.substr(arrowPos + 2);
    // If no "{" after "->", we're in an expression arm - don't suggest variants
    if (afterArrow.find('{') == std::string::npos) {
      return;
    }
  }

  // Find the matched variable by scanning backwards for "match <var> {"
  int depth = 0;
  std::string varName;
  for (int i = static_cast<int>(cursorLine); i >= 0; --i) {
    for (size_t j = 0; j < lines[i].size(); ++j) {
      if (lines[i][j] == '}') ++depth;
      else if (lines[i][j] == '{') {
        if (depth > 0) { --depth; continue; }
        std::string before = lines[i].substr(0, j);
        // Skip arm body blocks (preceded by "->")
        if (before.find("->") != std::string::npos) {
          ++depth;
          continue;
        }
        if (i > 0) {
          std::string prevLine = lines[i-1];
          size_t ws2 = prevLine.find_last_not_of(" \t\r\n");
          if (ws2 != std::string::npos && prevLine[ws2] == '>'
              && ws2 > 0 && prevLine[ws2-1] == '-') {
            ++depth;
            continue;
          }
        }
        if (i > 0) before = lines[i-1] + " " + before;
        auto mp = before.rfind("match");
        if (mp != std::string::npos) {
          std::string after = before.substr(mp + 5);
          size_t s2 = after.find_first_not_of(" \t\r\n");
          if (s2 != std::string::npos) {
            after = after.substr(s2);
            for (char c : after) {
              if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                varName += c;
              else break;
            }
          }
        }
        goto found;
      }
    }
  }
  return;

found:
  if (varName.empty()) return;

  // Resolve variable type
  std::string typeName = inferVarType(varName, tree, cursorLine, nullptr, project);
  if (typeName.empty()) return;

  std::string baseName = typeName;
  auto lt = baseName.find('<');
  if (lt != std::string::npos) baseName = baseName.substr(0, lt);

  // Find enum declaration
  LucisParser::EnumDeclContext* ed = findEnumDecl(tree, baseName);
  if (!ed && project && project->isValid()) {
    for (auto& ns : project->registry().allModules()) {
      auto* sym = project->registry().findSymbol(ns, baseName);
      if (!sym || sym->kind != ExportedSymbol::Enum) continue;
      ed = dynamic_cast<LucisParser::EnumDeclContext*>(sym->decl);
      if (ed) break;
    }
  }
  if (!ed) return;

  // Push variants
  for (auto* variant : ed->enumVariant()) {
    auto* vId = variant->IDENTIFIER();
    if (!vId) continue;
    if (!matchesPrefix(vId->getText(), prefix)) continue;
    CompletionItem ci;
    ci.label = vId->getText();
    ci.kind = CompletionKind::EnumMember;
    ci.detail = baseName + "::" + vId->getText();
    if (variant->LPAREN()) {
      ci.insertText = vId->getText() + "(${1:_})";
      ci.insertTextFormat = InsertTextFormat::Snippet;
    }
    items.push_back(std::move(ci));
  }
}

void CompletionProvider::addKeywords(std::vector<CompletionItem> &items,
                                     const std::string &prefix) {
  static const char *keywords[] = {
      "if",     "else",   "for",       "while",  "do",       "loop",
      "switch", "case",   "default",   "break",  "continue", "ret",
      "fn",     "struct", "enum",      "union",  "extend",   "type",
      "extern", "use",    "namespace", "try",    "catch",    "finally",
       "throw",  "spawn",  "await",     "lock",   "defer",    "as",
       "match",  "or",     "is",     "in",     "sizeof",    "typeof", "true",     "false",
       "null",   "return", "asm",       "volatile",
       "goto",   "intel"};

  for (auto *kw : keywords) {
    if (!matchesPrefix(kw, prefix))
      continue;
    CompletionItem ci;
    ci.label = kw;
    ci.kind = CompletionKind::Keyword;
    items.push_back(std::move(ci));
  }

  // #include directive (matches on "include" or "#include")
  bool matchHash = !prefix.empty() && prefix[0] == '#' &&
                   matchesPrefix("include", prefix.substr(1));
  if (matchHash || matchesPrefix("include", prefix)) {
    CompletionItem ci;
    ci.label = "#include";
    ci.kind = CompletionKind::Snippet;
    ci.detail = "C header include directive";
    ci.insertText = "#include <${1:header}>";
    ci.insertTextFormat = InsertTextFormat::Snippet;
    ci.filterText = "#include";
    items.push_back(std::move(ci));
  }

  // #inline block
  bool matchInline = (!prefix.empty() && prefix[0] == '#' &&
                      matchesPrefix("inline", prefix.substr(1))) ||
                     matchesPrefix("#inline", prefix);
  if (matchInline) {
    CompletionItem ci;
    ci.label = "#inline";
    ci.kind = CompletionKind::Snippet;
    ci.detail = "Inline scope block (injects into parent scope)";
    ci.insertText = "#inline {\n\t$0\n}";
    ci.insertTextFormat = InsertTextFormat::Snippet;
    ci.filterText = "#inline";
    items.push_back(std::move(ci));
  }

  // #scope block
  bool matchScope = (!prefix.empty() && prefix[0] == '#' &&
                     matchesPrefix("scope", prefix.substr(1))) ||
                    matchesPrefix("#scope", prefix);
  if (matchScope) {
    CompletionItem ci;
    ci.label = "#scope";
    ci.kind = CompletionKind::Snippet;
    ci.detail = "RAII scope block — callbacks run on exit (LIFO)";
    ci.insertText = "#scope (${1:fn()}) {\n\t$0\n}";
    ci.insertTextFormat = InsertTextFormat::Snippet;
    ci.filterText = "#scope";
    items.push_back(std::move(ci));
  }
}

void CompletionProvider::addGlobalBuiltins(std::vector<CompletionItem> &items,
                                           const std::string &prefix) {
  // Global builtins always available without import
  static const struct {
    const char *name;
    const char *params; // display params
    const char *retType;
    const char *snippet; // nullptr = name + "()"
    const char *doc;
  } globals[] = {
      {"assert", "(bool condition)", "void", "assert(${1:condition})",
       "Aborts if condition is false."},
      {"assertMsg", "(bool condition, string msg)", "void",
       "assertMsg(${1:condition}, ${2:msg})",
       "Aborts with message if condition is false."},
      {"panic", "(string msg)", "void", "panic(${1:msg})",
       "Immediately aborts execution with a message."},
      {"unreachable", "()", "void", nullptr,
       "Marks unreachable code. Aborts if reached."},
      {"exit", "(int32 code)", "void", "exit(${1:code})",
       "Exits the process with the given status code."},
      {"toInt", "(string s)", "int64", "toInt(${1:s})",
       "Parses a string into an int64."},
      {"toFloat", "(string s)", "float64", "toFloat(${1:s})",
       "Parses a string into a float64."},
      {"toBool", "(string s)", "bool", "toBool(${1:s})",
       "Parses a string into a bool."},
      {"toString", "(T value)", "string", "toString(${1:value})",
       "Converts any primitive value to string."},
      {"cstr", "(string s)", "*char", "cstr(${1:s})",
       "Converts a Lucis string to a null-terminated C string."},
      {"fromCStr", "(*char ptr)", "string", "fromCStr(${1:ptr})",
       "Converts a null-terminated C string to a Lucis string."},
      {"fromCStrCopy", "(*char ptr)", "string", "fromCStrCopy(${1:ptr})",
       "Copies a null-terminated C string into owned Lucis string memory."},
      {"fromCStrLen", "(*char ptr, usize len)", "string",
       "fromCStrLen(${1:ptr}, ${2:len})",
       "Converts a C string with explicit length to a Lucis string."},
      {"freeStr", "(string s)", "void", "freeStr(${1:s})",
       "Frees memory of a string allocated by fromCStrCopy. Only call on owned "
       "strings."},
  };

  for (auto &g : globals) {
    if (!matchesPrefix(g.name, prefix))
      continue;
    CompletionItem ci;
    ci.label = g.name;
    ci.kind = CompletionKind::Function;
    ci.detail = std::string(g.params) + " -> " + g.retType;
    ci.documentation = "```lucis\nfn " + std::string(g.name) + g.params + " -> " +
                       g.retType + "\n```\n\n" + g.doc + "\n\n*Global builtin*";
    if (g.snippet) {
      ci.insertText = g.snippet;
      ci.insertTextFormat = InsertTextFormat::Snippet;
    } else {
      ci.insertText = std::string(g.name) + "()";
    }
    ci.sortText = "1_" + std::string(g.name); // prioritize over keywords
    items.push_back(std::move(ci));
  }
}

void CompletionProvider::addIntrinsicRoot(std::vector<CompletionItem> &items,
                                          const std::string &prefix) {
  if (!matchesPrefix("lucis", prefix)) return;
  CompletionItem ci;
  ci.label = "lucis";
  ci.kind = CompletionKind::Module;
  ci.detail = "intrinsic root";
  ci.documentation = "```lucis\nmodule lucis\n```\n\nIntrinsic namespace root. "
                     "Contains low-level compiler builtin functions.\n\n"
                     "*Always available without `use`*";
  ci.insertText = "lucis::";
  ci.sortText = "0_lucis";
  items.push_back(std::move(ci));
}

void CompletionProvider::addIntrinsics(std::vector<CompletionItem> &items,
                                       const std::string &scopeName,
                                       const std::string &prefix) {
  // Parse scopeName to extract potential intrinsic namespace
  // scopeName can be "lucis" (list namespaces) or "lucis::core" (list functions)
  std::string intrinsicNs;
  if (scopeName == "lucis") {
    // List intrinsic namespaces
    for (auto &ns : intrinsicRegistry_.allNamespaces()) {
      if (!matchesPrefix(ns, prefix)) continue;
      CompletionItem ci;
      ci.label = ns;
      ci.kind = CompletionKind::Module;
      ci.detail = "intrinsic namespace";
      ci.documentation = "Intrinsics namespace. Contains low-level compiler "
                         "builtin functions.";
      ci.insertText = ns + "::";
      ci.sortText = "0_" + ns;
      items.push_back(std::move(ci));
    }
    return;
  }

  // Extract namespace from "lucis::core" → "core"
  if (scopeName.rfind("lucis::", 0) == 0)
    intrinsicNs = scopeName.substr(7); // 7 = len("lucis::")

  if (!intrinsicNs.empty() && intrinsicRegistry_.hasNamespace(intrinsicNs)) {
    // List intrinsic functions in the namespace
    for (auto *func : intrinsicRegistry_.functionsInNamespace(intrinsicNs)) {
      if (!matchesPrefix(func->name, prefix)) continue;

      std::string params;
      for (size_t i = 0; i < func->params.size(); i++) {
        if (i > 0) params += ", ";
        params += func->params[i].type;
      }
      if (func->isVariadic) {
        if (!params.empty()) params += ", ";
        params += "...";
      }

      CompletionItem ci;
      ci.label = func->name;
      ci.kind = CompletionKind::Function;
      ci.detail = "(" + params + ") -> " + func->returnType;
      std::string sigParams;
      for (size_t i = 0; i < func->params.size(); i++) {
        if (i > 0) sigParams += ", ";
        sigParams += func->params[i].type;
      }
      if (func->isVariadic) {
        if (!sigParams.empty()) sigParams += ", ";
        sigParams += "...";
      }
      ci.documentation = "```lucis\nfn " + func->name + "(" + sigParams + ") -> " +
                         func->returnType + "\n```\n\n" + func->description +
                         "\n\n*Intrinsic*";
      ci.insertText = func->name + "()";
      ci.sortText = "0_" + func->name;
      items.push_back(std::move(ci));
    }
  }
}

void CompletionProvider::warmHeaderCache(const std::string& projectRoot) {
  std::vector<CompletionItem> dummy;
  CompletionProvider temp;
  temp.projectRoot_ = projectRoot;
  temp.addHeaderSuggestions(dummy, "", false, "", nullptr);
}

void CompletionProvider::addHeaderSuggestions(
    std::vector<CompletionItem> &items, const std::string &prefix,
    bool omitClosingChar, const std::string &filePath,
    const ProjectContext *project) {

  namespace fs = std::filesystem;

  // ── Collect local header candidates ────────────────────────────────
  // Key: relative path for display; Value: full path (for dedup)
  struct HeaderEntry {
    std::string label;      // relative path used for matching + insert
    std::string detail;     // "local" or "C header" (system)
  };
  std::vector<HeaderEntry> allHeaders;

  // Scan directories for .h files
  auto scanDir = [&](const fs::path &dir, const fs::path &baseForRel) {
    if (dir.empty() || !fs::is_directory(dir)) return;
    std::error_code ec;
    for (auto &entry : fs::recursive_directory_iterator(dir, ec)) {
      if (ec) break;
      auto ext = entry.path().extension().string();
      if (ext != ".h" && ext != ".H") continue;
      auto rel = fs::relative(entry.path(), baseForRel);
      allHeaders.push_back({rel.string(), "local"});
    }
  };

  // 1) Project root (skip hidden dirs and build dirs) — first priority
  if (project && !project->projectRoot().empty()) {
    auto root = fs::path(project->projectRoot());
    std::error_code ec;
    for (auto &entry : fs::directory_iterator(root, ec)) {
      if (ec) break;
      auto name = entry.path().filename().string();
      if (name[0] == '.' || name == "build" || name == ".lucis") continue;
      if (!fs::is_directory(entry.path())) continue;
      scanDir(entry.path(), root);
    }
  }

  // 2) Source file's own directory (fallback)
  if (!filePath.empty()) {
    auto srcDir = fs::path(filePath).parent_path();
    scanDir(srcDir, srcDir);
  }

  // 3) System headers (cached)
  static std::vector<std::string> cachedHeaders;
  static std::vector<std::string> cachedHeadersLower;
  static bool cached = false;
  if (!cached) {
    CHeaderResolver::setHeaderCacheRoot(projectRoot_);
    cachedHeaders = CHeaderResolver::listSystemHeaders();
    cachedHeadersLower.reserve(cachedHeaders.size());
    for (const auto &h : cachedHeaders) {
      std::string lower;
      lower.reserve(h.size());
      for (auto c : h) lower += (char)std::tolower((unsigned char)c);
      cachedHeadersLower.push_back(std::move(lower));
    }
    cached = true;
  }
  for (auto &h : cachedHeaders)
    allHeaders.push_back({h, "C header"});

  // ── No prefix: show all ────────────────────────────────────────────
  if (prefix.empty()) {
    for (auto &e : allHeaders) {
      CompletionItem ci;
      ci.label      = e.label;
      ci.kind       = CompletionKind::File;
      ci.detail     = e.detail;
      ci.insertText = omitClosingChar ? e.label : e.label + ">";
      items.push_back(std::move(ci));
    }
    return;
  }

  // ── Fuzzy match ────────────────────────────────────────────────────
  std::string prefixLower;
  prefixLower.reserve(prefix.size());
  for (auto c : prefix) prefixLower += (char)std::tolower((unsigned char)c);

  struct ScoredMatch {
    size_t index;
    int score;
  };
  std::vector<ScoredMatch> matches;

  for (size_t i = 0; i < allHeaders.size(); ++i) {
    std::string labelLower;
    labelLower.reserve(allHeaders[i].label.size());
    for (auto c : allHeaders[i].label)
      labelLower += (char)std::tolower((unsigned char)c);

    size_t prefixIdx = 0;
    size_t labelIdx = 0;
    int score = 0;
    bool matched = true;

    while (prefixIdx < prefixLower.size()) {
      if (labelIdx >= labelLower.size()) {
        matched = false;
        break;
      }
      if (labelLower[labelIdx] == prefixLower[prefixIdx]) {
        if (labelIdx == 0 || labelLower[labelIdx-1] == '/' ||
            labelLower[labelIdx-1] == '_' || labelLower[labelIdx-1] == '-')
          score += 10;
        else
          score += 1;
        prefixIdx++;
      }
      labelIdx++;
    }

    if (matched) {
      // Bonus for exact prefix match
      if (labelLower.compare(0, prefixLower.size(), prefixLower) == 0)
        score += 50;
      // Local headers boosted over system
      if (allHeaders[i].detail == "local") score += 20;
      score -= (int)(allHeaders[i].label.size() / 2);
      matches.push_back({i, score});
    }
  }

  std::sort(matches.begin(), matches.end(),
            [&](const ScoredMatch &a, const ScoredMatch &b) {
              if (a.score != b.score) return a.score > b.score;
              return allHeaders[a.index].label < allHeaders[b.index].label;
            });

  size_t limit = std::min(matches.size(), (size_t)500);
  for (size_t i = 0; i < limit; ++i) {
    auto &e = allHeaders[matches[i].index];
    CompletionItem ci;
    ci.label      = e.label;
    ci.kind       = CompletionKind::File;
    ci.detail     = e.detail;
    ci.insertText = omitClosingChar ? e.label : e.label + ">";
    items.push_back(std::move(ci));
  }
}

// ═══════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════

std::string CompletionProvider::resolveMethodReturnType(
    const std::string &receiverType, const std::string &methodName,
    LucisParser::ProgramContext *tree, const ProjectContext *project) {

  if (receiverType.empty() || methodName.empty())
    return "";

  // Helper to resolve placeholder return types
  auto resolvePlaceholder = [&](const std::string &retType) -> std::string {
    if (retType == "_self")
      return receiverType;
    if (retType == "_elem") {
      // Array element type
      if (receiverType.size() > 2 && receiverType[0] == '[') {
        auto cb = receiverType.find(']');
        if (cb != std::string::npos && cb + 1 < receiverType.size())
          return receiverType.substr(cb + 1);
      }
      // Extended type element: Vec<int32> → int32
      auto ab = receiverType.find('<');
      if (ab != std::string::npos) {
        auto closeAngle = receiverType.rfind('>');
        if (closeAngle != std::string::npos && closeAngle > ab)
          return receiverType.substr(ab + 1, closeAngle - ab - 1);
      }
      return "";
    }
    if (retType == "_key") {
      // Map<K,V> → K
      auto ab = receiverType.find('<');
      if (ab != std::string::npos) {
        auto inner = receiverType.substr(ab + 1);
        auto comma = inner.find(',');
        if (comma != std::string::npos)
          return inner.substr(0, comma);
      }
      return "";
    }
    if (retType == "_val") {
      // Map<K,V> → V
      auto ab = receiverType.find('<');
      if (ab != std::string::npos) {
        auto inner = receiverType.substr(ab + 1);
        auto comma = inner.find(',');
        auto closeAngle = inner.rfind('>');
        if (comma != std::string::npos && closeAngle != std::string::npos &&
            closeAngle > comma) {
          auto val = inner.substr(comma + 1, closeAngle - comma - 1);
          while (!val.empty() && val[0] == ' ')
            val.erase(0, 1);
          return val;
        }
      }
      return "";
    }
    if (retType == "_vec_key") {
      // map<K,V> → vec<K>
      auto ab = receiverType.find('<');
      if (ab != std::string::npos) {
        auto inner = receiverType.substr(ab + 1);
        auto comma = inner.find(',');
        if (comma != std::string::npos)
          return "vec<" + inner.substr(0, comma) + ">";
      }
      return "";
    }
    if (retType == "_vec_val") {
      // map<K,V> → vec<V>
      auto ab = receiverType.find('<');
      if (ab != std::string::npos) {
        auto inner = receiverType.substr(ab + 1);
        auto comma = inner.find(',');
        auto closeAngle = inner.rfind('>');
        if (comma != std::string::npos && closeAngle != std::string::npos &&
            closeAngle > comma) {
          auto val = inner.substr(comma + 1, closeAngle - comma - 1);
          while (!val.empty() && val[0] == ' ')
            val.erase(0, 1);
          return "vec<" + val + ">";
        }
      }
      return "";
    }
    return retType;
  };

  // 1) Check array methods
  if (receiverType.size() > 2 && receiverType[0] == '[') {
    auto arrayMethods = methodRegistry_.arrayMethods();
    for (auto *md : arrayMethods) {
      if (md->name == methodName)
        return resolvePlaceholder(md->returnType);
    }
  }

  // 2) Check extended type methods (vec<T>, map<K,V>, set<T>)
  auto angleBracket = receiverType.find('<');
  if (angleBracket != std::string::npos) {
    std::string baseName = receiverType.substr(0, angleBracket);
    auto *extDesc = extTypeRegistry_.lookup(normalizeExtBaseName(baseName));
    if (extDesc) {
      for (auto &md : extDesc->methods) {
        if (md.name == methodName)
          return resolvePlaceholder(md.returnType);
      }
    }
  }

  // 3) Check primitive type methods
  auto *typeInfo = typeRegistry_.lookup(receiverType);
  if (typeInfo) {
    auto methods = methodRegistry_.methodsFor(typeInfo->kind);
    for (auto *md : methods) {
      if (md->name == methodName)
        return resolvePlaceholder(md->returnType);
    }
  }

  std::string receiverLookup = receiverType;
  std::unordered_map<std::string, std::string> subst;
  std::string rBase;
  std::vector<std::string> rArgs;
  if (parseGenericInstance(receiverType, rBase, rArgs))
    receiverLookup = rBase;

  // 4) Check extend methods defined in the same file
  if (tree) {
    for (auto *tld : tree->topLevelDecl()) {
      auto *ext = tld->extendDecl();
      if (!ext || !ext->IDENTIFIER())
        continue;
      std::string extTypeName = safeText(ext->IDENTIFIER());
      if (extTypeName != receiverLookup)
        continue;

      if (!rArgs.empty() && ext->typeParamList()) {
        auto tps = ext->typeParamList()->typeParam();
        for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
          auto ids = tps[i]->IDENTIFIER();
          if (!ids.empty())
            subst[ids[0]->getText()] = rArgs[i];
        }
      }

      for (auto *m : ext->extendMethod()) {
        if (!m->IDENTIFIER(0))
          continue;
        if (safeIdAt(m, 0) == methodName) {
          if (m->typeSpec())
            return substituteTypeParams(safeText(m->typeSpec()), subst);
          return "void";
        }
      }
    }
  }

  // 5) Check extend methods from project (cross-file)
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto syms = project->registry().getModuleSymbols(ns);
      for (auto *sym : syms) {
        if (sym->kind != ExportedSymbol::ExtendBlock)
          continue;
        auto *ext = dynamic_cast<LucisParser::ExtendDeclContext *>(sym->decl);
        if (!ext || !ext->IDENTIFIER())
          continue;
        if (safeText(ext->IDENTIFIER()) != receiverLookup)
          continue;

        if (!rArgs.empty() && ext->typeParamList()) {
          auto tps = ext->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = rArgs[i];
          }
        }

        for (auto *m : ext->extendMethod()) {
          if (!m->IDENTIFIER(0))
            continue;
          if (safeIdAt(m, 0) == methodName) {
            if (m->typeSpec())
              return substituteTypeParams(safeText(m->typeSpec()), subst);
            return "void";
          }
        }
      }
    }
  }

  return "";
}

std::string CompletionProvider::resolveFieldType(
    const std::string &receiverType, const std::string &fieldName,
    LucisParser::ProgramContext *tree, const CBindings *bindings,
    const ProjectContext *project) {

  if (receiverType.empty() || fieldName.empty())
    return "";

  std::string receiverLookup = receiverType;
  std::unordered_map<std::string, std::string> subst;
  std::string rBase;
  std::vector<std::string> rArgs;
  if (parseGenericInstance(receiverType, rBase, rArgs))
    receiverLookup = rBase;

  // 1) Same-file user struct/union
  if (tree) {
    for (auto *tld : tree->topLevelDecl()) {
      if (auto *sd = tld->structDecl()) {
        if (sd->IDENTIFIER() || safeText(sd->IDENTIFIER()) != receiverLookup)
          continue;
        if (!rArgs.empty() && sd->typeParamList()) {
          auto tps = sd->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = rArgs[i];
          }
        }
        for (auto *field : sd->structField()) {
          if (field->IDENTIFIER() &&
              safeText(field->IDENTIFIER()) == fieldName)
            return substituteTypeParams(typeSpecToString(field->typeSpec()),
                                        subst);
        }
      }
      if (auto *ud = tld->unionDecl()) {
        if (!ud->IDENTIFIER() || safeText(ud->IDENTIFIER()) != receiverLookup)
          continue;
        if (!rArgs.empty() && ud->typeParamList()) {
          auto tps = ud->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = rArgs[i];
          }
        }
        for (auto *field : ud->unionField()) {
          if (field->IDENTIFIER() &&
              safeText(field->IDENTIFIER()) == fieldName)
            return substituteTypeParams(typeSpecToString(field->typeSpec()),
                                        subst);
        }
      }
    }
  }

  // 2) Cross-file user struct/union
  if (project && project->isValid()) {
    for (auto &ns : project->registry().allModules()) {
      auto *sym = project->registry().findSymbol(ns, receiverLookup);
      if (!sym)
        continue;
      if (sym->kind == ExportedSymbol::Struct) {
        auto *decl = dynamic_cast<LucisParser::StructDeclContext *>(sym->decl);
        if (!decl)
          continue;
        if (!rArgs.empty() && decl->typeParamList()) {
          auto tps = decl->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = rArgs[i];
          }
        }
        for (auto *field : decl->structField()) {
          if (field->IDENTIFIER() &&
              safeText(field->IDENTIFIER()) == fieldName)
            return substituteTypeParams(typeSpecToString(field->typeSpec()),
                                        subst);
        }
      }

      if (sym->kind == ExportedSymbol::Union) {
        auto *decl = dynamic_cast<LucisParser::UnionDeclContext *>(sym->decl);
        if (!decl)
          continue;
        if (!rArgs.empty() && decl->typeParamList()) {
          auto tps = decl->typeParamList()->typeParam();
          for (size_t i = 0; i < std::min(rArgs.size(), tps.size()); i++) {
            auto ids = tps[i]->IDENTIFIER();
            if (!ids.empty())
              subst[ids[0]->getText()] = rArgs[i];
          }
        }
        for (auto *field : decl->unionField()) {
          if (field->IDENTIFIER() &&
              safeText(field->IDENTIFIER()) == fieldName)
            return substituteTypeParams(typeSpecToString(field->typeSpec()),
                                        subst);
        }
      }
    }
  }

  // 3) Built-in struct/union from TypeRegistry (e.g. Error)
  if (auto *ti = typeRegistry_.lookup(receiverType)) {
    if (ti->kind == TypeKind::Struct || ti->kind == TypeKind::Union) {
      for (auto &field : ti->fields) {
        if (field.name == fieldName && field.typeInfo)
          return field.typeInfo->name;
      }
    }
  }

  // 4) C struct
  if (bindings) {
    if (auto *cs = bindings->findStruct(receiverType)) {
      for (auto &field : cs->fields) {
        if (field.name == fieldName && field.typeInfo)
          return field.typeInfo->name;
      }
    }
  }

  return "";
}

std::unordered_map<std::string, CompletionProvider::LocalVar>
CompletionProvider::collectLocals(LucisParser::FunctionDeclContext *func,
                                  size_t beforeLine,
                                  LucisParser::ProgramContext *tree,
                                  const CBindings *bindings,
                                  const ProjectContext *project) {
  std::unordered_map<std::string, LocalVar> result;

  FuncLookupCtx flc;
  flc.tree = tree;
  flc.bindings = bindings;
  flc.builtinReg = &builtinRegistry_;
        flc.intrinsicReg = &intrinsicRegistry_;
  flc.extTypeReg = &extTypeRegistry_;
  flc.methodReg = &methodRegistry_;
  flc.project = project;

  if (auto *params = func->paramList()) {
    for (auto *p : params->param()) {
      result[safeText(p->IDENTIFIER())] = {safeText(p->typeSpec()), 0};
    }
  }

  collectLocalsFromBlock(func->block(), beforeLine, result, &flc);
  return result;
}

std::unordered_map<std::string, CompletionProvider::LocalVar>
CompletionProvider::collectLocalsFromMethod(
    LucisParser::ExtendMethodContext *method, size_t beforeLine) {
  std::unordered_map<std::string, LocalVar> result;

  bool isInstance = (method->AMPERSAND() != nullptr);
  if (isInstance) {
    // &self param
    result[safeIdAt(method, 1)] = {"&self", 0};
    // Extra params
    for (auto *p : method->param()) {
      result[safeText(p->IDENTIFIER())] = {safeText(p->typeSpec()), 0};
    }
  } else {
    if (auto *pl = method->paramList()) {
      for (auto *p : pl->param()) {
        result[safeText(p->IDENTIFIER())] = {safeText(p->typeSpec()), 0};
      }
    }
  }

  collectLocalsFromBlock(method->block(), beforeLine, result);
  return result;
}

LucisParser::FunctionDeclContext *
CompletionProvider::findEnclosingFunction(LucisParser::ProgramContext *tree,
                                          size_t line) {
  size_t tokenLine = line + 1; // 0-based → 1-based
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *func = tld->functionDecl()) {
      auto *start = func->getStart();
      auto *stop = func->getStop();
      if (start && stop && tokenLine >= start->getLine() &&
          tokenLine <= stop->getLine())
        return func;
    }
  }
  return nullptr;
}

LucisParser::ExtendMethodContext *
CompletionProvider::findEnclosingMethod(LucisParser::ProgramContext *tree,
                                        size_t line) {
  size_t tokenLine = line + 1;
  for (auto *tld : tree->topLevelDecl()) {
    auto *ext = tld->extendDecl();
    if (!ext)
      continue;
    for (auto *method : ext->extendMethod()) {
      auto *start = method->getStart();
      auto *stop = method->getStop();
      if (start && stop && tokenLine >= start->getLine() &&
          tokenLine <= stop->getLine())
        return method;
    }
  }
  return nullptr;
}

LucisParser::StructDeclContext *
CompletionProvider::findStructDecl(LucisParser::ProgramContext *tree,
                                   const std::string &name) {
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *sd = tld->structDecl()) {
      if (safeText(sd->IDENTIFIER()) == name)
        return sd;
    }
  }
  return nullptr;
}

LucisParser::UnionDeclContext *
CompletionProvider::findUnionDecl(LucisParser::ProgramContext *tree,
                                  const std::string &name) {
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *ud = tld->unionDecl()) {
      if (safeText(ud->IDENTIFIER()) == name)
        return ud;
    }
  }
  return nullptr;
}

LucisParser::EnumDeclContext *
CompletionProvider::findEnumDecl(LucisParser::ProgramContext *tree,
                                 const std::string &name) {
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *ed = tld->enumDecl()) {
      if (ed->IDENTIFIER() && safeText(ed->IDENTIFIER()) == name)
        return ed;
    }
  }
  return nullptr;
}

LucisParser::ExtendDeclContext *
CompletionProvider::findExtendDecl(LucisParser::ProgramContext *tree,
                                   const std::string &name) {
  for (auto *tld : tree->topLevelDecl()) {
    if (auto *ext = tld->extendDecl()) {
      if (safeText(ext->IDENTIFIER()) == name)
        return ext;
    }
  }
  return nullptr;
}

std::string CompletionProvider::inferVarType(const std::string &varName,
                                             LucisParser::ProgramContext *tree,
                                             size_t cursorLine,
                                             const CBindings *bindings,
                                             const ProjectContext *project) {
  // Check function locals
  auto *func = findEnclosingFunction(tree, cursorLine);
  if (func) {
    auto locals = collectLocals(func, cursorLine, tree, bindings, project);
    auto it = locals.find(varName);
    if (it != locals.end())
      return it->second.typeName;
  }

  // Check extend method locals
  auto *method = findEnclosingMethod(tree, cursorLine);
  if (method) {
    auto locals = collectLocalsFromMethod(method, cursorLine);
    auto it = locals.find(varName);
    if (it != locals.end()) {
      // In instance methods, self is tracked as "&self" placeholder.
      // Resolve it to the concrete owner struct name so dot/arrow
      // completion can list instance fields/methods.
      if (it->second.typeName == "&self") {
        for (auto *tld : tree->topLevelDecl()) {
          auto *ext = tld->extendDecl();
          if (!ext || !ext->IDENTIFIER())
            continue;
          for (auto *m : ext->extendMethod()) {
            if (m == method)
              return safeText(ext->IDENTIFIER());
          }
        }
      }
      return it->second.typeName;
    }
  }

  return "";
}

std::string
CompletionProvider::formatFuncSignature(LucisParser::FunctionDeclContext *func) {
  std::string sig = "fn " + safeIdAt(func, 0) + "(";
  if (auto *params = func->paramList()) {
    bool first = true;
    for (auto *p : params->param()) {
      if (!first)
        sig += ", ";
      first = false;
      sig += typeSpecToString(p->typeSpec());
      if (p->SPREAD())
        sig += " ...";
      else
        sig += " ";
      sig += safeText(p->IDENTIFIER());
    }
  }
  sig += ") " + typeSpecToString(func->typeSpec());
  return sig;
}

std::string CompletionProvider::formatMethodSignature(
    LucisParser::ExtendMethodContext *method) {
  std::string sig = "fn " + safeIdAt(method, 0) + "(";
  bool isInstance = (method->AMPERSAND() != nullptr);
  if (isInstance) {
    sig += "&" + safeIdAt(method, 1);
    bool first = true;
    for (auto *p : method->param()) {
      sig += ", ";
      sig += typeSpecToString(p->typeSpec()) + " " + safeText(p->IDENTIFIER());
    }
  } else {
    if (auto *pl = method->paramList()) {
      bool first = true;
      for (auto *p : pl->param()) {
        if (!first)
          sig += ", ";
        first = false;
        sig +=
            typeSpecToString(p->typeSpec()) + " " + safeText(p->IDENTIFIER());
      }
    }
  }
  sig += ") " + typeSpecToString(method->typeSpec());
  return sig;
}

std::string
CompletionProvider::typeSpecToString(LucisParser::TypeSpecContext *ts) {
  if (!ts)
    return "?";
  return ts->getText();
}

bool CompletionProvider::matchesPrefix(const std::string &name,
                                       const std::string &prefix) {
  if (prefix.empty())
    return true;
  if (name.size() < prefix.size())
    return false;
  for (size_t i = 0; i < prefix.size(); i++) {
    if (std::tolower(name[i]) != std::tolower(prefix[i]))
      return false;
  }
  return true;
}

void CompletionProvider::dedup(std::vector<CompletionItem> &items) {
  std::unordered_set<std::string> seen;
  auto it = std::remove_if(items.begin(), items.end(),
                           [&seen](const CompletionItem &item) {
                             return !seen.insert(item.label).second;
                           });
  items.erase(it, items.end());
}

std::string
CompletionProvider::buildIncludeFingerprint(LucisParser::ProgramContext *tree) {
  if (!tree)
    return {};
  std::string out;
  for (auto *pre : tree->preambleDecl()) {
    auto *inc = pre->includeDecl();
    if (!inc)
      continue;
    out += inc->getText();
    out.push_back('\n');
  }
  return out;
}

// ═══════════════════════════════════════════════════════════════════════
//  Doc-tag completions (inside /** ... */ blocks)
// ═══════════════════════════════════════════════════════════════════════

void CompletionProvider::addDocTagCompletions(
    std::vector<CompletionItem> &items, const std::string &prefix) {
  struct DocTagDef {
    const char *name;
    const char *detail;
    const char *snippet; // nullptr = plain text insert
    const char *documentation;
  };

  static const DocTagDef tags[] = {
      // name + description
      {"@param", "name description", "param ${1:name} ${2:description}",
       "Document a function parameter."},

      {"@property", "name description", "property ${1:name} ${2:description}",
       "Document a property."},

      {"@field", "name description", "field ${1:name} ${2:description}",
       "Document a struct field."},

      // description
      {"@return", "description", "return ${1:description}",
       "Describe the return value."},

      {"@returns", "description", "returns ${1:description}",
       "Describe what the function returns."},

      {"@brief", "description", "brief ${1:description}",
       "Short summary of the documented element."},

      {"@deprecated", "[reason]", "deprecated ${1:reason}",
       "Mark as deprecated with optional reason."},

      {"@version", "semver", "version ${1:0.0.1}", "Specify the version."},

      {"@author", "name", "author ${1:name}", "Specify the author."},

      {"@see", "reference", "see ${1:reference}",
       "Add a cross-reference to related documentation."},

      {"@since", "version", "since ${1:version}",
       "Specify when this was introduced."},

      {"@throws", "description", "throws ${1:description}",
       "Document an error or exception condition."},

      {"@todo", "description", "todo ${1:description}", "Add a to-do note."},

      // multi-line block
      {"@example", "code example", "example\n * ${1:code}",
       "Provide a usage example (code block until next tag)."},

      {"@remarks", "additional notes", "remarks ${1:text}",
       "Add additional remarks or detailed explanation."},

      {"@note", "note text", "note ${1:text}", "Add an informational note."},

      {"@warning", "warning text", "warning ${1:text}",
       "Add a warning about potential issues."},

      // flags (no arguments)
      {"@private", nullptr, "private", "Mark as private visibility."},

      {"@public", nullptr, "public", "Mark as public visibility."},

      {"@protected", nullptr, "protected", "Mark as protected visibility."},

      {"@internal", nullptr, "internal",
       "Mark as internal (not part of public API)."},

      {"@struct", nullptr, "struct", "Mark documentation as struct-related."},

      {"@namespace", nullptr, "namespace",
       "Mark documentation as namespace-related."},
  };

  for (auto &tag : tags) {
    std::string label = tag.name;
    // Match against prefix (without the '@')
    std::string tagNameOnly = label.substr(1); // remove '@'
    if (!prefix.empty() && !matchesPrefix(tagNameOnly, prefix))
      continue;

    CompletionItem item;
    item.label = label;
    item.kind = CompletionKind::Keyword;
    item.filterText = tagNameOnly; // filter without '@'
    if (tag.detail)
      item.detail = tag.detail;
    if (tag.documentation)
      item.documentation = tag.documentation;

    if (tag.snippet) {
      item.insertText = tag.snippet;
      // Only use snippet format if it contains tab stops
      if (std::string(tag.snippet).find("${") != std::string::npos)
        item.insertTextFormat = InsertTextFormat::Snippet;
    } else {
      item.insertText = tagNameOnly;
    }

    // Sort tags so param/returns appear first
    if (tagNameOnly == "param")
      item.sortText = "00_param";
    else if (tagNameOnly == "returns")
      item.sortText = "01_returns";
    else if (tagNameOnly == "return")
      item.sortText = "02_return";
    else if (tagNameOnly == "example")
      item.sortText = "03_example";
    else if (tagNameOnly == "throws")
      item.sortText = "04_throws";
    else
      item.sortText = "10_" + tagNameOnly;

    items.push_back(std::move(item));
  }
}
