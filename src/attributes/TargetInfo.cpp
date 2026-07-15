#include "attributes/TargetInfo.h"
#include "attributes/CfgPredicate.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

TargetInfo::TargetInfo()
    : pointerWidth(64)
    , debug(false)
{
}

static std::string toLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = char(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

TargetInfo::TargetInfo(const std::string& tripleStr, bool debug)
    : endian("little")
    , pointerWidth(64)
    , osFamily("unix")
    , triple(tripleStr)
    , debug(debug)
{
    // Parse the LLVM triple format: arch-vendor-os-environment
    // e.g. "x86_64-unknown-linux-gnu", "aarch64-apple-macosx11.0"
    auto parts = tripleStr;
    // Extract arch (first component before first '-')
    auto firstDash = parts.find('-');
    if (firstDash != std::string::npos) {
        arch = parts.substr(0, firstDash);
        parts = parts.substr(firstDash + 1);

        // Skip vendor (second component)
        auto secondDash = parts.find('-');
        if (secondDash != std::string::npos) {
            parts = parts.substr(secondDash + 1);

            // Extract OS (third component)
            auto thirdDash = parts.find('-');
            std::string osPart;
            if (thirdDash != std::string::npos) {
                osPart = parts.substr(0, thirdDash);
            } else {
                osPart = parts;
            }

            // Normalize OS names
            auto lower = toLower(osPart);
            if (lower == "linux" || lower == "linux-gnu")
                os = "linux";
            else if (lower == "win32" || lower == "windows" || lower == "mingw32" || lower == "mingw")
                os = "windows";
            else if (lower == "darwin" || lower == "macos" || lower == "macosx")
                os = "macos";
            else if (lower == "freebsd")
                os = "freebsd";
            else if (lower == "openbsd")
                os = "openbsd";
            else if (lower == "netbsd")
                os = "netbsd";
            else
                os = lower;
        }
    }

    if (os == "windows")
        osFamily = "windows";

    // Normalize arch names
    auto archLower = toLower(arch);
    if (archLower == "amd64" || archLower == "x86_64" || archLower == "x64")
        arch = "x86_64";
    else if (archLower == "aarch64" || archLower == "arm64")
        arch = "aarch64";
    else if (archLower == "i386" || archLower == "i486" || archLower == "i586" || archLower == "i686" || archLower == "x86")
        arch = "x86";

    // Detect pointer width from arch
    if (arch == "x86_64" || arch == "aarch64" || arch.find("64") != std::string::npos)
        pointerWidth = 64;
    else
        pointerWidth = 32;
}

std::string TargetInfo::get(const std::string& key) const {
    if (key == "target_os")             return os;
    if (key == "target_arch")           return arch;
    if (key == "target_endian")         return endian;
    if (key == "target_pointer_width")  return std::to_string(pointerWidth);
    if (key == "target_os_family")      return osFamily;
    if (key == "target_triple")         return triple;
    if (key == "debug_assertions")      return debug ? "true" : "false";
    return {};
}

bool TargetInfo::matches(const CfgPredicate& pred) const {
    return pred.evaluate(*this);
}
