#############################################
# CMake build script for IntegrationTests
#

cmake_minimum_required(VERSION 2.6)

project(IntegrationTests)
include(ExternalProject)

#####################
# Macro definitions:

add_definitions(
    -DENABLE_3FD_CST
    -DENABLE_3FD_ERR_IMPL_DETAILS
    -DTESTING
)

set(POCO_ROOT "/opt/Poco-1.6.1")
set(BOOST_HOME "/opt/boost-1.60")

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}/../3FD"
    "${PROJECT_SOURCE_DIR}/../gtest/include"
    "${PROJECT_SOURCE_DIR}/../OpenCL"
    "${PROJECT_SOURCE_DIR}/../btree"
    "${POCO_ROOT}/include"
    "${BOOST_HOME}/include"
)

########################
# Dependency libraries:

# How to build 3FD project
ExternalProject_Add(3FD_project
  DOWNLOAD_COMMAND echo "       3FD source is local"
  CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../3FD && ./configure.sh
  BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../3FD && make
  INSTALL_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../3FD && echo "        3FD installed in " && pwd
)

# How to build gtest project
ExternalProject_Add(gtest_project
  DOWNLOAD_COMMAND echo "       gtest source is local"
  CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../gtest && ./configure.sh
  BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../gtest && make
  INSTALL_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/../gtest && echo "        gtest installed in " && pwd
)

# How and what libs to link:
add_library(3FD STATIC IMPORTED)
add_library(gtest STATIC IMPORTED)
add_library(boost_system STATIC IMPORTED)
add_library(boost_thread STATIC IMPORTED)
add_library(PocoUtil STATIC IMPORTED)
add_library(PocoXML STATIC IMPORTED)
add_library(PocoFoundation STATIC IMPORTED)

# Where the lib binaries are:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(3FD PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FD.a")
    set_target_properties(gtest PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtest.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt.a")
    set_target_properties(PocoUtil PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtil.a")
    set_target_properties(PocoXML PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXML.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundation.a")
elseif(buildType STREQUAL debug)
    set_target_properties(3FD PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3FD/lib3FDd.a")
    set_target_properties(gtest PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtestd.a")
    set_target_properties(boost_system PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_system-mt-d.a")
    set_target_properties(boost_thread PROPERTIES IMPORTED_LOCATION "${BOOST_HOME}/lib/libboost_thread-mt-d.a")
    set_target_properties(PocoUtil PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoUtild.a")
    set_target_properties(PocoXML PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoXMLd.a")
    set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "${POCO_ROOT}/lib/libPocoFoundationd.a")
endif()

# Associate the dependencies with the corresponding projects:
add_dependencies(3FD 3FD_project)
add_dependencies(gtest gtest_project)

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT IntegrationTests.3fd.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/application.config ${CMAKE_CURRENT_BINARY_DIR}/IntegrationTests.3fd.config
   DEPENDS ${PROJECT_SOURCE_DIR}/application.config
)

# Executable source files:
add_executable(IntegrationTests
    IntegrationTests.cpp
    tests_core.cpp
    tests_gc.cpp
    tests_opencl.cpp
    tests_sqlite.cpp
    IntegrationTests.3fd.config
)

target_link_libraries(IntegrationTests 
    gtest
    3FD
    pthread dl
    boost_system boost_thread
    PocoUtil PocoXML PocoFoundation
)
