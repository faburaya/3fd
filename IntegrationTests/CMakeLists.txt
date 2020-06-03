cmake_minimum_required(VERSION 3.10)

project(IntegrationTests)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-show-template-tree -fno-elide-type")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
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
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/../"
    "${PROJECT_SOURCE_DIR}/../gtest/include/"
)

########################
# Dependency libraries:

# How and what external libs to link:
#add_library(PocoFoundation STATIC IMPORTED)

# Where the lib binaries are:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
    set_target_properties(3fd-core       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/core/lib3fd-core.a")
    set_target_properties(3fd-utils      PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-utils.a")
    set_target_properties(3fd-sqlite     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-sqlite.a")
    set_target_properties(3fd-broker     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-broker.a")
    set_target_properties(3fd-opencl     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-opencl.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtest.a")
    set_target_properties(sqlite3        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../sqlite3/libsqlite3.a")
    set_target_properties(nanodbc        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../nanodbc/libnanodbc.a")
#   set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libPocoFoundation.a")
elseif(buildType STREQUAL debug)
    set_target_properties(3fd-core       PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/core/lib3fd-cored.a")
    set_target_properties(3fd-utils      PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-utilsd.a")
    set_target_properties(3fd-sqlite     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-sqlited.a")
    set_target_properties(3fd-broker     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-brokerd.a")
    set_target_properties(3fd-opencl     PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../3fd/utils/lib3fd-opencld.a")
    set_target_properties(gtest          PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../gtest/libgtestd.a")
    set_target_properties(sqlite3        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../sqlite3/libsqlite3d.a")
    set_target_properties(nanodbc        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/../nanodbc/libnanodbcd.a")
#   set_target_properties(PocoFoundation PROPERTIES IMPORTED_LOCATION "$ENV{LIB_DEPS_DIR}/lib/libPocoFoundationd.a")
endif()

# Place the configuration file along with the executable:
add_custom_command(
   OUTPUT IntegrationTests.3fd.config
   COMMAND cp ${PROJECT_SOURCE_DIR}/application.config $ENV{BUILD_DIR}/bin/IntegrationTests.3fd.config
   DEPENDS ${PROJECT_SOURCE_DIR}/application.config
)

# Executable source files:
add_executable(IntegrationTests
    ../TestShared/error_handling.cpp
    ../TestShared/main.cpp
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
    3fd-opencl
    3fd-broker
    3fd-sqlite
    3fd-core
    3fd-utils
#   PocoFoundation
    sqlite3 nanodbc odbc
    pthread dl stdc++fs
)

################
# Installation:

install(
    TARGETS IntegrationTests
    DESTINATION "$ENV{BUILD_DIR}/bin"
)
