####################################
# CMake build script for 3FD
#

cmake_minimum_required(VERSION 2.6)

project(3FD)

# Macro definitions:
add_definitions(
    -DENABLE_3FD_CST
    -DENABLE_3FD_ERR_IMPL_DETAILS
)

# NDEBUG when release mode:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
endif()

set(POCO_ROOT "/opt/poco-1.7.5")
set(BOOST_HOME "/opt/boost-1.60")

# Include directories:
include_directories(
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/../btree"
    "${PROJECT_SOURCE_DIR}/../OpenCL"
    "${POCO_ROOT}/include"
    "${BOOST_HOME}/include"
)

# Static library source files:
add_library(3FD STATIC
    callstacktracer.cpp
    cmdline.cpp
    configuration.cpp
    dependencies.cpp
    exceptions.cpp
    gc_addresseshashtable.cpp
    gc_arrayofedges.cpp
    gc_garbagecollector.cpp
    gc_memorydigraph.cpp
    gc_messages.cpp
    gc_vertex.cpp
    gc_vertexstore.cpp
    logger.cpp
    logger_poco.cpp
    opencl_impl.cpp
    opencl_impl_commandtracker.cpp
    opencl_impl_context.cpp
    opencl_impl_device.cpp
    opencl_impl_platform.cpp
    opencl_impl_program.cpp
    opencl_impl_programmanifest.cpp
    runtime.cpp
    sqlite3.c
    sqlite_databaseconn.cpp
    sqlite_dbconnpool.cpp
    sqlite_prepstatement.cpp
    sqlite_transaction.cpp
    utils_asynchronous.cpp
    utils_dynmempool.cpp
    utils_event.cpp
    utils_memorypool.cpp
)

