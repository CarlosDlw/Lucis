#pragma once

#include <string>

struct CfgPredicate;

struct TargetInfo {
    std::string os;           // "linux", "windows", "macos", "freebsd"
    std::string arch;         // "x86_64", "aarch64", "riscv64", "x86", "arm"
    std::string endian;       // "little", "big"
    int pointerWidth;         // 32 or 64
    std::string osFamily;     // "unix", "windows"
    std::string triple;       // full target triple string
    bool debug;               // debug assertions enabled

    TargetInfo();

    // Build from an LLVM target triple string (e.g. "x86_64-unknown-linux-gnu").
    explicit TargetInfo(const std::string& tripleStr, bool debug = false);

    // Look up a target constant by name (e.g. "target_os", "target_arch").
    std::string get(const std::string& key) const;

    // Evaluate a single predicate against this target.
    bool matches(const CfgPredicate& pred) const;
};
