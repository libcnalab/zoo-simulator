# SPDX-License-Identifier: MIT
#
# zoo_core: the whole simulation, free of CNA and of any graphics, audio or
# window dependency, so that it builds and tests in seconds and under
# sanitizers.
file(GLOB_RECURSE ZOO_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(zoo_core STATIC ${ZOO_CORE_SOURCES})
add_library(Zoo::Core ALIAS zoo_core)
target_include_directories(zoo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(zoo_core SYSTEM PRIVATE "${ZOO_NLOHMANN_JSON_INCLUDE}")
target_link_libraries(zoo_core PRIVATE zoo_warnings PUBLIC zoo_sanitizers)
