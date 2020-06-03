#!/bin/bash

mode=$1;

if [ -n "$mode" ] && [ -n "$(echo $mode | grep -i '^debug$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode -DCMAKE_DEBUG_POSTFIX=d";

elif [ -n "$mode" ] && [ -n "$(echo $mode | grep -i '^release$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode";

else
    printf 'ERROR! Usage: ./build.sh [debug|release]\n\n';
    exit;
fi

export BUILD_DIR=$(pwd)/build;

{ ls $BUILD_DIR/lib || mkdir -p $BUILD_DIR/lib; } &> /dev/null;
{ ls $BUILD_DIR/bin || mkdir -p $BUILD_DIR/bin; } &> /dev/null;

cmake $CMAKE_OPTIONS -DCMAKE_EXPORT_COMPILE_COMMANDS=1;
numCpuCores=$(grep -c ^processor /proc/cpuinfo);
make -j $numCpuCores && make install;

{ ls $BUILD_DIR/include/btree || mkdir -p $BUILD_DIR/include/btree; } &> /dev/null;
cp -rf btree $BUILD_DIR/include/btree/;

function copyHeaders()
{
    local moduleRelativePath=$1;
    { ls $BUILD_DIR/include/$moduleRelativePath || mkdir -p $BUILD_DIR/include/$moduleRelativePath; } &> /dev/null;
    find $moduleRelativePath -maxdepth 1 -type f | grep '\.h$' | grep -v "_impl\|_winrt\|pch.h\|targetver.h" | xargs -I{} cp {} build/include/$moduleRelativePath;
}

copyHeaders 3fd/core;
copyHeaders 3fd/utils;
copyHeaders 3fd/sqlite;
copyHeaders 3fd/broker;
copyHeaders 3fd/opencl;
copyHeaders 3fd/opencl/CL;

cp Acknowledgements.txt   $BUILD_DIR/;
cp LICENSE                $BUILD_DIR/;
cp README                 $BUILD_DIR/;
