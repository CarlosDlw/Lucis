#include "lsp/LspServer.h"
#include "lsp/ProjectContext.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

// LSP error codes
static constexpr int kParseError     = -32700;
static constexpr int kMethodNotFound = -32601;
static constexpr int kInvalidRequest = -32600;

// ═══════════════════════════════════════════════════════════════════════
//  Main loop
// ═══════════════════════════════════════════════════════════════════════

int LspServer::run() {
    // LSP uses stderr for logging (stdout is the protocol channel)
    std::cerr << "[lux-lsp] server started\n";

    while (!shutdown_) {
        auto raw = transport_.readMessage();
        if (!raw) {
            break; // EOF or read error
        }

        json msg;
        try {
            msg = json::parse(*raw);
        } catch (const json::parse_error& e) {
            std::cerr << "[lux-lsp] JSON parse error: " << e.what() << "\n";
            continue;
        }

        dispatch(msg);
    }

    std::cerr << "[lux-lsp] server stopped\n";
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  Dispatch
// ═══════════════════════════════════════════════════════════════════════

void LspServer::dispatch(const json& msg) {
    if (!msg.contains("method")) {
        // Response or invalid — ignore for now
        return;
    }

    std::string method = msg["method"];

    if (method == "initialize") {
        handleInitialize(msg);
    } else if (method == "initialized") {
        handleInitialized(msg);
    } else if (method == "shutdown") {
        handleShutdown(msg);
    } else if (method == "exit") {
        shutdown_ = true;
    } else if (method == "textDocument/didOpen") {
        handleDidOpen(msg);
    } else if (method == "textDocument/didChange") {
        handleDidChange(msg);
    } else if (method == "textDocument/didSave") {
        handleDidSave(msg);
    } else if (method == "textDocument/didClose") {
        handleDidClose(msg);
    } else if (method == "textDocument/hover") {
        handleHover(msg);
    } else if (method == "textDocument/definition") {
        handleDefinition(msg);
    } else if (method == "textDocument/completion") {
        handleCompletion(msg);
    } else if (method == "textDocument/semanticTokens/full") {
        handleSemanticTokensFull(msg);
    } else if (method == "textDocument/signatureHelp") {
        handleSignatureHelp(msg);
    } else {
        // Unknown method — if it has an id, it's a request → respond with error
        if (msg.contains("id")) {
            sendError(msg["id"], kMethodNotFound,
                      "method not supported: " + method);
        }
        // Notifications without id are silently ignored
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  LSP Lifecycle
// ═══════════════════════════════════════════════════════════════════════

void LspServer::handleInitialize(const json& msg) {
    std::cerr << "[lux-lsp] initialize\n";

    json capabilities = {
        {"textDocumentSync", {
            {"openClose", true},
            {"save", {{"includeText", true}}},
            {"change", 1}  // Full document sync
        }},
        {"hoverProvider", true},
        {"definitionProvider", true},
        {"completionProvider", {
            {"triggerCharacters", {".", ":", ">", "@"}},
            {"resolveProvider", false}
        }},
        {"signatureHelpProvider", {
            {"triggerCharacters", {"(", ","}}
        }},
        {"semanticTokensProvider", {
            {"full", true},
            {"legend", {
                {"tokenTypes", SemanticTokensProvider::tokenTypes()},
                {"tokenModifiers", SemanticTokensProvider::tokenModifiers()}
            }}
        }}
    };

    json result = {
        {"capabilities", capabilities},
        {"serverInfo", {
            {"name", "lux-lsp"},
            {"version", "0.1.0"}
        }}
    };

    sendResponse(msg["id"], result);
    initialized_ = true;
}

void LspServer::handleInitialized(const json& /*msg*/) {
    std::cerr << "[lux-lsp] initialized\n";
}

void LspServer::handleShutdown(const json& msg) {
    std::cerr << "[lux-lsp] shutdown\n";
    sendResponse(msg["id"], nullptr);
    shutdown_ = true;
}

// ═══════════════════════════════════════════════════════════════════════
//  Main file discovery
// ═══════════════════════════════════════════════════════════════════════

// Quick text-based search for the file that has `namespace Main` and a `main`
// function. Avoids full parsing — just looks for the two markers in source text.
std::string LspServer::findMainFile(const std::string& projectRoot) {
    if (projectRoot.empty()) return {};
    try {
        for (auto& entry : fs::recursive_directory_iterator(
                 projectRoot, fs::directory_options::skip_permission_denied)) {
            std::error_code ec;
            if (!entry.is_regular_file(ec) || ec) continue;
            if (entry.path().extension() != ".lx") continue;

            std::ifstream f(entry.path());
            if (!f) continue;

            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

            // Must declare the Main namespace
            if (content.find("namespace Main") == std::string::npos) continue;

            // Must define a top-level main() — look for common signatures:
            // "main()" or "main(int" ...
            bool hasMain = content.find(" main(") != std::string::npos
                        || content.find("\nmain(") != std::string::npos;
            if (!hasMain) continue;

            return entry.path().string();
        }
    } catch (...) {}
    return {};
}

// Rebuilds the ProjectContext, always anchoring to the main file when
// possible. This prevents the context from becoming stale when the user
// switches between tabs (VS Code does not re-fire didOpen for already-
// opened documents).
void LspServer::rebuildContext(const std::string& filePath, bool force) {
    std::string projectRoot = ProjectContext::findProjectRoot(filePath);
    if (projectRoot.empty()) return;

    // (Re-)discover the main file if we don't know it yet, or if this file
    // IS the main file (so we always prefer the freshest path).
    if (mainFilePath_.empty()) {
        mainFilePath_ = findMainFile(projectRoot);
        if (!mainFilePath_.empty())
            std::cerr << "[lux-lsp] main file: " << mainFilePath_ << "\n";
    } else {
        // If the file being opened/saved declares `namespace Main`, update cache.
        // (handles renames or new main files added to the project)
        std::ifstream f(filePath);
        if (f) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            bool hasNamespaceMain = content.find("namespace Main") != std::string::npos;
            bool hasMain = content.find(" main(") != std::string::npos
                        || content.find("\nmain(") != std::string::npos;
            if (hasNamespaceMain && hasMain)
                mainFilePath_ = filePath;
        }
    }

    // Always build context from the main file so it stays stable across tab
    // switches. Fall back to the current file if main cannot be found.
    std::string anchorFile = mainFilePath_.empty() ? filePath : mainFilePath_;

    // Fast-path for didOpen storms: if we're already built for the same
    // project root and same anchor, skip expensive full-project parsing.
    if (!force && projectContext_.isValid() &&
        contextProjectRoot_ == projectRoot &&
        contextAnchorPath_ == anchorFile) {
        return;
    }

    if (projectContext_.build(anchorFile)) {
        contextProjectRoot_ = projectRoot;
        contextAnchorPath_ = anchorFile;
        std::cerr << "[lux-lsp] project root: "
                  << projectContext_.projectRoot()
                  << " (anchored to " << anchorFile << ")\n";
    } else {
        std::cerr << "[lux-lsp] project build FAILED for: "
                  << anchorFile << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Document Sync
// ═══════════════════════════════════════════════════════════════════════

void LspServer::handleDidOpen(const json& msg) {
    auto& params = msg["params"];
    auto& td     = params["textDocument"];
    std::string uri  = td["uri"];
    std::string text = td["text"];

    documents_.open(uri, text);
    parseCache_.invalidate(uri);
    std::cerr << "[lux-lsp] opened: " << uri << "\n";

    std::string filePath = DocumentStore::uriToPath(uri);
    rebuildContext(filePath, false);

    publishDiagnostics(uri, text);
}

void LspServer::handleDidSave(const json& msg) {
    auto& params = msg["params"];
    auto& td     = params["textDocument"];
    std::string uri = td["uri"];

    std::string text;
    if (params.contains("text")) {
        text = params["text"];
        documents_.update(uri, text);
        parseCache_.invalidate(uri);
        cachedTokens_.erase(uri);
    } else {
        text = documents_.get(uri);
    }

    std::cerr << "[lux-lsp] saved: " << uri << "\n";

    // Rebuild project context on save (files may have changed).
    std::string filePath = DocumentStore::uriToPath(uri);
    rebuildContext(filePath, true);

    publishDiagnostics(uri, text);
}

void LspServer::handleDidChange(const json& msg) {
    auto& params = msg["params"];
    std::string uri = params["textDocument"]["uri"];

    // Full document sync (change mode 1): contentChanges[0].text is the full text
    auto& changes = params["contentChanges"];
    if (changes.empty()) return;

    std::string text = changes[0]["text"];
    documents_.update(uri, text);
    parseCache_.invalidate(uri);
    cachedTokens_.erase(uri);

    publishDiagnostics(uri, text);
}

void LspServer::handleDidClose(const json& msg) {
    auto& params = msg["params"];
    std::string uri = params["textDocument"]["uri"];

    documents_.close(uri);
    parseCache_.invalidate(uri);
    cachedTokens_.erase(uri);
    std::cerr << "[lux-lsp] closed: " << uri << "\n";

    // Clear diagnostics for the closed file
    sendNotification("textDocument/publishDiagnostics", {
        {"uri", uri},
        {"diagnostics", json::array()}
    });
}

// ═══════════════════════════════════════════════════════════════════════
//  Diagnostics
// ═══════════════════════════════════════════════════════════════════════

void LspServer::publishDiagnostics(const std::string& uri,
                                    const std::string& source) {
    std::string filePath = DocumentStore::uriToPath(uri);
    auto* cached = parseCache_.getOrParse(uri, source);
    auto diags = diagnosticEngine_.run(source, filePath,
                                        projectContext_.isValid()
                                            ? &projectContext_ : nullptr,
                                        cached);

    json lspDiags = json::array();
    for (auto& d : diags) {
        json item = {
            {"range", {
                {"start", {{"line", d.line}, {"character", d.col}}},
                {"end",   {{"line", d.endLine}, {"character", d.endCol}}}
            }},
            {"severity", static_cast<int>(d.severity)},
            {"source", d.source},
            {"message", d.message}
        };
        if (!d.code.empty())
            item["code"] = d.code;
        lspDiags.push_back(std::move(item));
    }

    sendNotification("textDocument/publishDiagnostics", {
        {"uri", uri},
        {"diagnostics", lspDiags}
    });

    std::cerr << "[lux-lsp] published " << diags.size()
              << " diagnostic(s) for " << uri << "\n";
}

// ═══════════════════════════════════════════════════════════════════════
//  Language Features
// ═══════════════════════════════════════════════════════════════════════

void LspServer::handleHover(const json& msg) {
    auto& params = msg["params"];
    std::string uri = params["textDocument"]["uri"];
    size_t line = params["position"]["line"];
    size_t col  = params["position"]["character"];

    std::string source = documents_.get(uri);
    if (source.empty()) {
        sendResponse(msg["id"], nullptr);
        return;
    }

    // Extract file path from URI for project context
    std::string filePath = uri;
    if (filePath.rfind("file://", 0) == 0)
        filePath = filePath.substr(7);

    auto* cached = parseCache_.getOrParse(uri, source);
    auto result = hoverProvider_.hover(source, line, col, filePath,
                                        projectContext_.isValid()
                                            ? &projectContext_ : nullptr,
                                        cached);
    if (!result) {
        sendResponse(msg["id"], nullptr);
        return;
    }

    json hover = {
        {"contents", {
            {"kind", "markdown"},
            {"value", result->contents}
        }},
        {"range", {
            {"start", {{"line", result->line}, {"character", result->col}}},
            {"end",   {{"line", result->endLine}, {"character", result->endCol}}}
        }}
    };

    sendResponse(msg["id"], hover);
}

void LspServer::handleDefinition(const json& msg) {
    try {
        auto& params = msg["params"];
        std::string uri = params["textDocument"]["uri"];
        size_t line = params["position"]["line"];
        size_t col  = params["position"]["character"];

        std::string source = documents_.get(uri);
        if (source.empty()) {
            sendResponse(msg["id"], nullptr);
            return;
        }

        std::string filePath = uri;
        if (filePath.rfind("file://", 0) == 0)
            filePath = filePath.substr(7);

        auto* cached = parseCache_.getOrParse(uri, source);
        auto result = definitionProvider_.definition(source, line, col, filePath,
                                                      projectContext_.isValid()
                                                          ? &projectContext_ : nullptr,
                                                      cached);
        if (!result) {
            sendResponse(msg["id"], nullptr);
            return;
        }

        json location = {
            {"uri", result->uri},
            {"range", {
                {"start", {{"line", result->line}, {"character", result->col}}},
                {"end",   {{"line", result->endLine}, {"character", result->endCol}}}
            }}
        };

        sendResponse(msg["id"], location);
    } catch (const std::exception& e) {
        std::cerr << "[lux-lsp] definition error: " << e.what() << "\n";
        sendResponse(msg["id"], nullptr);
    } catch (...) {
        std::cerr << "[lux-lsp] definition unknown error\n";
        sendResponse(msg["id"], nullptr);
    }
}

void LspServer::handleCompletion(const json& msg) {
    try {
        auto& params = msg["params"];
        std::string uri = params["textDocument"]["uri"];
        size_t line = params["position"]["line"];
        size_t col  = params["position"]["character"];

        std::string source = documents_.get(uri);
        if (source.empty()) {
            sendResponse(msg["id"], json::array());
            return;
        }

        std::string filePath = uri;
        if (filePath.rfind("file://", 0) == 0)
            filePath = filePath.substr(7);

        auto* cached = parseCache_.getOrParse(uri, source);
        auto items = completionProvider_.complete(
            source, line, col, filePath,
            projectContext_.isValid() ? &projectContext_ : nullptr,
            cached);

        json result = json::array();
        for (auto& item : items) {
            json ci = {
                {"label", item.label},
                {"kind", static_cast<int>(item.kind)}
            };
            if (!item.detail.empty())
                ci["detail"] = item.detail;
            if (!item.documentation.empty())
                ci["documentation"] = {
                    {"kind", "markdown"},
                    {"value", item.documentation}
                };
            if (!item.insertText.empty()) {
                ci["insertText"] = item.insertText;
                ci["insertTextFormat"] = static_cast<int>(item.insertTextFormat);
            }
            if (!item.sortText.empty())
                ci["sortText"] = item.sortText;
            if (!item.filterText.empty())
                ci["filterText"] = item.filterText;
            result.push_back(std::move(ci));
        }

        sendResponse(msg["id"], result);
    } catch (const std::exception& e) {
        std::cerr << "[lux-lsp] completion error: " << e.what() << "\n";
        sendResponse(msg["id"], json::array());
    } catch (...) {
        std::cerr << "[lux-lsp] completion unknown error\n";
        sendResponse(msg["id"], json::array());
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Signature Help
// ═══════════════════════════════════════════════════════════════════════

void LspServer::handleSignatureHelp(const json& msg) {
    try {
        auto& params = msg["params"];
        std::string uri = params["textDocument"]["uri"];
        size_t line = params["position"]["line"];
        size_t col  = params["position"]["character"];

        std::string source = documents_.get(uri);
        if (source.empty()) {
            sendResponse(msg["id"], nullptr);
            return;
        }

        std::string filePath = uri;
        if (filePath.rfind("file://", 0) == 0)
            filePath = filePath.substr(7);

        auto* cached = parseCache_.getOrParse(uri, source);
        auto result = signatureHelpProvider_.signatureHelp(
            source, line, col, filePath,
            projectContext_.isValid() ? &projectContext_ : nullptr,
            cached);

        if (!result) {
            sendResponse(msg["id"], nullptr);
            return;
        }

        json sigs = json::array();
        for (auto& si : result->signatures) {
            json params_arr = json::array();
            for (auto& pi : si.parameters) {
                json p = {{"label", pi.label}};
                params_arr.push_back(std::move(p));
            }
            json sigJson = {
                {"label", si.label},
                {"parameters", params_arr}
            };
            if (!si.documentation.empty()) {
                sigJson["documentation"] = {
                    {"kind", "markdown"},
                    {"value", si.documentation}
                };
            }
            sigs.push_back(std::move(sigJson));
        }

        json response = {
            {"signatures", sigs},
            {"activeSignature", result->activeSignature},
            {"activeParameter", result->activeParameter}
        };

        sendResponse(msg["id"], response);
    } catch (const std::exception& e) {
        std::cerr << "[lux-lsp] signatureHelp error: " << e.what() << "\n";
        sendResponse(msg["id"], nullptr);
    } catch (...) {
        std::cerr << "[lux-lsp] signatureHelp unknown error\n";
        sendResponse(msg["id"], nullptr);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Semantic Tokens
// ═══════════════════════════════════════════════════════════════════════

void LspServer::handleSemanticTokensFull(const json& msg) {
    try {
        auto& params = msg["params"];
        std::string uri = params["textDocument"]["uri"];

        std::string source = documents_.get(uri);
        if (source.empty()) {
            sendResponse(msg["id"], {{"data", json::array()}});
            return;
        }

        // Check semantic tokens cache first
        auto tokIt = cachedTokens_.find(uri);
        if (tokIt != cachedTokens_.end() && tokIt->second.source == source) {
            sendResponse(msg["id"], {{"data", tokIt->second.tokens}});
            return;
        }

        // Parse and tokenize, then cache
        auto* cached = parseCache_.getOrParse(uri, source);
        auto data = semanticTokensProvider_.tokenize(source, cached);
        cachedTokens_[uri] = {source, data};

        sendResponse(msg["id"], {{"data", data}});
    } catch (const std::exception& e) {
        std::cerr << "[lux-lsp] semantic tokens error: " << e.what() << "\n";
        sendResponse(msg["id"], {{"data", json::array()}});
    } catch (...) {
        std::cerr << "[lux-lsp] semantic tokens unknown error\n";
        sendResponse(msg["id"], {{"data", json::array()}});
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  JSON-RPC helpers
// ═══════════════════════════════════════════════════════════════════════

void LspServer::sendResponse(const json& id, const json& result) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    transport_.sendMessage(response.dump());
}

void LspServer::sendError(const json& id, int code,
                           const std::string& message) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    transport_.sendMessage(response.dump());
}

void LspServer::sendNotification(const std::string& method,
                                  const json& params) {
    json notification = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
    transport_.sendMessage(notification.dump());
}
