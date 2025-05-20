cmake_minimum_required(VERSION 3.8)

# ──────────────────────────────────────────────────────────────────────────────
# 1. Tree-sitter core
# ──────────────────────────────────────────────────────────────────────────────
add_library(treesitter STATIC
    tree-sitter/lib/src/lib.c
)
target_include_directories(treesitter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ──────────────────────────────────────────────────────────────────────────────
# 2. C / C++ grammars
# ──────────────────────────────────────────────────────────────────────────────
add_library(ts_c_objs   OBJECT 
    tree-sitter-c/src/parser.c)

add_library(ts_cpp_objs OBJECT
    tree-sitter-cpp/src/parser.c
    tree-sitter-cpp/src/scanner.c
)

add_library(treesitter_grammars STATIC
    $<TARGET_OBJECTS:ts_c_objs>
    $<TARGET_OBJECTS:ts_cpp_objs>
)
target_link_libraries(treesitter_grammars PUBLIC treesitter)

# ──────────────────────────────────────────────────────────────────────────────
# 3. LLVM / libclang (auto-detect DLL vs. static import-lib)
# ──────────────────────────────────────────────────────────────────────────────
set(LLVM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/LLVM")

set(_LIBCLANG_DLL "${LLVM_ROOT}/bin/libclang.dll")
set(_LIBCLANG_LIB "${LLVM_ROOT}/lib/libclang.lib")

if (EXISTS "${_LIBCLANG_DLL}")
    message(STATUS "Using shared libclang (${_LIBCLANG_DLL}).")
    add_library(libclang SHARED IMPORTED GLOBAL)
    set_target_properties(libclang PROPERTIES
        IMPORTED_LOCATION             "${_LIBCLANG_DLL}"
        IMPORTED_IMPLIB               "${_LIBCLANG_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LLVM_ROOT}/include"
    )
else()
    message(STATUS "libclang.dll not found – falling back to static import library.")
    add_library(libclang STATIC IMPORTED GLOBAL)
    set_target_properties(libclang PROPERTIES
        IMPORTED_LOCATION             "${_LIBCLANG_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LLVM_ROOT}/include"
    )
endif()

# Make sure multi-config generators (VS/Xcode) are satisfied
set_property(TARGET libclang PROPERTY IMPORTED_CONFIGURATIONS
             DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
