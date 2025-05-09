# CMakeList.txt : CMake project for mut, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
# Add source to this project's executable.
add_executable (mut "mut.cpp" )

find_package(glad CONFIG REQUIRED)
target_link_libraries(mut PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(mut PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(mut PRIVATE imgui::imgui)

find_package(OpenGL REQUIRED)
target_link_libraries(mut PRIVATE OpenGL::GL)
   


# TODO: Add tests and install targets if needed.
