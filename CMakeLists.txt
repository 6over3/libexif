cmake_minimum_required(VERSION 3.14)
project(libexif C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# WAMR tunables
set(WAMR_BUILD_AOT 1 CACHE STRING "Enable AOT execution")
set(WAMR_BUILD_LIBC_BUILTIN 1 CACHE STRING "Enable built-in libc")
set(WAMR_BUILD_LIBC_WASI 1 CACHE STRING "Enable WASI libc")

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
if(APPLE)
    add_definitions(-DBH_PLATFORM_DARWIN)
endif()

if(NOT DEFINED WAMR_BUILD_TARGET)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
        set(WAMR_BUILD_TARGET "AARCH64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WAMR_BUILD_TARGET "X86_64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(WAMR_BUILD_TARGET "X86_32")
    else()
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

set(WAMR_ROOT_DIR ${CMAKE_SOURCE_DIR}/vendor/wamr)
include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
add_library(vmlib OBJECT ${WAMR_RUNTIME_LIB_SOURCE})

set(PLATFORM_LIBS -lm -lpthread)
if(NOT APPLE)
    list(APPEND PLATFORM_LIBS -ldl -lrt)
endif()

set(STRICT_C_FLAGS -Wall -Wextra -Wpedantic -Werror)

# libexif — single static lib with WAMR baked in
add_library(exif STATIC libexif.c $<TARGET_OBJECTS:vmlib>)
target_include_directories(exif
    PUBLIC  ${CMAKE_SOURCE_DIR}
    PRIVATE ${WAMR_ROOT_DIR}/core/iwasm/include)
target_compile_options(exif PRIVATE ${STRICT_C_FLAGS} -fvisibility=hidden)
target_compile_definitions(exif PRIVATE EXIF_BUILD EXIF_SHARED)

# Demo
add_executable(exif_demo demo.c)
target_compile_options(exif_demo PRIVATE ${STRICT_C_FLAGS})
target_link_libraries(exif_demo PRIVATE exif ${PLATFORM_LIBS})

# Benchmark
add_executable(exif_bench bench.c)
target_compile_options(exif_bench PRIVATE ${STRICT_C_FLAGS})
target_link_libraries(exif_bench PRIVATE exif ${PLATFORM_LIBS})

# Tests
enable_testing()
add_executable(exif_test test_exif.c)
target_compile_definitions(exif_test PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(exif_test PRIVATE ${STRICT_C_FLAGS})
target_link_libraries(exif_test PRIVATE exif ${PLATFORM_LIBS})
add_test(NAME exif_test COMMAND exif_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
