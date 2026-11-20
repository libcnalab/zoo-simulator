# SPDX-License-Identifier: MIT
if(NOT TARGET GTest::gtest_main)
    if(NOT EXISTS "${ZOO_GOOGLETEST_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "No GoogleTest at '${ZOO_GOOGLETEST_DIR}'. Set -DZOO_GOOGLETEST_DIR=... or -DZOO_BUILD_TESTS=OFF.")
    endif()
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ZOO_GOOGLETEST_DIR}" "${CMAKE_BINARY_DIR}/_googletest" EXCLUDE_FROM_ALL)
endif()
include(GoogleTest)

file(GLOB_RECURSE ZOO_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp")
add_executable(zoo_core_tests ${ZOO_TEST_SOURCES})
target_link_libraries(zoo_core_tests PRIVATE Zoo::Core zoo_warnings GTest::gtest_main)
target_include_directories(zoo_core_tests SYSTEM PRIVATE "${ZOO_NLOHMANN_JSON_INCLUDE}")
target_compile_definitions(zoo_core_tests PRIVATE ZOO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(zoo_core_tests DISCOVERY_TIMEOUT 60)

# The player's body (src/game/Walker.cpp) needs no window: the simulation and cna-engine's
# collision module carry it, so its walks are tested like the core.
if(ZOO_BUILD_GAME AND TARGET CnaEngine::Core)
    file(GLOB ZOO_GAME_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp")
    # The walker stands on the ground as it is drawn (GroundPaint.cpp: no device, pure maths).
    add_executable(zoo_game_tests ${ZOO_GAME_TEST_SOURCES} "${CMAKE_SOURCE_DIR}/src/game/Walker.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/GroundPaint.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/AnimalWater.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/AnimalTextures.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/KeeperAppearance.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/MeshWriter.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/FoliageTextures.cpp"
                                  "${CMAKE_SOURCE_DIR}/src/game/PlantModels.cpp")
    target_link_libraries(zoo_game_tests PRIVATE Zoo::Core zoo_warnings CnaEngine::Graphics CNA::GamerServices
                                                 GTest::gtest_main)
    target_include_directories(zoo_game_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/game" "${CMAKE_CURRENT_SOURCE_DIR}/core")
    target_compile_definitions(zoo_game_tests PRIVATE ZOO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Texture tests create a GraphicsDevice. Keep them runnable on headless builders instead
        # of relying on a parent shell to have selected SDL's offscreen video backend.
        gtest_discover_tests(zoo_game_tests DISCOVERY_TIMEOUT 60
                            PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
    else()
        gtest_discover_tests(zoo_game_tests DISCOVERY_TIMEOUT 60)
    endif()
endif()

# The procedural animation maths of the asset pipeline is pure numpy and tested without Blender.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy"
                    RESULT_VARIABLE ZOO_NUMPY_MISSING OUTPUT_QUIET ERROR_QUIET)
    if(ZOO_NUMPY_MISSING EQUAL 0)
        add_test(NAME zoogen_animation
                 COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_SOURCE_DIR}/tools/animals/tests")
    else()
        message(STATUS "numpy not found: the zoogen animation tests are skipped")
    endif()
endif()
