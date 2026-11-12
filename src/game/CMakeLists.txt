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
# The data-driven definitions travel beside the executable like the assets.
add_custom_command(TARGET zoo-simulator POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data"
    COMMENT "Copying data/ beside zoo-simulator" VERBATIM)
