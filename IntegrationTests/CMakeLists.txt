#############################################
# CMake build script for IntegrationTests
#

cmake_minimum_required(VERSION 2.6)

project(IntegrationTests)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

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
    "${PROJECT_SOURCE_DIR}/../OpenCL"
    "${PROJECT_SOURCE_DIR}/../btree"
    "${PROJECT_SOURCE_DIR}/../build/include"
)

########################
# Dependency libraries:

# How and what libs to link:
add_library(3FD            STATIC IMPORTED)
add_library(gtest          STATIC IMPORTED)
add_library(sqlite3        STATIC IMPORTED)
add_library(boost_system   STATIC IMPORTED)
add_library(boost_thread   STATIC IMPORTED)
add_library(PocoDataODBC   STATIC IMPORTED)
add_library(PocoData       STATIC IMPORTED)
add_library(PocoUtil       STATIC IMPORTED)
add_library(PocoXML        STATIC IMPORTED)
add_library(PocoFoundation STATIC IMPORTED)

# Where the lib binaries are:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(3FD            PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FD.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libgtest.a")
    set_target_properties(sqlite3        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libsqlite3.a")
    set_target_properties(boost_system   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libboost_system-mt.a")
    set_target_properties(boost_thread   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libboost_thread-mt.a")
    set_target_properties(PocoDataODBC   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoDataODBC.a")
    set_target_properties(PocoData       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoData.a")
    set_target_properties(PocoUtil       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoUtil.a")
    set_target_properties(PocoXML        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoXML.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoFoundation.a")
elseif(buildType STREQUAL debug)
    set_target_properties(3FD            PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FDd.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libgtestd.a")
    set_target_properties(sqlite3        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libsqlite3.a")
    set_target_properties(boost_system   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libboost_system-mt-d.a")
    set_target_properties(boost_thread   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libboost_thread-mt-d.a")
    set_target_properties(PocoDataODBC   PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoDataODBCd.a")
    set_target_properties(PocoData       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoDatad.a")
    set_target_properties(PocoUtil       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoUtild.a")
    set_target_properties(PocoXML        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoXMLd.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../build/lib/libPocoFoundationd.a")
endif()

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT IntegrationTests.3fd.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/application.config ${PROJECT_SOURCE_DIR}/../build/bin/IntegrationTests.3fd.config
   DEPENDS ${PROJECT_SOURCE_DIR}/application.config
)

# Executable source files:
add_executable(IntegrationTests
    IntegrationTests.cpp
    tests_broker.cpp
    tests_core.cpp
    tests_gc.cpp
    tests_opencl.cpp
    tests_sqlite.cpp
    IntegrationTests.3fd.config
)

# Linking:
target_link_libraries(IntegrationTests 
    gtest
    3FD
    odbc sqlite3 pthread dl
    boost_system boost_thread
    PocoDataODBC PocoData PocoUtil PocoXML PocoFoundation
)

################
# Installation:

install(
    TARGETS IntegrationTests
    DESTINATION "${PROJECT_SOURCE_DIR}/../build/bin"
)
