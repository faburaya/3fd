#!/bin/bash

mode=$1

if [ -n $mode ] && [ -n "$(echo $mode | grep -i '^debug$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode -DCMAKE_DEBUG_POSTFIX=d"

elif [ -n $mode ] && [ -n "$(echo $mode | grep -i '^release$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode"

else
    printf 'ERROR! Usage: ./configure.sh [debug|release]\n\n'
    exit
fi

{ ls build/include/3FD || mkdir -p build/include/3FD; } &> /dev/null
{ ls build/lib || mkdir -p build/lib; } &> /dev/null
{ ls build/bin || mkdir -p build/bin; } &> /dev/null

cd 3FD
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

cd ..
