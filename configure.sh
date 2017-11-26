#!/bin/bash

MODE=$1

if [[ -n $MODE ]] && [[ $MODE == $(echo $MODE | grep -i 'debug\|release') ]];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$MODE -DCMAKE_DEBUG_POSTFIX=d -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=\"-std=c++11\""
else
    printf 'ERROR! Usage: ./configure.sh [debug|release]\n\n'
    exit
fi

export POCO_ROOT="/opt/poco-1.7.8p3"
export BOOST_HOME="/opt/boost-1.65.1"

{ ls build/include/3FD || mkdir -p build/include/3FD; } &> /dev/null
{ ls build/lib || mkdir -p build/lib; } &> /dev/null
{ ls build/bin || mkdir -p build/bin; } &> /dev/null

cd gtest
echo Cleaning gtest...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring gtest...
cmake $CMAKE_OPTIONS

cd ../3FD
echo Cleaning 3FD...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring 3FD...
cmake $CMAKE_OPTIONS

cd ../UnitTests
echo Cleaning UnitTests...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring UnitTests...
cmake $CMAKE_OPTIONS

cd ../IntegrationTests
echo Cleaning IntegrationTests...
{ ls Makefile && make clean; } &> /dev/null
ls CMakeCache.txt &> /dev/null && rm CMakeCache.txt
ls CMakeFiles &> /dev/null && rm -rf CMakeFiles
echo Configuring IntegrationTests...
cmake $CMAKE_OPTIONS
