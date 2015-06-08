####################################
# CMake build script for UnitTests
#

cmake_minimum_required(VERSION 2.6)

project(UnitTests)

# Macro definitions:
add_definitions(
    -DENABLE_3FD_CST
    -DENABLE_3FD_ERR_IMPL_DETAILS
    -DTESTING
)

set(POCO_ROOT "/opt/Poco-1.4.7")
set(BOOST_HOME "/opt/boost-1.55")

# Include directories:
include_directories(
    "${PROJECT_SOURCE_DIR}/../3FD"
    "${PROJECT_SOURCE_DIR}/../gtest/include"
    "${PROJECT_SOURCE_DIR}/../btree"
    "${POCO_ROOT}/include"
    "${BOOST_HOME}/include"
)

# Dependency libraries:
add_library(3FD STATIC IMPORTED)
add_library(gtest STATIC IMPORTED)
add_library(PocoUtil STATIC IMPORTED)
add_library(PocoXML STATIC IMPORTED)
add_library(PocoFoundation STATIC IMPORTED)

string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(3FD PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FD.a")
    set_target_properties(gtest PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtest.a")
    set_target_properties(PocoUtil PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtil.a")
    set_target_properties(PocoXML PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXML.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundation.a")
elseif(buildType STREQUAL debug)
    set_target_properties(3FD PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FDd.a")
    set_target_properties(gtest PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtestd.a")
    set_target_properties(PocoUtil PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtild.a")
    set_target_properties(PocoXML PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXMLd.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundationd.a")
endif()

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT UnitTests.3fd.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/application.config ${CMAKE_CURRENT_BINARY_DIR}/UnitTests.3fd.config
   DEPENDS ${PROJECT_SOURCE_DIR}/application.config
)

# Executable source files:
add_executable(UnitTests
    UnitTests.cpp
    tests_gc_hashtable.cpp
    tests_gc_memblock.cpp
    tests_gc_memdigraph.cpp
    tests_gc_vertex.cpp
    tests_utils_pool.cpp
    UnitTests.3fd.config
)

target_link_libraries(UnitTests gtest 3FD pthread PocoUtil PocoXML PocoFoundation)

