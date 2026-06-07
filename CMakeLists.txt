cmake_minimum_required(VERSION 3.15)
project(grug C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use: cmake -S . -B build")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SOURCES "src/*.c" "src/algorithms/*.c")
add_executable(grug ${SOURCES})

if(MSVC)
    target_compile_options(grug PRIVATE /O2 /W4)
else()
    target_compile_options(grug PRIVATE -O2 -Wall -Wextra)
    # Uncomment for a host-tuned build:
    # target_compile_options(grug PRIVATE -march=native)
endif()
