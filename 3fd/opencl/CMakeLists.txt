cmake_minimum_required(VERSION 3.10)

project(3fd-opencl)

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
)

# NDEBUG when release mode:
string(TOLOWER ${CMAKE_BUILD_TYPE} buildType)
if(buildType STREQUAL release)
    add_definitions(-DNDEBUG)
endif()

########################
# Include directories:

include_directories(
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/../../"
)

########################
# Dependency libraries:

# Static library source files:
add_library(3fd-opencl STATIC
    opencl_impl.cpp
    opencl_impl_commandtracker.cpp
    opencl_impl_context.cpp
    opencl_impl_device.cpp
    opencl_impl_platform.cpp
    opencl_impl_program.cpp
    opencl_impl_programmanifest.cpp
)

################
# Installation:

install(
    TARGETS 3fd-opencl
    DESTINATION "${PROJECT_SOURCE_DIR}/../../build/lib"
)