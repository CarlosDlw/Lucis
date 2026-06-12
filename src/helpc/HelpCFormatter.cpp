#include "helpc/HelpCFormatter.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

// ── Constructor ─────────────────────────────────────────────────────────────

HelpCFormatter::HelpCFormatter(const HelpCOptions& opts) : opts_(opts) {}

// ── ANSI color helpers ──────────────────────────────────────────────────────

std::string HelpCFormatter::bold(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[1m" + s + "\033[0m";
}

std::string HelpCFormatter::dim(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[2m" + s + "\033[0m";
}

std::string HelpCFormatter::cyan(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[36m" + s + "\033[0m";
}

std::string HelpCFormatter::green(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[32m" + s + "\033[0m";
}

std::string HelpCFormatter::yellow(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[33m" + s + "\033[0m";
}

std::string HelpCFormatter::magenta(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[35m" + s + "\033[0m";
}

std::string HelpCFormatter::red(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[31m" + s + "\033[0m";
}

std::string HelpCFormatter::blue(const std::string& s) const {
    if (opts_.noColor) return s;
    return "\033[34m" + s + "\033[0m";
}

std::string HelpCFormatter::reset() const {
    if (opts_.noColor) return "";
    return "\033[0m";
}

// ── Box drawing ─────────────────────────────────────────────────────────────

void HelpCFormatter::printBoxTop(const std::string& label, int width) const {
    std::cout << "╭─ " << bold(label) << " ";
    int remaining = width - 4 - static_cast<int>(label.size());
    for (int i = 0; i < remaining; i++) std::cout << "─";
    std::cout << "╮\n";
}

void HelpCFormatter::printBoxLine(const std::string& content, int width) const {
    // Note: width here is character count. We print without strict padding
    // since ANSI codes make width calculation complex.
    std::cout << "│ " << content << "\n";
}

void HelpCFormatter::printBoxBottom(int width) const {
    std::cout << "╰";
    for (int i = 0; i < width - 1; i++) std::cout << "─";
    std::cout << "╯\n";
}

void HelpCFormatter::printBoxEmpty(int width) const {
    std::cout << "│\n";
}

// ── Utilities ───────────────────────────────────────────────────────────────

std::string HelpCFormatter::padRight(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

std::string HelpCFormatter::formatHex(int64_t value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
}

std::string HelpCFormatter::functionSignature(const HelpCFunction& fn) const {
    std::string sig = fn.returnCType + " " + fn.name + "(";
    for (size_t i = 0; i < fn.params.size(); i++) {
        if (i > 0) sig += ", ";
        sig += fn.params[i].cType;
        if (!fn.params[i].name.empty())
            sig += " " + fn.params[i].name;
    }
    if (fn.isVariadic) {
        if (!fn.params.empty()) sig += ", ";
        sig += "...";
    }
    sig += ")";
    return sig;
}

// ── Single symbol: Function ─────────────────────────────────────────────────

void HelpCFormatter::printFunction(const HelpCFunction& fn,
                                   const HelpCHeaderInfo& hdr) const {
    printBoxTop("function", 62);

    // Signature
    printBoxLine(cyan(fn.returnLucisType) + " " + bold(fn.name) + "(" +
        [&]() {
            std::string args;
            for (size_t i = 0; i < fn.params.size(); i++) {
                if (i > 0) args += ", ";
                args += cyan(fn.params[i].lucisType) + " " + fn.params[i].name;
            }
            if (fn.isVariadic) {
                if (!fn.params.empty()) args += ", ";
                args += "...";
            }
            return args;
        }() + ")");

    // Doc comment
    if (!fn.doc.empty()) {
        printBoxEmpty();
        printBoxLine(dim(fn.doc));
    }

    // Parameters
    if (!fn.params.empty()) {
        printBoxEmpty();
        printBoxLine(bold("Parameters:"));

        // Calculate column widths
        size_t maxName = 0, maxCType = 0, maxLucisType = 0;
        for (auto& p : fn.params) {
            maxName  = std::max(maxName, p.name.size());
            maxCType = std::max(maxCType, p.cType.size());
            maxLucisType = std::max(maxLucisType, p.lucisType.size());
        }
        maxName  = std::max(maxName, (size_t)4);
        maxCType = std::max(maxCType, (size_t)4);

        for (auto& p : fn.params) {
            printBoxLine("  " + green(padRight(p.name, maxName)) + "  " +
                        dim(padRight(p.cType, maxCType)) + "  " +
                        cyan("(" + p.lucisType + ")"));
        }
        if (fn.isVariadic) {
            printBoxLine("  " + green(padRight("...", maxName)) + "  " +
                        dim("variadic"));
        }
    }

    // Returns
    printBoxEmpty();
    printBoxLine(bold("Returns: ") + cyan(fn.returnLucisType) +
                dim(" (" + fn.returnCType + ")"));

    // Header and link
    printBoxLine(bold("Header:  ") + "<" + hdr.headerName + ">");
    if (!hdr.linkFlag.empty())
        printBoxLine(bold("Link:    ") + hdr.linkFlag);

    // T usage example
    printBoxEmpty();
    printBoxLine(bold("T usage:"));
    {
        std::string call = "  " + fn.name + "(";
        for (size_t i = 0; i < fn.params.size(); i++) {
            if (i > 0) call += ", ";
            // Generate example argument
            if (fn.params[i].lucisType == "*char")
                call += "c\"...\"";
            else if (fn.params[i].lucisType == "int32")
                call += "0";
            else if (fn.params[i].lucisType == "float32" || fn.params[i].lucisType == "float64")
                call += "0.0";
            else if (fn.params[i].lucisType == "bool")
                call += "true";
            else if (fn.params[i].lucisType.find('*') == 0)
                call += "ptr";
            else
                call += fn.params[i].name;
        }
        call += ");";
        printBoxLine(yellow(call));
    }

    printBoxBottom(62);
}

// ── Single symbol: Struct ───────────────────────────────────────────────────

void HelpCFormatter::printStruct(const HelpCStruct& s,
                                 const HelpCHeaderInfo& hdr) const {
    std::string label = s.isUnion ? "union" : "struct";
    printBoxTop(label, 62);

    // Name and size
    {
        std::string info = label + " " + bold(s.name);
        if (s.size >= 0)
            info += dim(" (" + std::to_string(s.size) + " bytes");
        if (s.alignment >= 0 && opts_.verbose)
            info += dim(", align " + std::to_string(s.alignment));
        if (s.size >= 0)
            info += dim(")");
        printBoxLine(info);
    }

    // Doc
    if (!s.doc.empty()) {
        printBoxEmpty();
        printBoxLine(dim(s.doc));
    }

    // Fields
    if (!s.fields.empty()) {
        printBoxEmpty();
        printBoxLine(bold("Fields:"));

        size_t maxName = 0, maxCType = 0, maxLucisType = 0;
        for (auto& f : s.fields) {
            maxName  = std::max(maxName, f.name.size());
            maxCType = std::max(maxCType, f.cType.size());
            maxLucisType = std::max(maxLucisType, f.lucisType.size());
        }
        maxName = std::max(maxName, (size_t)4);

        for (auto& f : s.fields) {
            std::string line = "  " + green(padRight(f.name, maxName)) + "  " +
                              dim(padRight(f.cType, maxCType)) + "  " +
                              cyan("(" + f.lucisType + ")");
            if (opts_.verbose && f.offset >= 0)
                line += dim("  offset " + std::to_string(f.offset));
            if (opts_.verbose && f.size >= 0)
                line += dim("  size " + std::to_string(f.size));
            printBoxLine(line);
        }
    }

    // Header
    printBoxEmpty();
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");
    if (!hdr.linkFlag.empty())
        printBoxLine(bold("Link:   ") + hdr.linkFlag);

    // Related struct macros (constants of this type)
    std::vector<const HelpCStructMacro*> related;
    for (auto& sm : hdr.structMacros) {
        if (sm.structType == s.name)
            related.push_back(&sm);
    }

    if (!related.empty()) {
        printBoxEmpty();
        size_t show = opts_.showAll ? related.size() : std::min(related.size(), (size_t)8);
        printBoxLine(bold("Known constants") +
            dim(" (" + std::to_string(related.size()) + " total):"));
        for (size_t i = 0; i < show; i++) {
            std::string vals = "{ ";
            for (size_t v = 0; v < related[i]->fieldValues.size(); v++) {
                if (v > 0) vals += ", ";
                vals += std::to_string(related[i]->fieldValues[v]);
            }
            vals += " }";
            printBoxLine("  " + yellow(padRight(related[i]->name, 16)) + dim(vals));
        }
        if (show < related.size())
            printBoxLine(dim("  ... (" + std::to_string(related.size() - show) +
                            " more, use --all)"));
    }

    // T usage
    printBoxEmpty();
    printBoxLine(bold("T usage:"));
    {
        std::string usage = "  " + s.name + " x = " + s.name + " { ";
        for (size_t i = 0; i < s.fields.size(); i++) {
            if (i > 0) usage += ", ";
            usage += "0";
        }
        usage += " };";
        printBoxLine(yellow(usage));
    }

    printBoxBottom(62);
}

// ── Single symbol: Enum ─────────────────────────────────────────────────────

void HelpCFormatter::printEnum(const HelpCEnum& e,
                               const HelpCHeaderInfo& hdr) const {
    printBoxTop("enum", 62);

    printBoxLine("enum " + bold(e.name) +
                dim(" (" + std::to_string(e.values.size()) + " values)"));

    if (!e.doc.empty()) {
        printBoxEmpty();
        printBoxLine(dim(e.doc));
    }

    if (!e.values.empty()) {
        printBoxEmpty();

        size_t maxName = 0;
        for (auto& v : e.values)
            maxName = std::max(maxName, v.name.size());
        maxName = std::max(maxName, (size_t)4);

        size_t show = opts_.showAll ? e.values.size()
                                    : std::min(e.values.size(), (size_t)15);
        for (size_t i = 0; i < show; i++) {
            auto& v = e.values[i];
            std::string line = "  " + green(padRight(v.name, maxName)) +
                              " = " + std::to_string(v.value);
            if (opts_.verbose)
                line += dim("  (" + formatHex(v.value) + ")");
            printBoxLine(line);
        }
        if (show < e.values.size())
            printBoxLine(dim("  ... (" + std::to_string(e.values.size() - show) +
                            " more, use --all)"));
    }

    printBoxEmpty();
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");
    if (!hdr.linkFlag.empty())
        printBoxLine(bold("Link:   ") + hdr.linkFlag);

    // T usage
    if (!e.values.empty()) {
        printBoxEmpty();
        printBoxLine(bold("T usage:"));
        printBoxLine(yellow("  int32 val = " + e.values[0].name + ";"));
    }

    printBoxBottom(62);
}

// ── Single symbol: Typedef ──────────────────────────────────────────────────

void HelpCFormatter::printTypedef(const HelpCTypedef& td,
                                  const HelpCHeaderInfo& hdr) const {
    printBoxTop("typedef", 62);

    printBoxLine("typedef " + bold(td.name));

    if (!td.doc.empty()) {
        printBoxEmpty();
        printBoxLine(dim(td.doc));
    }

    printBoxEmpty();
    printBoxLine(bold("Alias for: ") + dim(td.underlyingCType));
    printBoxLine(bold("Lucis type:    ") + cyan(td.underlyingLucisType));

    // If underlying is a struct, try to show its fields
    for (auto& s : hdr.structs) {
        if (s.name == td.underlyingLucisType || s.name == td.name) {
            if (!s.fields.empty()) {
                printBoxEmpty();
                printBoxLine(bold("Resolved fields:"));
                size_t maxName = 0;
                for (auto& f : s.fields)
                    maxName = std::max(maxName, f.name.size());
                for (auto& f : s.fields) {
                    printBoxLine("  " + green(padRight(f.name, maxName)) + "  " +
                                cyan("(" + f.lucisType + ")"));
                }
                if (s.size >= 0)
                    printBoxLine(dim("  Size: " + std::to_string(s.size) + " bytes"));
            }
            break;
        }
    }

    printBoxEmpty();
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");

    printBoxBottom(62);
}

// ── Single symbol: Macro (integer) ──────────────────────────────────────────

void HelpCFormatter::printMacro(const HelpCMacro& m,
                                const HelpCHeaderInfo& hdr) const {
    std::string label = m.isNull ? "macro (null)" : "macro (integer)";
    printBoxTop(label, 62);

    printBoxLine("#define " + bold(m.name) + "  " +
                cyan(std::to_string(m.value)));

    if (!m.isNull) {
        printBoxEmpty();
        printBoxLine(bold("Value: ") + std::to_string(m.value) +
                    dim(" (" + formatHex(m.value) + ")"));
    } else {
        printBoxEmpty();
        printBoxLine(bold("Value: ") + "NULL");
    }

    printBoxEmpty();
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");

    printBoxBottom(62);
}

// ── Single symbol: Struct macro ─────────────────────────────────────────────

void HelpCFormatter::printStructMacro(const HelpCStructMacro& sm,
                                      const HelpCHeaderInfo& hdr) const {
    printBoxTop("macro (struct literal)", 62);

    // Show #define
    {
        std::string vals = "{ ";
        for (size_t i = 0; i < sm.fieldValues.size(); i++) {
            if (i > 0) vals += ", ";
            vals += std::to_string(sm.fieldValues[i]);
        }
        vals += " }";
        printBoxLine("#define " + bold(sm.name) + "  " +
                    cyan(sm.structType) + vals);
    }

    printBoxEmpty();
    printBoxLine(bold("Type: ") + cyan(sm.structType));

    // Show field names + values if we can find the struct
    for (auto& s : hdr.structs) {
        if (s.name == sm.structType && !s.fields.empty()) {
            printBoxEmpty();
            printBoxLine(bold("Fields:"));
            size_t maxName = 0;
            for (auto& f : s.fields)
                maxName = std::max(maxName, f.name.size());
            for (size_t i = 0; i < s.fields.size() && i < sm.fieldValues.size(); i++) {
                printBoxLine("  " + green(padRight(s.fields[i].name, maxName)) +
                            " = " + std::to_string(sm.fieldValues[i]));
            }
            break;
        }
    }

    printBoxEmpty();
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");

    // T usage
    printBoxEmpty();
    printBoxLine(bold("T usage:"));
    printBoxLine(yellow("  ClearBackground(" + sm.name + ");"));
    if (!sm.fieldValues.empty()) {
        // Find first field name
        for (auto& s : hdr.structs) {
            if (s.name == sm.structType && !s.fields.empty()) {
                printBoxLine(yellow("  int32 val = " + sm.name + "." +
                            s.fields[0].name + ";  // " +
                            std::to_string(sm.fieldValues[0])));
                break;
            }
        }
    }

    printBoxBottom(62);
}

// ── Single symbol: Global ───────────────────────────────────────────────────

void HelpCFormatter::printGlobal(const HelpCGlobal& g,
                                 const HelpCHeaderInfo& hdr) const {
    printBoxTop("global", 62);

    printBoxLine(cyan(g.lucisType) + " " + bold(g.name));

    if (!g.doc.empty()) {
        printBoxEmpty();
        printBoxLine(dim(g.doc));
    }

    printBoxEmpty();
    printBoxLine(bold("C type: ") + dim(g.cType));
    printBoxLine(bold("Lucis type: ") + cyan(g.lucisType));
    printBoxLine(bold("Header: ") + "<" + hdr.headerName + ">");

    printBoxBottom(62);
}

// ── Summary ─────────────────────────────────────────────────────────────────

void HelpCFormatter::printSummary(const HelpCHeaderInfo& hdr) const {
    size_t total = hdr.functions.size() + hdr.structs.size() +
                   hdr.enums.size() + hdr.typedefs.size() +
                   hdr.macros.size() + hdr.structMacros.size() +
                   hdr.globals.size();

    std::cout << "\n";
    std::cout << bold("<" + hdr.headerName + ">") << " — "
              << total << " symbols\n\n";

    auto row = [&](const std::string& label, size_t count) {
        if (count == 0) return;
        std::cout << "  " << padRight(label + ":", 16) << count << "\n";
    };

    row("Functions",  hdr.functions.size());
    row("Structs",    hdr.structs.size());
    row("Enums",      hdr.enums.size());
    row("Typedefs",   hdr.typedefs.size());
    row("Macros",     hdr.macros.size() + hdr.structMacros.size());
    row("Globals",    hdr.globals.size());

    if (!hdr.linkFlag.empty())
        std::cout << "\n  Link: " << bold(hdr.linkFlag) << "\n";

    std::cout << "\n";
}

// ── List: Functions ─────────────────────────────────────────────────────────

void HelpCFormatter::printListFunctions(const HelpCHeaderInfo& hdr) const {
    if (hdr.functions.empty()) {
        std::cout << "No functions found in <" << hdr.headerName << ">\n";
        return;
    }

    std::cout << "\n" << bold("Functions") << " in <" << hdr.headerName
              << "> (" << hdr.functions.size() << "):\n\n";

    // Group by common prefix (first word before uppercase/lowercase transition)
    std::map<std::string, std::vector<const HelpCFunction*>> groups;
    for (auto& fn : hdr.functions) {
        std::string prefix;
        for (size_t i = 0; i < fn.name.size(); i++) {
            if (i > 0 && std::isupper(fn.name[i]) && std::islower(fn.name[i - 1])) {
                prefix = fn.name.substr(0, i);
                break;
            }
        }
        if (prefix.empty()) prefix = fn.name;
        groups[prefix].push_back(&fn);
    }

    // If too many groups (fragmented), just list flat
    bool useGroups = groups.size() <= hdr.functions.size() / 2 && groups.size() > 1;

    if (useGroups) {
        for (auto& [prefix, fns] : groups) {
            std::cout << "  " << dim(prefix + "*") << "\n";
            for (auto* fn : fns) {
                std::string sig;
                for (size_t i = 0; i < fn->params.size(); i++) {
                    if (i > 0) sig += ", ";
                    sig += fn->params[i].cType;
                }
                if (fn->isVariadic) {
                    if (!fn->params.empty()) sig += ", ";
                    sig += "...";
                }
                std::cout << "    " << green(padRight(fn->name, 36))
                          << dim("(" + sig + ")") << " → "
                          << cyan(fn->returnLucisType) << "\n";
            }
            std::cout << "\n";
        }
    } else {
        size_t maxName = 0;
        for (auto& fn : hdr.functions)
            maxName = std::max(maxName, fn.name.size());
        maxName = std::min(maxName, (size_t)40);

        for (auto& fn : hdr.functions) {
            std::string sig;
            for (size_t i = 0; i < fn.params.size(); i++) {
                if (i > 0) sig += ", ";
                sig += fn.params[i].cType;
            }
            if (fn.isVariadic) {
                if (!fn.params.empty()) sig += ", ";
                sig += "...";
            }
            std::cout << "  " << green(padRight(fn.name, maxName + 2))
                      << dim("(" + sig + ")") << " → "
                      << cyan(fn.returnLucisType) << "\n";
        }
    }

    std::cout << "\n";
}

// ── List: Structs ───────────────────────────────────────────────────────────

void HelpCFormatter::printListStructs(const HelpCHeaderInfo& hdr) const {
    if (hdr.structs.empty()) {
        std::cout << "No structs found in <" << hdr.headerName << ">\n";
        return;
    }

    std::cout << "\n" << bold("Structs") << " in <" << hdr.headerName
              << "> (" << hdr.structs.size() << "):\n\n";

    size_t maxName = 0;
    for (auto& s : hdr.structs)
        maxName = std::max(maxName, s.name.size());
    maxName = std::min(maxName, (size_t)24);

    for (auto& s : hdr.structs) {
        std::string sizeStr = s.size >= 0 ? std::to_string(s.size) + " bytes" : "?";

        std::string fieldsStr;
        for (size_t i = 0; i < s.fields.size() && i < 4; i++) {
            if (i > 0) fieldsStr += ", ";
            fieldsStr += s.fields[i].name;
        }
        if (s.fields.size() > 4) fieldsStr += ", ...";

        std::cout << "  " << green(padRight(s.name, maxName + 2))
                  << dim(padRight(sizeStr, 12))
                  << cyan(fieldsStr) << "\n";
    }

    std::cout << "\n";
}

// ── List: Enums ─────────────────────────────────────────────────────────────

void HelpCFormatter::printListEnums(const HelpCHeaderInfo& hdr) const {
    if (hdr.enums.empty()) {
        std::cout << "No enums found in <" << hdr.headerName << ">\n";
        return;
    }

    std::cout << "\n" << bold("Enums") << " in <" << hdr.headerName
              << "> (" << hdr.enums.size() << "):\n\n";

    size_t maxName = 0;
    for (auto& e : hdr.enums)
        maxName = std::max(maxName, e.name.size());
    maxName = std::min(maxName, (size_t)28);

    for (auto& e : hdr.enums) {
        std::string countStr = std::to_string(e.values.size()) + " values";
        std::string rangeStr;
        if (!e.values.empty()) {
            rangeStr = e.values.front().name + " .. " + e.values.back().name;
        }

        std::cout << "  " << green(padRight(e.name, maxName + 2))
                  << dim(padRight(countStr, 14))
                  << cyan(rangeStr) << "\n";
    }

    std::cout << "\n";
}

// ── List: Macros ────────────────────────────────────────────────────────────

void HelpCFormatter::printListMacros(const HelpCHeaderInfo& hdr) const {
    if (hdr.macros.empty() && hdr.structMacros.empty()) {
        std::cout << "No macros found in <" << hdr.headerName << ">\n";
        return;
    }

    size_t total = hdr.macros.size() + hdr.structMacros.size();
    std::cout << "\n" << bold("Macros") << " in <" << hdr.headerName
              << "> (" << total << "):\n";

    // Integer macros
    if (!hdr.macros.empty()) {
        std::cout << "\n  " << dim("Integer constants:") << "\n";

        size_t maxName = 0;
        for (auto& m : hdr.macros)
            maxName = std::max(maxName, m.name.size());
        maxName = std::min(maxName, (size_t)32);

        size_t show = opts_.showAll ? hdr.macros.size()
                                    : std::min(hdr.macros.size(), (size_t)20);
        for (size_t i = 0; i < show; i++) {
            auto& m = hdr.macros[i];
            std::cout << "    " << green(padRight(m.name, maxName + 2))
                      << "= " << (m.isNull ? "NULL" : std::to_string(m.value))
                      << "\n";
        }
        if (show < hdr.macros.size())
            std::cout << dim("    ... (" + std::to_string(hdr.macros.size() - show) +
                            " more, use --all)") << "\n";
    }

    // Struct macros
    if (!hdr.structMacros.empty()) {
        std::cout << "\n  " << dim("Struct literal constants:") << "\n";

        size_t maxName = 0;
        for (auto& sm : hdr.structMacros)
            maxName = std::max(maxName, sm.name.size());
        maxName = std::min(maxName, (size_t)20);

        for (auto& sm : hdr.structMacros) {
            std::string vals = sm.structType + " { ";
            for (size_t i = 0; i < sm.fieldValues.size(); i++) {
                if (i > 0) vals += ", ";
                vals += std::to_string(sm.fieldValues[i]);
            }
            vals += " }";

            std::cout << "    " << green(padRight(sm.name, maxName + 2))
                      << cyan(vals) << "\n";
        }
    }

    std::cout << "\n";
}

// ── List: Typedefs ──────────────────────────────────────────────────────────

void HelpCFormatter::printListTypedefs(const HelpCHeaderInfo& hdr) const {
    if (hdr.typedefs.empty()) {
        std::cout << "No typedefs found in <" << hdr.headerName << ">\n";
        return;
    }

    std::cout << "\n" << bold("Typedefs") << " in <" << hdr.headerName
              << "> (" << hdr.typedefs.size() << "):\n\n";

    size_t maxName = 0;
    for (auto& td : hdr.typedefs)
        maxName = std::max(maxName, td.name.size());
    maxName = std::min(maxName, (size_t)24);

    for (auto& td : hdr.typedefs) {
        std::cout << "  " << green(padRight(td.name, maxName + 2))
                  << "→ " << dim(td.underlyingCType)
                  << "  " << cyan("(" + td.underlyingLucisType + ")") << "\n";
    }

    std::cout << "\n";
}

// ── List: Globals ───────────────────────────────────────────────────────────

void HelpCFormatter::printListGlobals(const HelpCHeaderInfo& hdr) const {
    if (hdr.globals.empty()) {
        std::cout << "No globals found in <" << hdr.headerName << ">\n";
        return;
    }

    std::cout << "\n" << bold("Globals") << " in <" << hdr.headerName
              << "> (" << hdr.globals.size() << "):\n\n";

    size_t maxName = 0;
    for (auto& g : hdr.globals)
        maxName = std::max(maxName, g.name.size());
    maxName = std::min(maxName, (size_t)24);

    for (auto& g : hdr.globals) {
        std::cout << "  " << green(padRight(g.name, maxName + 2))
                  << cyan(g.lucisType) << dim("  (" + g.cType + ")") << "\n";
    }

    std::cout << "\n";
}

// ── Search results ──────────────────────────────────────────────────────────

void HelpCFormatter::printSearchResults(const std::vector<SearchResult>& results,
                                        const std::string& query,
                                        const HelpCHeaderInfo& hdr) const {
    if (results.empty()) {
        std::cout << "\nNo symbols matching '" << bold(query)
                  << "' in <" << hdr.headerName << ">\n\n";
        return;
    }

    std::cout << "\nSymbols matching '" << bold(query)
              << "' in <" << hdr.headerName << "> ("
              << results.size() << "):\n\n";

    size_t maxName = 0, maxKind = 0;
    for (auto& r : results) {
        maxName = std::max(maxName, r.name.size());
        maxKind = std::max(maxKind, r.kind.size());
    }
    maxName = std::min(maxName, (size_t)32);
    maxKind = std::min(maxKind, (size_t)12);

    for (auto& r : results) {
        std::cout << "  " << green(padRight(r.name, maxName + 2))
                  << magenta(padRight(r.kind, maxKind + 2))
                  << dim(r.summary) << "\n";
    }

    std::cout << "\n";
}

// ── Related symbols ─────────────────────────────────────────────────────────

void HelpCFormatter::printRelatedFunctions(const std::string& typeName,
                                           const HelpCHeaderInfo& hdr) const {
    std::vector<const HelpCFunction*> related;
    for (auto& fn : hdr.functions) {
        // Check if any parameter or return uses this type
        bool uses = false;
        if (fn.returnLucisType == typeName) uses = true;
        for (auto& p : fn.params) {
            if (p.lucisType == typeName || p.cType.find(typeName) != std::string::npos)
                uses = true;
        }
        if (uses) related.push_back(&fn);
    }

    if (related.empty()) return;

    size_t show = opts_.showAll ? related.size()
                                : std::min(related.size(), (size_t)10);

    std::cout << "\n" << bold("Related functions") << " ("
              << related.size() << "):\n";

    for (size_t i = 0; i < show; i++) {
        auto* fn = related[i];
        std::string sig;
        for (size_t j = 0; j < fn->params.size(); j++) {
            if (j > 0) sig += ", ";
            sig += fn->params[j].cType;
        }
        std::cout << "  " << green(padRight(fn->name, 36))
                  << dim("(" + sig + ")") << " → "
                  << cyan(fn->returnLucisType) << "\n";
    }
    if (show < related.size())
        std::cout << dim("  ... (" + std::to_string(related.size() - show) +
                        " more, use --all)") << "\n";
}

void HelpCFormatter::printRelatedMacros(const std::string& typeName,
                                        const HelpCHeaderInfo& hdr) const {
    std::vector<const HelpCStructMacro*> related;
    for (auto& sm : hdr.structMacros) {
        if (sm.structType == typeName)
            related.push_back(&sm);
    }

    if (related.empty()) return;

    std::cout << "\n" << bold("Related macros") << " ("
              << related.size() << "):\n  ";

    for (size_t i = 0; i < related.size(); i++) {
        if (i > 0) std::cout << ", ";
        if (i > 0 && i % 6 == 0) std::cout << "\n  ";
        std::cout << yellow(related[i]->name);
    }
    std::cout << "\n";
}

// ── JSON output ─────────────────────────────────────────────────────────────

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default:   out += c; break;
        }
    }
    return out;
}

void HelpCFormatter::printFunctionJson(const HelpCFunction& fn,
                                       const HelpCHeaderInfo& hdr) const {
    std::cout << "{\n";
    std::cout << "  \"kind\": \"function\",\n";
    std::cout << "  \"name\": \"" << jsonEscape(fn.name) << "\",\n";
    std::cout << "  \"header\": \"" << jsonEscape(hdr.headerName) << "\",\n";
    if (!hdr.linkFlag.empty())
        std::cout << "  \"library\": \"" << jsonEscape(hdr.linkFlag) << "\",\n";
    if (!fn.doc.empty())
        std::cout << "  \"doc\": \"" << jsonEscape(fn.doc) << "\",\n";
    std::cout << "  \"returnType\": { \"c\": \"" << jsonEscape(fn.returnCType)
              << "\", \"t\": \"" << jsonEscape(fn.returnLucisType) << "\" },\n";
    std::cout << "  \"parameters\": [\n";
    for (size_t i = 0; i < fn.params.size(); i++) {
        auto& p = fn.params[i];
        std::cout << "    { \"name\": \"" << jsonEscape(p.name)
                  << "\", \"cType\": \"" << jsonEscape(p.cType)
                  << "\", \"lucisType\": \"" << jsonEscape(p.lucisType) << "\" }";
        if (i + 1 < fn.params.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"isVariadic\": " << (fn.isVariadic ? "true" : "false") << "\n";
    std::cout << "}\n";
}

void HelpCFormatter::printStructJson(const HelpCStruct& s,
                                     const HelpCHeaderInfo& hdr) const {
    std::cout << "{\n";
    std::cout << "  \"kind\": \"" << (s.isUnion ? "union" : "struct") << "\",\n";
    std::cout << "  \"name\": \"" << jsonEscape(s.name) << "\",\n";
    std::cout << "  \"header\": \"" << jsonEscape(hdr.headerName) << "\",\n";
    if (s.size >= 0)
        std::cout << "  \"size\": " << s.size << ",\n";
    if (s.alignment >= 0)
        std::cout << "  \"alignment\": " << s.alignment << ",\n";
    if (!s.doc.empty())
        std::cout << "  \"doc\": \"" << jsonEscape(s.doc) << "\",\n";
    std::cout << "  \"fields\": [\n";
    for (size_t i = 0; i < s.fields.size(); i++) {
        auto& f = s.fields[i];
        std::cout << "    { \"name\": \"" << jsonEscape(f.name)
                  << "\", \"cType\": \"" << jsonEscape(f.cType)
                  << "\", \"lucisType\": \"" << jsonEscape(f.lucisType) << "\"";
        if (f.offset >= 0)
            std::cout << ", \"offset\": " << f.offset;
        if (f.size >= 0)
            std::cout << ", \"size\": " << f.size;
        std::cout << " }";
        if (i + 1 < s.fields.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}

void HelpCFormatter::printEnumJson(const HelpCEnum& e,
                                   const HelpCHeaderInfo& hdr) const {
    std::cout << "{\n";
    std::cout << "  \"kind\": \"enum\",\n";
    std::cout << "  \"name\": \"" << jsonEscape(e.name) << "\",\n";
    std::cout << "  \"header\": \"" << jsonEscape(hdr.headerName) << "\",\n";
    if (!e.doc.empty())
        std::cout << "  \"doc\": \"" << jsonEscape(e.doc) << "\",\n";
    std::cout << "  \"values\": [\n";
    for (size_t i = 0; i < e.values.size(); i++) {
        auto& v = e.values[i];
        std::cout << "    { \"name\": \"" << jsonEscape(v.name)
                  << "\", \"value\": " << v.value << " }";
        if (i + 1 < e.values.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}

void HelpCFormatter::printHeaderJson(const HelpCHeaderInfo& hdr) const {
    std::cout << "{\n";
    std::cout << "  \"header\": \"" << jsonEscape(hdr.headerName) << "\",\n";
    if (!hdr.linkFlag.empty())
        std::cout << "  \"library\": \"" << jsonEscape(hdr.linkFlag) << "\",\n";
    std::cout << "  \"functions\": " << hdr.functions.size() << ",\n";
    std::cout << "  \"structs\": " << hdr.structs.size() << ",\n";
    std::cout << "  \"enums\": " << hdr.enums.size() << ",\n";
    std::cout << "  \"typedefs\": " << hdr.typedefs.size() << ",\n";
    std::cout << "  \"macros\": " << (hdr.macros.size() + hdr.structMacros.size()) << ",\n";
    std::cout << "  \"globals\": " << hdr.globals.size() << "\n";
    std::cout << "}\n";
}
