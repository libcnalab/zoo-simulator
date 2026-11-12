# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)
project(zoo_simulator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CNA is a shared library on Linux (libcna.so) and links its consumers with -z nocopyreloc,
# so every object that ends up in the executable must be position independent, including
# the cna-engine archives and zoo_core.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(ZOO_BUILD_GAME "Build the CNA-powered game executable (needs cna-engine, CNA, Sharp Runtime)" ON)
option(ZOO_BUILD_TESTS "Build the automated tests" ON)
option(ZOO_WARNINGS_AS_ERRORS "Treat warnings in Zoo Simulator's own code as errors" ON)
set(ZOO_SANITIZE "" CACHE STRING "Sanitizers for Zoo Simulator's own targets: empty, address, undefined, thread")

# The CNA ecosystem. Zoo Simulator lives in .../openeggbert/zoo-simulator, the
# libraries in .../libcna/<name>; every path can be overridden.
set(ZOO_LIBCNA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../libcna" CACHE PATH "Directory holding cna, cna-engine, sharp-runtime, easy-gl")
set(CNA_ENGINE_ROOT "${ZOO_LIBCNA_ROOT}/cna-engine" CACHE PATH "cna-engine checkout")
set(ZOO_SHARP_RUNTIME_ROOT "${ZOO_LIBCNA_ROOT}/sharp-runtime" CACHE PATH "Sharp Runtime checkout (also provides vendored GoogleTest and nlohmann/json)")
set(ZOO_NLOHMANN_JSON_INCLUDE "${ZOO_SHARP_RUNTIME_ROOT}/vendor" CACHE PATH "Directory containing nlohmann/json.hpp")
set(ZOO_GOOGLETEST_DIR "${ZOO_SHARP_RUNTIME_ROOT}/vendor/googletest" CACHE PATH "GoogleTest source tree")

add_library(zoo_warnings INTERFACE)
target_compile_options(zoo_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
if(ZOO_WARNINGS_AS_ERRORS)
    target_compile_options(zoo_warnings INTERFACE -Werror)
endif()
add_library(zoo_sanitizers INTERFACE)
if(ZOO_SANITIZE)
    target_compile_options(zoo_sanitizers INTERFACE -fsanitize=${ZOO_SANITIZE} -fno-omit-frame-pointer)
    target_link_options(zoo_sanitizers INTERFACE -fsanitize=${ZOO_SANITIZE})
endif()

add_subdirectory(src/core)

if(ZOO_BUILD_GAME)
    set(CNA_ENGINE_WITH_ENHANCED_GRAPHICS ON CACHE BOOL "cna-engine PBR, shadows, glTF helpers")
    # Visitors are CNA avatars: they live in Gamer Services, which works
    # offline and does not need the ENet networking module.
    set(CNA_ENABLE_GAMER_SERVICES ON CACHE BOOL "CNA avatars (visitors)")
    set(CNA_ENABLE_NET OFF CACHE BOOL "CNA networking (unused)")
    set(CNA_ENABLE_DRACO OFF CACHE BOOL "Draco mesh compression (unused)")
    set(CNA_BUILD_TESTS OFF CACHE BOOL "CNA tests")
    set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "CNA examples")
    set(CNA_BUILD_C_API OFF CACHE BOOL "CNA C API")
    set(CNA_DEBUG_CONTEXT_HOTKEYS OFF CACHE STRING "F9/F10 context-loss hotkeys")
    if(NOT EMSCRIPTEN AND NOT ANDROID)
        set(CNA_ENGINE_CNA_RENDERER OPENGL33 CACHE STRING "CNA renderer")
    endif()
    include("${CNA_ENGINE_ROOT}/cmake/CnaEngineGame.cmake")
    add_subdirectory(src/game)
endif()

if(ZOO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
