cmake_minimum_required(VERSION 3.8)

# ---------- 1. Tree-sitter core ----------
add_library(treesitter STATIC
    tree-sitter/lib/src/lib.c
)
target_include_directories(treesitter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include)

# ---------- 2. C + C++ grammars ----------
add_library(ts_c_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-c/src/parser.c)
add_library(ts_cpp_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src/scanner.c)

add_library(treesitter_grammars STATIC
    $<TARGET_OBJECTS:ts_c_objs>
    $<TARGET_OBJECTS:ts_cpp_objs>)
target_link_libraries(treesitter_grammars PUBLIC treesitter)

# third_party/CMakeLists.txt
set(LLVM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/LLVM)

# note the extra keyword  GLOBAL
add_library(libclang SHARED IMPORTED GLOBAL)

# single-config path (Ninja/Make)
set_target_properties(libclang PROPERTIES
    IMPORTED_LOCATION  "${LLVM_ROOT}/bin/libclang.dll"
    IMPORTED_IMPLIB    "${LLVM_ROOT}/lib/libclang.lib")

# multi-config paths (Visual Studio / Xcode)
set_property(TARGET libclang PROPERTY IMPORTED_CONFIGURATIONS
             DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)

foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(libclang PROPERTIES
        IMPORTED_LOCATION_${cfg}  "${LLVM_ROOT}/bin/libclang.dll"
        IMPORTED_IMPLIB_${cfg}    "${LLVM_ROOT}/lib/libclang.lib")
endforeach()

set_target_properties(libclang PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LLVM_ROOT}/include")
