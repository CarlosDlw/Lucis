#pragma once

#include <string>
#include "ffi/CBindings.h"

// Evaluate a c_macro block's raw C text using gcc's preprocessor.
// Populates bindings with CMacro and CFunctionLikeMacro entries.
//
// tempDir     - directory for temp files (e.g. ".lucis/cmacro/" or "/tmp").
// sourceFile  - the .lc source file path (for diagnostics).
// lineNumber  - line number of the c_macro block in the source file.
// evalValues  - when true, also compiles & runs to extract integer values.
// Returns true on success (even if some macros couldn't be evaluated).
bool evalCMacroRaw(const std::string& rawC,
                   const std::string& sourceFile,
                   unsigned lineNumber,
                   const std::string& tempDir,
                   CBindings& bindings,
                   bool evalValues = true);
