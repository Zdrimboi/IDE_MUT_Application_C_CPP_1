# CMakeList.txt : CMake project for mut, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable
add_executable(mut "mut.cpp")

# Include directories
target_include_directories(mut PRIVATE
    ${CMAKE_SOURCE_DIR}/Include
    ${CMAKE_SOURCE_DIR}/Include/GLFW
    ${CMAKE_SOURCE_DIR}/Include/imgui
)

# Link directories
target_link_directories(mut PRIVATE ${CMAKE_SOURCE_DIR}/lib/GLFW)

# Link libraries
target_link_libraries(mut PRIVATE
    glfw3.lib
    opengl32.lib # OpenGL is provided by the system on Windows
)

# Add ImGui source files
target_sources(mut PRIVATE
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_impl_opengl3.cpp
)

# Copy DLLs to the output directory
add_custom_command(TARGET mut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/dll/glfw3.dll $<TARGET_FILE_DIR:mut>
)