#pragma once

#include <string>
#include <vector>

// Structured diagnostic message compatible with LSP.
struct Diagnostic {
    enum Severity {
        Error   = 1,
        Warning = 2,
        Info    = 3,
        Hint    = 4
    };

    size_t    line     = 0;  // 0-based
    size_t    col      = 0;  // 0-based
    size_t    endLine  = 0;  // 0-based (same as line if unknown)
    size_t    endCol   = 0;  // 0-based (same as col if unknown)
    Severity  severity = Error;
    std::string message;
    std::string source = "lucis";
    std::string code;
};
