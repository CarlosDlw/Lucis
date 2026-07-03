#include "ffi/CMacroEval.h"
#include "ffi/CBindings.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────

// Extract macro name from a "#define NAME ..." line.
static std::string extractDefineName(const std::string& line) {
    auto s = line.find("#define");
    if (s == std::string::npos) return {};
    s += 7; // skip "#define"
    // Skip optional space for "# define"
    if (s < line.size() && line[s] == ' ') s++;
    else if (s < line.size() && line[s] != ' ' && line[s] != '\t') return {};
    s = line.find_first_not_of(" \t", s);
    if (s == std::string::npos) return {};
    size_t e = s;
    while (e < line.size() && !std::isspace(line[e]) && line[e] != '(') e++;
    return line.substr(s, e - s);
}

// Collect macro names from source text that have #define directives.
static std::unordered_set<std::string> collectDefineNames(const std::string& rawC) {
    std::unordered_set<std::string> names;
    std::istringstream stream(rawC);
    std::string line;
    while (std::getline(stream, line)) {
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line.find("#define") != 0) continue;
        auto name = extractDefineName(line);
        if (!name.empty()) names.insert(name);
    }
    return names;
}

// Parse a single -dM output line into macro info.
// Returns false if the line is not a valid macro definition.
static bool parseDMacroLine(const std::string& line,
                            std::string& name,
                            bool& isFuncLike,
                            std::vector<std::string>& params,
                            std::string& body) {
    if (line.find("#define ") != 0) return false;

    auto s = std::string("#define ").size();
    s = line.find_first_not_of(" \t", s);
    if (s == std::string::npos) return false;

    // Read name
    size_t nameStart = s;
    while (s < line.size() && !std::isspace(line[s]) && line[s] != '(') s++;
    name = line.substr(nameStart, s - nameStart);
    if (name.empty()) return false;

    // Check if function-like
    if (s < line.size() && line[s] == '(') {
        isFuncLike = true;
        // Parse params
        auto close = line.find(')', s);
        if (close != std::string::npos) {
            std::string pstr = line.substr(s + 1, close - s - 1);
            std::istringstream ps(pstr);
            std::string p;
            while (std::getline(ps, p, ',')) {
                auto f = p.find_first_not_of(" \t");
                auto e = p.find_last_not_of(" \t");
                if (f != std::string::npos && e != std::string::npos)
                    params.push_back(p.substr(f, e - f + 1));
            }
            s = close + 1;
        }
    } else {
        isFuncLike = false;
    }

    // Body (rest of line after name / closing paren)
    body = line.substr(s);
    auto bf = body.find_first_not_of(" \t");
    if (bf != std::string::npos)
        body = body.substr(bf);
    else
        body.clear();

    return true;
}

// Tokenize a macro body into tokens for function-like macros.
static std::vector<std::string> tokenizeBody(const std::string& body) {
    std::vector<std::string> tokens;
    std::string tok;
    for (size_t i = 0; i < body.size(); i++) {
        char c = body[i];
        if (std::isspace(c)) {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            continue;
        }
        if (c == '<' && i+1 < body.size() && body[i+1] == '<') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back("<<"); i++;
            continue;
        }
        if (c == '>' && i+1 < body.size() && body[i+1] == '>') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(">>"); i++;
            continue;
        }
        if (c == '(' || c == ')' || c == '+' || c == '-' || c == '*' ||
            c == '/' || c == '%' || c == '&' || c == '|' || c == '^' ||
            c == '~' || c == '?' || c == ':') {
            if (!tok.empty()) { tokens.push_back(tok); tok.clear(); }
            tokens.push_back(std::string(1, c));
            continue;
        }
        tok += c;
    }
    if (!tok.empty()) tokens.push_back(tok);
    return tokens;
}

// Scan source for flag macros (#define NAME without value).
static std::unordered_set<std::string> collectEmptyDefines(const std::string& rawC) {
    std::unordered_set<std::string> names;
    std::istringstream stream(rawC);
    std::string line;
    while (std::getline(stream, line)) {
        auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        line = line.substr(first);
        if (line.find("#define") != 0) continue;
        // "#define NAME" with nothing after the name
        auto name = extractDefineName(line);
        if (name.empty()) continue;
        // Check if there's a value after the name
        auto valStart = line.find_first_not_of(" \t", line.find(name) + name.size());
        if (valStart == std::string::npos || line[valStart] == '\r' || line[valStart] == '\n')
            names.insert(name);
        else if (valStart < line.size() &&
                 (line[valStart] == '/' && valStart+1 < line.size() && line[valStart+1] == '/'))
            names.insert(name); // comment instead of value
    }
    return names;
}

// ═══════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════

bool evalCMacroRaw(const std::string& rawC,
                   const std::string& sourceFile,
                   unsigned lineNumber,
                   const std::string& tempDir,
                   CBindings& bindings,
                   bool evalValues) {
    // 1. Ensure temp directory exists
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    if (ec) {
        std::cerr << "[cmacro] cannot create temp dir: " << tempDir << "\n";
        return false;
    }

    // 2. Create a unique temp file
    fs::path tmpPath = fs::path(tempDir) / ("cmacro_" + std::to_string(::rand()) + ".c");
    {
        std::ofstream ofs(tmpPath);
        if (!ofs) {
            std::cerr << "[cmacro] failed to create temp file\n";
            return false;
        }
        ofs << rawC;
    }

    // 3. Collect expected macro names from source
    auto expectedNames = collectDefineNames(rawC);
    if (expectedNames.empty()) {
        fs::remove(tmpPath, ec);
        return true; // nothing to define
    }

    // 4. Run gcc -E -dM to get all macro definitions after preprocessing
    //    -P omits linemarkers; -dM dumps macro definitions.
    std::string dM_cmd = "gcc -E -dM -P -x c " + tmpPath.string() + " 2>/dev/null";

    std::array<char, 4096> buffer;
    std::string dM_output;
    FILE* dM_pipe = popen(dM_cmd.c_str(), "r");
    if (dM_pipe) {
        while (fgets(buffer.data(), buffer.size(), dM_pipe))
            dM_output += buffer.data();
        pclose(dM_pipe);
    }

    // 5. Parse -dM output and register macros
    {
        std::istringstream dM_stream(dM_output);
        std::string dM_line;
        while (std::getline(dM_stream, dM_line)) {
            std::string name;
            bool isFuncLike = false;
            std::vector<std::string> params;
            std::string body;
            if (!parseDMacroLine(dM_line, name, isFuncLike, params, body))
                continue;
            if (!expectedNames.count(name))
                continue; // not our macro (e.g. system header macro)

            if (isFuncLike) {
                // Check if already registered
                if (bindings.findFunctionLikeMacro(name))
                    continue;
                auto bodyTokens = tokenizeBody(body);
                CFunctionLikeMacro flm;
                flm.name = name;
                flm.paramNames = params;
                flm.bodyTokens = bodyTokens;
                flm.sourceFile = sourceFile;
                flm.line = lineNumber;
                bindings.addFunctionLikeMacro(std::move(flm));
            } else {
                if (bindings.findMacro(name))
                    continue;
                // Parse integer value if possible for LSP display.
                // The real value evaluation (via gcc + printf) happens in step 6.
                int64_t val = 0;
                if (!body.empty()) {
                    char* end = nullptr;
                    int64_t parsed = strtoll(body.c_str(), &end, 0);
                    if (end && end != body.c_str() &&
                        (*end == '\0' || std::isspace(static_cast<unsigned char>(*end))))
                        val = parsed;
                }
                CMacro cm;
                cm.name = name;
                cm.value = val;
                cm.sourceFile = sourceFile;
                cm.line = lineNumber;
                bindings.addMacro(std::move(cm));
            }
        }
    }

    // 6. Optionally evaluate integer values via gcc + printf
    if (evalValues) {
        // Only evaluate macros that have values (skip flag macros)
        auto emptyDefines = collectEmptyDefines(rawC);

        std::vector<std::string> simpleDefinesLines;
        {
            std::istringstream stream(rawC);
            std::string line;
            while (std::getline(stream, line)) {
                auto first = line.find_first_not_of(" \t\r");
                if (first == std::string::npos) continue;
                line = line.substr(first);
                if (line.find("#define ") != 0) continue;

                auto nameEnd = line.find_first_of(" \t(", 8);
                if (nameEnd == std::string::npos) continue;
                if (line[nameEnd] == '(') continue; // function-like — skip

                auto name = line.substr(8, nameEnd - 8);
                if (name.empty()) continue;
                if (emptyDefines.count(name)) continue; // flag macro

                simpleDefinesLines.push_back(line);
            }
        }

        if (!simpleDefinesLines.empty()) {
            fs::path evalSrcPath = fs::path(tempDir) / ("cmacro_eval_" + std::to_string(::rand()) + ".c");
            fs::path evalOutPath = fs::path(tempDir) / ("cmacro_eval_" + std::to_string(::rand()));
            {
                std::ofstream ofs(evalSrcPath);
                if (ofs) {
                    ofs << rawC << "\n\n"
                        << "#include <stdio.h>\n#include <stdint.h>\nint main() {\n";
                    for (size_t i = 0; i < simpleDefinesLines.size(); i++) {
                        auto name = extractDefineName(simpleDefinesLines[i]);
                        if (!name.empty())
                            ofs << "    printf(\"__lucis_cmacro_" << i << "=%lld\\n\", "
                                << "(long long)(" << name << "));\n";
                    }
                    ofs << "    return 0;\n}\n";
                }
            }

            std::string compCmd = "gcc -x c -o " + evalOutPath.string() + " "
                                  + evalSrcPath.string() + " 2>/dev/null";
            int compRet = std::system(compCmd.c_str());
            if (compRet == 0) {
                std::string runCmd = evalOutPath.string() + " 2>/dev/null";
                FILE* runPipe = popen(runCmd.c_str(), "r");
                if (runPipe) {
                    std::string runOutput;
                    while (fgets(buffer.data(), buffer.size(), runPipe))
                        runOutput += buffer.data();
                    pclose(runPipe);

                    std::istringstream outStream(runOutput);
                    std::string outLine;
                    while (std::getline(outStream, outLine)) {
                        if (outLine.find("__lucis_cmacro_") != 0) continue;
                        auto eq = outLine.find('=');
                        if (eq == std::string::npos) continue;
                        auto idxStr = outLine.substr(15, eq - 15);
                        auto valStr = outLine.substr(eq + 1);
                        size_t idx = 0;
                        try { idx = std::stoul(idxStr); }
                        catch (...) { continue; }
                        if (idx >= simpleDefinesLines.size()) continue;
                        auto name = extractDefineName(simpleDefinesLines[idx]);
                        if (name.empty()) continue;
                        int64_t val = 0;
                        try { val = std::stoll(valStr); }
                        catch (...) { continue; }

                        // Update the already-registered CMacro
                        CMacro cm;
                        cm.name = name;
                        cm.value = val;
                        cm.sourceFile = sourceFile;
                        cm.line = lineNumber;
                        // addMacro replaces existing by name (map insert/assign)
                        bindings.addMacro(std::move(cm));
                    }
                }
                fs::remove(evalOutPath, ec);
            }
            fs::remove(evalSrcPath, ec);
        }
    }

    // Cleanup temp file
    fs::remove(tmpPath, ec);
    return true;
}
