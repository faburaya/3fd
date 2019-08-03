######################################
# CMake build script for UnitTests
#

cmake_minimum_required(VERSION 3.0)

project(UnitTests)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-deprecated")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-show-template-tree -fno-elide-type")
endif()

#####################
# Macro definitions:

add_definitions(
    -DENABLE_3FD_CST
    -DENABLE_3FD_ERR_IMPL_DETAILS
    -DTESTING
)

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}/../3FD"
    "${PROJECT_SOURCE_DIR}/../btree"
    "$ENV{LIB_DEPS_DIR}/include"
)

########################
# Dependency libraries:

add_subdirectory("${PROJECT_SOURCE_DIR}/../3FD"
                 "${PROJECT_SOURCE_DIR}/../3FD")

# How and what libs to link:
add_library(gtest          STATIC IMPORTED)
add_library(PocoFoundation STATIC IMPORTED)

# Where the lib binaries are:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(3FD            PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FD.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libgtest.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libPocoFoundation.a")
elseif(buildType STREQUAL debug)
    set_target_properties(3FD            PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FDd.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libgtestd.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libPocoFoundationd.a")
endif()

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT UnitTests.3fd.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/application.config $ENV{BUILD_DIR}/bin/UnitTests.3fd.config
   DEPENDS ${PROJECT_SOURCE_DIR}/application.config
)

# Executable source files:
add_executable(UnitTests
    UnitTests.cpp
    tests_gc_hashtable.cpp
    tests_gc_vertex.cpp
    tests_gc_vertexstore.cpp
    tests_utils_algorithms.cpp
    tests_utils_io.cpp
    tests_utils_pool.cpp
    tests_utils_lockfreequeue.cpp
    tests_xml.cpp
    UnitTests.3fd.config
)

# Linking:
target_link_libraries(UnitTests
    gtest
    3FD
    pthread
    PocoFoundation
)

################
# Installation:

install(
    TARGETS UnitTests
    DESTINATION "$ENV{BUILD_DIR}/bin"
)
