#pragma once

// Compatibility shim for ANTLR C++ runtime variants.
// Some generated files from newer ANTLR toolchains use antlr4::internal::OnceFlag
// and antlr4::internal::call_once, but older runtime packages do not expose them.

#if !__has_include(<antlr4-runtime/internal/Synchronization.h>)
#include <mutex>
#include <utility>

namespace antlr4 {
namespace internal {

using OnceFlag = std::once_flag;

template <typename Callable, typename... Args>
inline void call_once(OnceFlag& flag, Callable&& callable, Args&&... args) {
    std::call_once(flag,
                   std::forward<Callable>(callable),
                   std::forward<Args>(args)...);
}

} // namespace internal
} // namespace antlr4

#endif
