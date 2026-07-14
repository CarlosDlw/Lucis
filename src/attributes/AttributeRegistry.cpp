#include "attributes/AttributeRegistry.h"

void AttributeRegistry::registerAttribute(const std::string& name, AttributeHandler handler) {
    handlers_[name] = std::move(handler);
}

const AttributeHandler* AttributeRegistry::lookup(const std::string& name) const {
    auto it = handlers_.find(name);
    return it != handlers_.end() ? &it->second : nullptr;
}

bool AttributeRegistry::isKnown(const std::string& name) const {
    return handlers_.count(name) > 0;
}

std::vector<std::string> AttributeRegistry::allNames() const {
    std::vector<std::string> names;
    names.reserve(handlers_.size());
    for (auto& [name, _] : handlers_)
        names.push_back(name);
    return names;
}

void registerBuiltinAttributes(AttributeRegistry& reg) {
    // ── error: marks an enum variant as the error discriminant ──────────
    reg.registerAttribute("error", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── deprecated: marks a declaration as deprecated ─────────────────
    reg.registerAttribute("deprecated", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            if (attr.args.size() > 2) return false;
            for (auto& a : attr.args) {
                if (a.kind != AttributeArg::String) return false;
            }
            return true;
        }
    });

    // ── repr: struct/union layout ─────────────────────────────────────
    reg.registerAttribute("repr", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            if (attr.args.empty()) return false;
            for (auto& a : attr.args) {
                if (a.kind != AttributeArg::Ident) return false;
                if (a.identValue != "C" && a.identValue != "packed" && a.identValue != "transparent")
                    return false;
            }
            return true;
        }
    });

    // ── no_mangle: preserve symbol name (no name mangling) ────────────
    reg.registerAttribute("no_mangle", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── export: force external linkage ────────────────────────────────
    reg.registerAttribute("export", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── link_section("..."): place in a specific ELF section ──────────
    reg.registerAttribute("link_section", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            return attr.args.size() == 1 && attr.args[0].kind == AttributeArg::String;
        }
    });

    // ── must_use: warn if return value is discarded ───────────────────
    reg.registerAttribute("must_use", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── noreturn: function never returns ──────────────────────────────
    reg.registerAttribute("noreturn", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── non_exhaustive: enum may gain variants in the future ──────────
    reg.registerAttribute("non_exhaustive", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── inline(always | never): function inlining hint ────────────────
    reg.registerAttribute("inline", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            if (attr.args.size() > 1) return false;
            for (auto& a : attr.args) {
                if (a.kind != AttributeArg::Ident) return false;
                if (a.identValue != "always" && a.identValue != "never")
                    return false;
            }
            return true;
        }
    });

    // ── cold: function is rarely executed ─────────────────────────────
    reg.registerAttribute("cold", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── hot: function is frequently executed ──────────────────────────
    reg.registerAttribute("hot", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── allow(..): suppress specific lint warnings ────────────────────
    reg.registerAttribute("allow", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            return !attr.args.empty();
        }
    });

    // ── deny(..): deny specific lint warnings ─────────────────────────
    reg.registerAttribute("deny", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            return !attr.args.empty();
        }
    });

    // ── doc("..."): documentation string ──────────────────────────────
    reg.registerAttribute("doc", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            return attr.args.size() == 1 && attr.args[0].kind == AttributeArg::String;
        }
    });

    // ── thread_local: global is thread-local storage ──────────────────
    reg.registerAttribute("thread_local", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── used: prevent linker from removing the symbol ─────────────────
    reg.registerAttribute("used", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) {
            return attr.args.empty();
        }
    });

    // ── optimize(speed | size): optimization hint ─────────────────────
    reg.registerAttribute("optimize", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            if (attr.args.size() != 1) return false;
            if (attr.args[0].kind != AttributeArg::Ident) return false;
            return attr.args[0].identValue == "speed" || attr.args[0].identValue == "size";
        }
    });

    // ── align(n): minimum alignment for struct/global ─────────────────
    reg.registerAttribute("align", AttributeHandler{
        .validate = [](const Attribute& attr, const TypeInfo*, std::vector<std::string>&) -> bool {
            if (attr.args.size() != 1) return false;
            if (attr.args[0].kind != AttributeArg::Int) return false;
            int64_t n = attr.args[0].intValue;
            return n > 0 && (n & (n - 1)) == 0; // power of 2
        }
    });
}
