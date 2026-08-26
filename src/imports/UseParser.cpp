#include "imports/UseParser.h"

namespace {

// True when the token immediately before `tok` (in the token stream) is
// an AS token of `group` — i.e. `tok` is the alias of the preceding symbol.
bool isAliasedToken(LucisParser::UseGroupContext* group, antlr4::Token* tok) {
    if (!tok) return false;
    auto idx = tok->getTokenIndex();
    for (auto* as : group->AS()) {
        if (as && as->getSymbol() &&
            as->getSymbol()->getTokenIndex() == idx - 1)
            return true;
    }
    return false;
}

}  // namespace

std::vector<UseBindingWithTokens> parseUseDeclWithTokens(
    LucisParser::UseDeclContext* useDecl) {
    std::vector<UseBindingWithTokens> out;
    if (!useDecl) return out;

    auto attach = [&](UseBinding&& b, antlr4::Token* exportedTok,
                      antlr4::Token* localTok) {
        UseBindingWithTokens e;
        e.binding = std::move(b);
        e.exportedToken = exportedTok;
        e.localToken = localTok ? localTok : exportedTok;
        out.push_back(std::move(e));
    };

    if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) {
        UseBinding b;
        b.kind = UseBinding::Kind::Module;
        b.modulePath = root->IDENTIFIER()->getText();
        b.localName = lastPathSegment(b.modulePath);
        b.ctx = root;
        antlr4::Token* tok = root->IDENTIFIER() ? root->IDENTIFIER()->getSymbol() : nullptr;
        attach(std::move(b), tok, tok);
        return out;
    }

    if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) {
        auto bindings = parseUseDecl(item);
        auto ids = item->IDENTIFIER();
        if (bindings.empty() || ids.empty()) return out;
        attach(std::move(bindings[0]), ids[0]->getSymbol(),
               ids.size() > 1 ? ids[1]->getSymbol() : nullptr);
        return out;
    }

    if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) {
        auto bindings = parseUseDecl(group);
        auto ids = group->IDENTIFIER();
        size_t idIdx = 0;
        for (auto& b : bindings) {
            if (idIdx >= ids.size()) break;
            antlr4::Token* exportedTok = ids[idIdx]->getSymbol();
            antlr4::Token* localTok = nullptr;
            if (b.localName != b.exportedName && idIdx + 1 < ids.size())
                localTok = ids[idIdx + 1]->getSymbol();
            attach(UseBinding{b}, exportedTok, localTok);
            idIdx += (b.localName != b.exportedName) ? 2 : 1;
        }
        return out;
    }

    if (auto* wildcard = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl)) {
        UseBinding b;
        b.kind = UseBinding::Kind::EnumWildcard;
        b.ctx = wildcard;
        attach(std::move(b),
               wildcard->typeSpec() && wildcard->typeSpec()->IDENTIFIER()
                   ? wildcard->typeSpec()->IDENTIFIER()->getSymbol()
                   : nullptr,
               nullptr);
        return out;
    }

    return out;
}

std::string lastPathSegment(const std::string& modulePath) {
    auto pos = modulePath.rfind("::");
    if (pos == std::string::npos)
        return modulePath;
    return modulePath.substr(pos + 2);
}

ImportAliasMap buildImportAliasMap(LucisParser::ProgramContext* tree) {
    ImportAliasMap out;
    if (!tree) return out;
    auto collect = [&](LucisParser::UseDeclContext* useDecl) {
        for (auto& b : parseUseDecl(useDecl)) {
            if (b.kind == UseBinding::Kind::Symbol) {
                out.symbols[b.localName] = {b.modulePath, b.exportedName};
            } else if (b.kind == UseBinding::Kind::Module) {
                out.modules[b.localName] = b.modulePath;
            }
        }
    };
    for (auto* pre : tree->preambleDecl())
        if (auto* ud = pre->useDecl()) collect(ud);
    for (auto* tld : tree->topLevelDecl())
        if (auto* ud = tld->useDecl()) collect(ud);
    return out;
}

std::vector<UseBinding> parseUseDecl(LucisParser::UseDeclContext* useDecl) {
    std::vector<UseBinding> out;
    if (!useDecl) return out;

    // use module;  → module root import
    if (auto* root = dynamic_cast<LucisParser::UseRootContext*>(useDecl)) {
        UseBinding b;
        b.kind = UseBinding::Kind::Module;
        b.modulePath = root->IDENTIFIER()->getText();
        b.localName = lastPathSegment(b.modulePath);
        b.ctx = root;
        out.push_back(std::move(b));
        return out;
    }

    // use module::func [as local];
    if (auto* item = dynamic_cast<LucisParser::UseItemContext*>(useDecl)) {
        auto ids = item->IDENTIFIER();
        if (ids.empty()) return out;
        UseBinding b;
        b.kind = UseBinding::Kind::Symbol;
        for (auto* id : item->modulePath()->IDENTIFIER()) {
            if (!b.modulePath.empty()) b.modulePath += "::";
            b.modulePath += id->getText();
        }
        b.exportedName = ids[0]->getText();
        b.localName = ids.size() > 1 ? ids[1]->getText() : b.exportedName;
        b.ctx = item;
        out.push_back(std::move(b));
        return out;
    }

    // use module::{ a, b as c, ... };
    if (auto* group = dynamic_cast<LucisParser::UseGroupContext*>(useDecl)) {
        std::string path;
        for (auto* id : group->modulePath()->IDENTIFIER()) {
            if (!path.empty()) path += "::";
            path += id->getText();
        }
        auto ids = group->IDENTIFIER();
        for (size_t i = 0; i < ids.size(); ++i) {
            UseBinding b;
            b.kind = UseBinding::Kind::Symbol;
            b.modulePath = path;
            b.exportedName = ids[i]->getText();
            // The next ID is the alias only when an AS token separates them.
            if (i + 1 < ids.size() && isAliasedToken(group, ids[i + 1]->getSymbol())) {
                b.localName = ids[i + 1]->getText();
                ++i;
            } else {
                b.localName = b.exportedName;
            }
            b.ctx = group;
            out.push_back(std::move(b));
        }
        return out;
    }

    // use Type::*;  → the checker resolves the type via typeSpec()
    if (auto* wildcard = dynamic_cast<LucisParser::UseEnumWildcardContext*>(useDecl)) {
        UseBinding b;
        b.kind = UseBinding::Kind::EnumWildcard;
        b.ctx = wildcard;
        out.push_back(std::move(b));
        return out;
    }

    return out;
}