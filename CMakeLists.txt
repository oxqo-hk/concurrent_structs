cmake_minimum_required(VERSION 3.12)

project(concurrent_structs VERSION 0.1 LANGUAGES C)

option(TIDY "check with clang-tidy" OFF)

find_program(CLANG_EXISTS clang)
if(CLAG_EXISTS)
    set(CMAKE_C_COMPILER clang)
else()
    set(CMAKE_C_COMILER gcc)
endif()

add_compile_options(-g)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out)

if(TIDY)
    find_program(CLANG_TIDY_EXISTS clang-tidy)
    if(CLANG_TIDY_EXISTS)
        set(CMAKE_C_CLANG_TIDY "clang-tidy;")
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY}-checks=*)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-llvmlibc-restrict-system-libc-headers)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-readability-*)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-altera-*)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-bugprone-easily-swappable-parameters)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-cppcoreguidelines-init-variables)
        set(CMAKE_C_CLANG_TIDY ${CMAKE_C_CLANG_TIDY},-llvm-include-order)
    endif()
endif()

set(CMAKE_C_CLANG_TIDY_WHITELIST ${CMAKE_SOURCE_DIR}/.*)

add_subdirectory(src)
add_subdirectory(test)