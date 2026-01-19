cmake_minimum_required(VERSION 3.10)

project(project LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.c
    third_party/*.cpp
    third_party/*.c
)

add_executable(project ${SOURCES})

target_include_directories(project PRIVATE
    include
    third_party
)
