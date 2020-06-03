cmake_minimum_required(VERSION 3.10)

project(nanodbc)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-show-template-tree -fno-elide-type")
endif()

#####################
# Macro definitions:

add_definitions(
#   -DSOMETHING
)

# NDEBUG when release mode:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
endif()

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}/../"
)

########################
# Dependency libraries:

# Static library source files:
add_library(nanodbc STATIC
    nanodbc.cpp
)

################
# Installation:

install(
    TARGETS nanodbc
    DESTINATION "${PROJECT_SOURCE_DIR}/../build/lib"
)