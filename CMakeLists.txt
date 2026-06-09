cmake_minimum_required(VERSION 3.20)
project(lux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LUX_ENABLE_SANITIZERS
    "Enable ASan/UBSan in debug-like builds"
    ON)

option(LUX_PREFER_PKG_CONFIG
    "Prefer pkg-config for third-party dependency discovery when available"
    ON)

find_package(PkgConfig QUIET)

# ─── LLVM ─────────────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Detect whether LLVM was installed as a monolithic shared library (e.g. LLVM 15+
# on Debian/Ubuntu ships as libLLVM-<ver>.so instead of individual component .a files).
execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --libs core support irreader passes native nativecodegen
                              mc mcparser target analysis transformutils scalaropts
                              instcombine ipo linker bitwriter
    OUTPUT_VARIABLE LLVM_LIBS_RAW_FROM_TOOLS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(LLVM_LIBS_RAW_FROM_TOOLS_DIR)
    set(LLVM_LIBS_RAW "${LLVM_LIBS_RAW_FROM_TOOLS_DIR}")
else()
    find_program(LLVM_CONFIG_EXECUTABLE
        NAMES
            llvm-config
            llvm-config-${LLVM_VERSION_MAJOR}
        HINTS
            ${LLVM_TOOLS_BINARY_DIR}
        PATH_SUFFIXES
            bin
    )

    if(LLVM_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs core support irreader passes native nativecodegen
                                            mc mcparser target analysis transformutils scalaropts
                                            instcombine ipo linker bitwriter
            OUTPUT_VARIABLE LLVM_LIBS_RAW
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
endif()

if(LLVM_LIBS_RAW MATCHES "^-lLLVM-")
    # Monolithic build — link against the single shared lib
    separate_arguments(LLVM_LIBS UNIX_COMMAND "${LLVM_LIBS_RAW}")
    message(STATUS "LLVM is monolithic: ${LLVM_LIBS}")
else()
    # Component build — use the CMake helper
    llvm_map_components_to_libnames(LLVM_LIBS
        core support irreader passes native nativecodegen
        mc mcparser target analysis transformutils scalaropts
        instcombine ipo linker bitwriter
    )
    message(STATUS "LLVM component libs: ${LLVM_LIBS}")
endif()

# ─── ANTLR4 Runtime ───────────────────────────────────────────────────────────
if(LUX_PREFER_PKG_CONFIG AND PkgConfig_FOUND)
    pkg_check_modules(ANTLR4_PC QUIET antlr4-runtime)
    if(ANTLR4_PC_FOUND)
        if(ANTLR4_PC_INCLUDE_DIRS)
            set(ANTLR4_INCLUDE_DIR ${ANTLR4_PC_INCLUDE_DIRS})
        endif()
        if(ANTLR4_PC_LINK_LIBRARIES)
            set(ANTLR4_LIB ${ANTLR4_PC_LINK_LIBRARIES})
        elseif(ANTLR4_PC_LIBRARIES)
            set(ANTLR4_LIB ${ANTLR4_PC_LIBRARIES})
        endif()
    endif()
endif()

if(NOT ANTLR4_INCLUDE_DIR)
    find_path(ANTLR4_INCLUDE_DIR
        NAMES antlr4-runtime.h
        PATHS
            /usr/include/antlr4-runtime
            /usr/local/include/antlr4-runtime
            /opt/local/include/antlr4-runtime
            /usr/include
            /usr/local/include
    )
endif()

if(NOT ANTLR4_LIB)
    find_library(ANTLR4_LIB
        NAMES
            antlr4-runtime
            antlr4-runtime4
        HINTS
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib64
            /opt/local/lib
            /usr/lib/llvm-${LLVM_VERSION_MAJOR}/lib
    )
endif()

if(NOT ANTLR4_INCLUDE_DIR OR NOT ANTLR4_LIB)
    message(FATAL_ERROR
        "ANTLR4 C++ runtime not found.\n"
        "Tried pkg-config and common system paths.\n"
        "Please install ANTLR4 runtime development files and/or set CMAKE_PREFIX_PATH."
    )
endif()
message(STATUS "Found ANTLR4: ${ANTLR4_LIB}")

# ─── libclang (C API for parsing C headers) ───────────────────────────────────
if(LUX_PREFER_PKG_CONFIG AND PkgConfig_FOUND)
    pkg_check_modules(LIBCLANG_PC QUIET libclang)
    if(LIBCLANG_PC_FOUND)
        if(LIBCLANG_PC_INCLUDE_DIRS)
            set(LIBCLANG_INCLUDE_DIR ${LIBCLANG_PC_INCLUDE_DIRS})
        endif()
        if(LIBCLANG_PC_LINK_LIBRARIES)
            set(LIBCLANG_LIB ${LIBCLANG_PC_LINK_LIBRARIES})
        elseif(LIBCLANG_PC_LIBRARIES)
            set(LIBCLANG_LIB ${LIBCLANG_PC_LIBRARIES})
        endif()
    endif()
endif()

if(NOT LIBCLANG_INCLUDE_DIR)
    find_path(LIBCLANG_INCLUDE_DIR
        NAMES clang-c/Index.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/local/include
            /usr/lib/llvm-${LLVM_VERSION_MAJOR}/include
            /usr/lib64/llvm/include
    )
endif()

if(NOT LIBCLANG_LIB)
    find_library(LIBCLANG_LIB
        NAMES
            clang
            libclang
            clang-${LLVM_VERSION_MAJOR}
            libclang-${LLVM_VERSION_MAJOR}
        HINTS
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib64
            /opt/local/lib
            /usr/lib/llvm-${LLVM_VERSION_MAJOR}/lib
            /usr/lib64/llvm-${LLVM_VERSION_MAJOR}/lib
            ${LLVM_LIBRARY_DIRS}
    )
endif()

if(NOT LIBCLANG_INCLUDE_DIR OR NOT LIBCLANG_LIB)
    message(FATAL_ERROR
        "libclang not found.\n"
        "Tried pkg-config and common system paths.\n"
        "Please install libclang development files and/or set CMAKE_PREFIX_PATH."
    )
endif()
message(STATUS "Found libclang: ${LIBCLANG_LIB}")

# ─── Builtins (C library linked into every compiled binary) ───────────────────
add_library(lux_builtins STATIC
    src/builtins/log/log.c
    src/builtins/collections/vec.c
    src/builtins/collections/map.c
    src/builtins/collections/set.c
    src/builtins/global/global.c
    src/builtins/io/io.c
    src/builtins/math/math.c
    src/builtins/string/string.c
    src/builtins/mem/mem.c
    src/builtins/random/random.c
    src/builtins/time/time.c
    src/builtins/fs/fs.c
    src/builtins/process/process.c
    src/builtins/conv/conv.c
    src/builtins/hash/hash.c
    src/builtins/bits/bits.c
    src/builtins/char/char.c
    src/builtins/path/path.c
    src/builtins/fmt/fmt.c
    src/builtins/regex/regex.c
    src/builtins/encoding/encoding.c
    src/builtins/os/os.c
    src/builtins/test/test.c
    src/builtins/crypto/crypto.c
    src/builtins/compress/compress.c
    src/builtins/net/net.c
    src/builtins/thread/thread.c
    src/builtins/error/error.c
)
set_target_properties(lux_builtins PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lux_builtins PRIVATE
    ${CMAKE_SOURCE_DIR}/src/builtins
)
target_link_libraries(lux_builtins PRIVATE z pthread)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(GENERATED_SOURCES
    src/generated/LuxLexer.cpp
    src/generated/LuxParser.cpp
    src/generated/LuxParserBaseVisitor.cpp
    src/generated/LuxParserVisitor.cpp
)

set(SOURCES
    src/main.cpp
    src/cli/CLI.cpp
    src/cli/ArgParser.cpp
    src/cli/LuxPipeline.cpp
    src/cli/BuildCommand.cpp
    src/cli/RunCommand.cpp
    src/cli/CheckCommand.cpp
    src/cli/TestCommand.cpp
    src/cli/HelpCommand.cpp
    src/parser/Parser.cpp
    src/checkers/Checker.cpp
    src/IRBuilder/IRGen.cpp
    src/LLVM_IR/IRModule.cpp
    src/LLVM_Optimizer/Optimizer.cpp
    src/machine_code/CodeGen.cpp
    src/imports/ImportResolver.cpp
    src/namespace/ProjectScanner.cpp
    src/namespace/NamespaceRegistry.cpp
    src/types/TypeInfo.cpp
    src/types/TypeRegistry.cpp
    src/types/MethodRegistry.cpp
    src/types/ExtendedTypeRegistry.cpp
    src/types/BuiltinRegistry.cpp
    src/intrinsics/IntrinsicRegistry.cpp
    src/intrinsics/namespaces/CoreNamespace.cpp
    src/intrinsics/namespaces/DebugNamespace.cpp
    src/ffi/CBindings.cpp
    src/ffi/CTypeMapper.cpp
    src/ffi/CHeaderResolver.cpp
    src/ffi/ABIInfo.cpp
    src/helpc/HelpC.cpp
    src/helpc/HelpCParser.cpp
    src/helpc/HelpCFormatter.cpp
    src/helpc/HelpCSearch.cpp
    src/lsp/LspTransport.cpp
    src/lsp/LspServer.cpp
    src/lsp/DocumentStore.cpp
    src/lsp/DiagnosticEngine.cpp
    src/lsp/HoverProvider.cpp
    src/lsp/DefinitionProvider.cpp
    src/lsp/CompletionProvider.cpp
    src/lsp/DocComment.cpp
    src/lsp/ParseCache.cpp
    src/lsp/SignatureHelpProvider.cpp
    src/lsp/SemanticTokensProvider.cpp
    src/lsp/ProjectContext.cpp
)

# ─── Target ───────────────────────────────────────────────────────────────────
add_executable(lux ${SOURCES} ${GENERATED_SOURCES})

target_include_directories(lux PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/generated
    ${CMAKE_SOURCE_DIR}/src/builtins
    ${ANTLR4_INCLUDE_DIR}
    ${LIBCLANG_INCLUDE_DIR}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(lux PRIVATE
        -include ${CMAKE_SOURCE_DIR}/src/antlr/AntlrRuntimeCompat.h
    )
endif()

# Compatibility flags for LLVM API differences across versions
if(LLVM_VERSION_MAJOR GREATER_EQUAL 15)
    target_compile_definitions(lux PRIVATE LLVM_VERSION_15_OR_NEWER)
endif()
if(LLVM_VERSION_MAJOR GREATER_EQUAL 18)
    target_compile_definitions(lux PRIVATE LLVM_VERSION_18_OR_NEWER)
endif()

target_link_libraries(lux PRIVATE
    # --whole-archive forces ALL symbols from lux_builtins into the binary so that
    # LLVM MCJIT can resolve them at JIT runtime via dlsym (lux run command).
    -Wl,--whole-archive lux_builtins -Wl,--no-whole-archive
    ${ANTLR4_LIB}
    ${LLVM_LIBS}
    ${LIBCLANG_LIB}
)

# Export all symbols so LLVM MCJIT can resolve builtins via dlsym (lux run command).
target_link_options(lux PRIVATE -rdynamic)

# ─── Debug runtime diagnostics (Phase 4) ─────────────────────────────────────
if(LUX_ENABLE_SANITIZERS
   AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"
   AND CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
    set(LUX_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)

    target_compile_options(lux PRIVATE ${LUX_SANITIZER_FLAGS})
    target_link_options(lux PRIVATE ${LUX_SANITIZER_FLAGS})

    target_compile_options(lux_builtins PRIVATE ${LUX_SANITIZER_FLAGS})

    target_compile_definitions(lux PRIVATE LUX_RUNTIME_DIAGNOSTICS=1)
    message(STATUS "Lux debug runtime diagnostics enabled (ASan/UBSan)")
endif()

# ─── Build instructions ───────────────────────────────────────────────────────
# mkdir build && cd build && cmake .. && make -j$(nproc)
# ./lux ../examples/main.lx
# ./lux ../examples/main.lx ./out -o2
