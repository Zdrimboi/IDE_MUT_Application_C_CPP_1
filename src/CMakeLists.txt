# CMakeList.txt : CMake project for mut, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable
add_executable(mut "main.cpp")

# ──────────────────────────────────────────────────────────────────────────────
# Include / link paths
# ──────────────────────────────────────────────────────────────────────────────
target_include_directories(mut PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/GLFW
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/glad
    ${CMAKE_SOURCE_DIR}/third_party/KHR
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
)

# Add ImGui source files
target_sources(mut PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/third_party/glad/glad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/platform_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/dpi_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/gui_layer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/clang_indexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/syntax_highlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/editor_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editor/text_editor.cpp
    )

target_link_directories(mut PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/GLFW
)

target_link_libraries(mut PRIVATE
    glfw3.lib
    opengl32.lib
    Shcore.lib
    treesitter_grammars
    libclang
)

# ──────────────────────────────────────────────────────────────────────────────
# Post-build: copy runtime DLLs
# ──────────────────────────────────────────────────────────────────────────────
# GLFW is always present
add_custom_command(TARGET mut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/third_party/GLFW/glfw3.dll"
            "$<TARGET_FILE_DIR:mut>"
)

# Only copy libclang when the imported target is a SHARED library
get_target_property(_LIBCLANG_TYPE libclang TYPE)
if (_LIBCLANG_TYPE STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET mut POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:libclang>"
                "$<TARGET_FILE_DIR:mut>"
    )
endif()
