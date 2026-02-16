cmake_minimum_required(VERSION 3.30)

project(Ashenvale VERSION 1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

add_subdirectory(thirdparty)
add_subdirectory(source)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Ashenvale)
