#include "helpc/HelpC.h"
#include "helpc/HelpCParser.h"
#include "helpc/HelpCSearch.h"

#include <iostream>
#include <string>

// ── Usage ───────────────────────────────────────────────────────────────────

void HelpC::printUsage() {
    std::cout
        << "lucis helpc — C Library Reference\n\n"
        << "Usage:\n"
        << "  lucis helpc <lib> [symbol]     Look up a symbol\n"
        << "  lucis helpc <lib> --list       List all symbols (summary)\n"
        << "  lucis helpc <lib> -l functions  List functions\n"
        << "  lucis helpc <lib> -l structs    List structs\n"
        << "  lucis helpc <lib> -l enums      List enums\n"
        << "  lucis helpc <lib> -l macros     List macros\n"
        << "  lucis helpc <lib> -l typedefs   List typedefs\n"
        << "  lucis helpc <lib> -l globals    List globals\n"
        << "  lucis helpc <lib> -s <query>    Search symbols\n\n"
        << "Flags:\n"
        << "  --all, -a         Show all values (don't truncate)\n"
        << "  --verbose, -v     Show offsets, sizes, raw C details\n"
        << "  --related, -r     Show related symbols\n"
        << "  --json            Output as JSON\n"
        << "  --no-color        Disable ANSI colors\n"
        << "  --regex           Treat search query as regex\n"
        << "  -I<path>          Extra include path\n\n"
        << "Examples:\n"
        << "  lucis helpc raylib InitWindow\n"
        << "  lucis helpc raylib Color -r\n"
        << "  lucis helpc raylib --list\n"
        << "  lucis helpc stdio printf\n"
        << "  lucis helpc raylib -s Draw\n"
        << "  lucis helpc raylib KeyboardKey --all\n"
        << "  lucis helpc raylib InitWindow --json\n";
}

// ── Argument parsing ────────────────────────────────────────────────────────

bool HelpC::parseArgs(int argc, char* argv[], HelpCCommand& cmd) {
    // argv[0] = "lucis", argv[1] = "helpc"
    // We need at least argv[2] (the header/lib)
    if (argc < 3) return false;

    cmd.header = argv[2];

    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--list" || arg == "-l") {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cmd.listCategory = argv[++i];
            } else {
                cmd.listCategory = "summary";
            }
        } else if (arg == "--search" || arg == "-s") {
            if (i + 1 < argc) {
                cmd.searchQuery = argv[++i];
            } else {
                std::cerr << "lucis helpc: --search requires a query\n";
                return false;
            }
        } else if (arg == "--all" || arg == "-a") {
            cmd.displayOpts.showAll = true;
        } else if (arg == "--verbose" || arg == "-v") {
            cmd.displayOpts.verbose = true;
        } else if (arg == "--related" || arg == "-r") {
            cmd.displayOpts.showRelated = true;
        } else if (arg == "--json") {
            cmd.displayOpts.jsonOutput = true;
            cmd.displayOpts.noColor = true; // JSON never has colors
        } else if (arg == "--no-color") {
            cmd.displayOpts.noColor = true;
        } else if (arg == "--regex") {
            cmd.displayOpts.useRegex = true;
        } else if (arg.rfind("-I", 0) == 0 && arg.size() > 2) {
            cmd.includePaths.push_back(arg.substr(2));
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return false;
        } else if (arg[0] != '-') {
            // Positional: the symbol name
            cmd.symbol = arg;
        } else {
            std::cerr << "lucis helpc: unknown flag '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

// ── Symbol lookup ───────────────────────────────────────────────────────────

bool HelpC::lookupSymbol(const std::string& name,
                         const HelpCHeaderInfo& hdr,
                         const HelpCFormatter& fmt,
                         const HelpCOptions& opts) {
    // Functions
    for (auto& fn : hdr.functions) {
        if (fn.name == name) {
            if (opts.jsonOutput)
                fmt.printFunctionJson(fn, hdr);
            else
                fmt.printFunction(fn, hdr);
            if (opts.showRelated && !opts.jsonOutput) {
                // Show functions with similar name
            }
            return true;
        }
    }

    // Structs
    for (auto& s : hdr.structs) {
        if (s.name == name) {
            if (opts.jsonOutput)
                fmt.printStructJson(s, hdr);
            else {
                fmt.printStruct(s, hdr);
                if (opts.showRelated) {
                    fmt.printRelatedFunctions(name, hdr);
                    fmt.printRelatedMacros(name, hdr);
                }
            }
            return true;
        }
    }

    // Enums
    for (auto& e : hdr.enums) {
        if (e.name == name) {
            if (opts.jsonOutput)
                fmt.printEnumJson(e, hdr);
            else
                fmt.printEnum(e, hdr);
            return true;
        }
    }

    // Check if name is an enum VALUE (e.g. KEY_SPACE)
    for (auto& e : hdr.enums) {
        for (auto& v : e.values) {
            if (v.name == name) {
                // Print the entire enum with this value highlighted
                if (opts.jsonOutput)
                    fmt.printEnumJson(e, hdr);
                else {
                    std::cout << "\n'" << name << "' is a value of enum '"
                              << e.name << "' (= " << v.value << ")\n\n";
                    fmt.printEnum(e, hdr);
                }
                return true;
            }
        }
    }

    // Typedefs
    for (auto& td : hdr.typedefs) {
        if (td.name == name) {
            fmt.printTypedef(td, hdr);
            // Also check if it aliases a struct and show related
            if (opts.showRelated) {
                fmt.printRelatedFunctions(name, hdr);
            }
            return true;
        }
    }

    // Integer macros
    for (auto& m : hdr.macros) {
        if (m.name == name) {
            fmt.printMacro(m, hdr);
            return true;
        }
    }

    // Struct macros
    for (auto& sm : hdr.structMacros) {
        if (sm.name == name) {
            fmt.printStructMacro(sm, hdr);
            return true;
        }
    }

    // Globals
    for (auto& g : hdr.globals) {
        if (g.name == name) {
            fmt.printGlobal(g, hdr);
            return true;
        }
    }

    return false;
}

// ── Main run ────────────────────────────────────────────────────────────────

int HelpC::run(const HelpCCommand& cmd) {
    HelpCParser parser(cmd.includePaths);
    HelpCHeaderInfo hdr;

    if (!parser.parse(cmd.header, hdr))
        return 1;

    HelpCFormatter fmt(cmd.displayOpts);

    // Mode 1: --list
    if (!cmd.listCategory.empty()) {
        if (cmd.displayOpts.jsonOutput) {
            fmt.printHeaderJson(hdr);
            return 0;
        }

        if (cmd.listCategory == "summary" || cmd.listCategory == "all") {
            fmt.printSummary(hdr);
            if (cmd.listCategory == "all") {
                fmt.printListFunctions(hdr);
                fmt.printListStructs(hdr);
                fmt.printListEnums(hdr);
                fmt.printListMacros(hdr);
                fmt.printListTypedefs(hdr);
                fmt.printListGlobals(hdr);
            }
        } else if (cmd.listCategory == "functions" || cmd.listCategory == "fn") {
            fmt.printListFunctions(hdr);
        } else if (cmd.listCategory == "structs") {
            fmt.printListStructs(hdr);
        } else if (cmd.listCategory == "enums") {
            fmt.printListEnums(hdr);
        } else if (cmd.listCategory == "macros") {
            fmt.printListMacros(hdr);
        } else if (cmd.listCategory == "typedefs") {
            fmt.printListTypedefs(hdr);
        } else if (cmd.listCategory == "globals") {
            fmt.printListGlobals(hdr);
        } else {
            std::cerr << "lucis helpc: unknown category '" << cmd.listCategory
                      << "'\n";
            std::cerr << "Valid: functions, structs, enums, macros, typedefs, globals, all\n";
            return 1;
        }
        return 0;
    }

    // Mode 2: --search
    if (!cmd.searchQuery.empty()) {
        std::vector<HelpCFormatter::SearchResult> results;
        if (cmd.displayOpts.useRegex)
            results = HelpCSearch::searchRegex(hdr, cmd.searchQuery);
        else
            results = HelpCSearch::search(hdr, cmd.searchQuery);

        fmt.printSearchResults(results, cmd.searchQuery, hdr);
        return results.empty() ? 1 : 0;
    }

    // Mode 3: symbol lookup
    if (!cmd.symbol.empty()) {
        if (lookupSymbol(cmd.symbol, hdr, fmt, cmd.displayOpts))
            return 0;

        // Not found — try fuzzy match
        std::cerr << "Symbol '" << cmd.symbol << "' not found in <"
                  << hdr.headerName << ">.\n";

        auto suggestions = HelpCSearch::fuzzyMatch(hdr, cmd.symbol);
        if (!suggestions.empty()) {
            std::cerr << "\nDid you mean:\n";
            fmt.printSearchResults(suggestions, cmd.symbol, hdr);
        }
        return 1;
    }

    // Mode 4: no symbol, no search, no list — show summary
    fmt.printSummary(hdr);
    return 0;
}
