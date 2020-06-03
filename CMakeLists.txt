cmake_minimum_required(VERSION 3.10)

project(3fd)

add_subdirectory(gtest)
add_subdirectory(nanodbc)
add_subdirectory(sqlite3)

add_subdirectory(3fd/core)
add_subdirectory(3fd/utils)
add_subdirectory(3fd/sqlite)
add_subdirectory(3fd/broker)
add_subdirectory(3fd/opencl)

add_subdirectory(UnitTests)
add_subdirectory(IntegrationTests)