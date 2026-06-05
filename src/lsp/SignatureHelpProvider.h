#pragma once

#include <string>
#include <vector>
#include <optional>

#include "generated/LuxParser.h"
#include "types/BuiltinRegistry.h"
#include "types/TypeRegistry.h"
#include "types/MethodRegistry.h"
#include "types/ExtendedTypeRegistry.h"
#include "ffi/CBindings.h"
#include "lsp/DocComment.h"

struct ParseResult;
class ProjectContext;

// A single parameter in a signature.
struct ParameterInfo {
    std::string label;   // e.g. "int32 x"
};

// A single overloaded signature.
struct SignatureInfo {
    std::string            label;       // full signature: "int32 add(int32 a, int32 b)"
    std::string            documentation;  // markdown (includes doc-comment)
    std::vector<ParameterInfo> parameters;
    size_t                 activeParameter = 0;
};

// Result for a signatureHelp request.
struct SignatureHelpResult {
    std::vector<SignatureInfo> signatures;
    size_t activeSignature = 0;
    size_t activeParameter = 0;
};

// Provides signature help (parameter hints) for function and method calls.
class SignatureHelpProvider {
public:
    std::optional<SignatureHelpResult> signatureHelp(
        const std::string& source, size_t line, size_t col,
        const std::string& filePath, const ProjectContext* project,
        ParseResult* preParsed = nullptr);

private:
    BuiltinRegistry      builtinRegistry_;
    TypeRegistry         typeRegistry_;
    MethodRegistry       methodRegistry_;
    ExtendedTypeRegistry extTypeRegistry_;

    // ── Call-site analysis ───────────────────────────────────────────

    struct CallSite {
        std::string functionName;  // "printf", "add", or method name "push"
        std::string receiverType;  // non-empty for method calls
        std::string staticTypeName; // non-empty for static calls: "Point" in Point::create
        size_t      activeParam;   // which param the cursor is on (0-based)
        bool        isMethodCall;
        bool        isStaticCall;
    };

    // Analyze text around cursor to determine what function/method call we're in.
    std::optional<CallSite> analyzeCallSite(const std::string& source,
                                            size_t line, size_t col);

    // ── Signature builders ──────────────────────────────────────────

    // Build signature from a user-defined function declaration.
    SignatureInfo buildFromFunction(LuxParser::FunctionDeclContext* func,
                                   const std::vector<DocComment>& docs);

    // Build signature from an extern declaration.
    SignatureInfo buildFromExtern(LuxParser::ExternDeclContext* ext,
                                 const std::vector<DocComment>& docs);

    // Build signature from a builtin function.
    SignatureInfo buildFromBuiltin(const BuiltinSignature& sig);

    // Build signature from a C function binding.
    SignatureInfo buildFromCFunction(const CFunction& func);

    // Build signature from an extend method declaration.
    SignatureInfo buildFromExtendMethod(LuxParser::ExtendMethodContext* method,
                                       const std::string& structName,
                                       const std::vector<DocComment>& docs);

    // Build signature from an ExtendedTypeRegistry method descriptor.
    SignatureInfo buildFromExtMethod(const MethodDescriptor& md,
                                    const std::string& receiverType);

    // ── Helpers ─────────────────────────────────────────────────────

    static std::string typeSpecToString(LuxParser::TypeSpecContext* ctx);
    static std::string paramNameFromType(const std::string& type);
    static std::string resolveTypePlaceholder(const std::string& raw,
                                              const std::string& selfType,
                                              const std::string& elemType,
                                              const std::string& keyType,
                                              const std::string& valType);
};
