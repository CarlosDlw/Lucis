#pragma once

#include <string>

// A #[fuzz] target collected by the checker and carried through to IRGen.
struct FuzzTarget {
    std::string modulePath;
    std::string funcName;
    std::string symbolName;    // resolved emit name (filled by LucisPipeline)
    std::string filePath;
    int line = 0;
    int col = 0;
};