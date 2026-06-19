#pragma once

#include "lsp/LspTransport.h"
#include "lsp/DocumentStore.h"
#include "lsp/DiagnosticEngine.h"
#include "lsp/HoverProvider.h"
#include "lsp/DefinitionProvider.h"
#include "lsp/CompletionProvider.h"
#include "lsp/SemanticTokensProvider.h"
#include "lsp/SignatureHelpProvider.h"
#include "lsp/ProjectContext.h"
#include "lsp/ParseCache.h"

#include <stdio.h>
#ifndef EOF
#define EOF (-1)
#endif
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

class LspServer {
public:
    LspServer();

    // Main loop — blocks until shutdown or EOF.
    int run();

private:
    LspTransport     transport_;
    DocumentStore    documents_;
    DiagnosticEngine diagnosticEngine_;
    HoverProvider      hoverProvider_;
    DefinitionProvider definitionProvider_;
    CompletionProvider   completionProvider_;
    SemanticTokensProvider semanticTokensProvider_;
    SignatureHelpProvider signatureHelpProvider_;
    ProjectContext   projectContext_;
    ParseCache       parseCache_;

    TypeRegistry      typeRegistry_;
    IntrinsicRegistry intrinsicRegistry_;

    // Semantic tokens cache: URI → {source, tokens}
    struct CachedTokens {
        std::string source;
        std::vector<uint32_t> tokens;
    };
    std::unordered_map<std::string, CachedTokens> cachedTokens_;

    bool             initialized_ = false;
    bool             shutdown_    = false;

    // Project root provided by the LSP client in the initialize request.
    std::string      rootPath_;

    // Dispatch a parsed JSON-RPC message.
    void dispatch(const json& msg);

    // LSP lifecycle
    void handleInitialize(const json& msg);
    void handleInitialized(const json& msg);
    void handleShutdown(const json& msg);

    // Document sync
    void handleDidOpen(const json& msg);
    void handleDidChange(const json& msg);
    void handleDidSave(const json& msg);
    void handleDidClose(const json& msg);

    // Language features
    void handleHover(const json& msg);
    void handleDefinition(const json& msg);
    void handleCompletion(const json& msg);
    void handleSemanticTokensFull(const json& msg);
    void handleSignatureHelp(const json& msg);

    // Publish diagnostics for a document.
    void publishDiagnostics(const std::string& uri, const std::string& source);

    // Find the canonical "main" file in a project root —
    // the file with `namespace Main` and a top-level `main` function.
    // Returns empty string if not found.
    static std::string findMainFile(const std::string& projectRoot);

    // Ensures projectContext_ is built, always anchored to the main file.
    // Falls back to filePath if no main file can be found.
    // When force=false, skips expensive full rebuild if the active context
    // already matches the same project root/anchor.
    void rebuildContext(const std::string& filePath, bool force = false);

    // Helpers
    void sendResponse(const json& id, const json& result);
    void sendError(const json& id, int code, const std::string& message);
    void sendNotification(const std::string& method, const json& params);

    // Cached path of the project's main file (namespace Main + main()).
    std::string mainFilePath_;

    // Cached context identity to avoid redundant full project rebuilds.
    std::string contextProjectRoot_;
    std::string contextAnchorPath_;
};
