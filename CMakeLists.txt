cmake_minimum_required(VERSION 3.10)

project(project LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.c
)

add_executable(project ${SOURCES})

target_include_directories(project PRIVATE
    include
)

add_subdirectory(deps/glfw)
add_subdirectory(deps/glm)
add_subdirectory(deps/tbb)

target_link_libraries(project PRIVATE glfw)
target_link_libraries(project PRIVATE glm)
target_link_libraries(project PRIVATE tbb)