# SPDX-License-Identifier: MIT
#
# The game: CNA and cna-engine on top of zoo_core.
file(GLOB_RECURSE ZOO_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
cna_engine_add_game(zoo-simulator
    SOURCES ${ZOO_GAME_SOURCES}
    ASSETS "${CMAKE_SOURCE_DIR}/assets"
    TITLE "Zoo Simulator"
    PROFILE enhanced
    LIBRARIES Zoo::Core CNA::GamerServices)
target_include_directories(zoo-simulator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(zoo-simulator SYSTEM PRIVATE "${ZOO_NLOHMANN_JSON_INCLUDE}")
set_target_properties(zoo-simulator PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
# The data-driven definitions travel beside the executable like the assets. Both are brought
# up to date on every build, not only when the executable links: a new recording or a changed
# species file must reach the game without a source change.
add_custom_target(zoo-simulator-files
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
    COMMENT "Copying data/ and assets/ beside zoo-simulator" VERBATIM)
add_dependencies(zoo-simulator zoo-simulator-files)
if(EMSCRIPTEN)
    # In the browser the working directory is "/": the definitions are preloaded beside the
    # assets (cna_engine_add_game preloads assets/ only). docs/WEB.md.
    target_link_options(zoo-simulator PRIVATE "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data@/data")
endif()
