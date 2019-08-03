#!/bin/bash

mode=$1

if [ -n "$mode" ] && [ -n "$(echo $mode | grep -i '^debug$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode -DCMAKE_DEBUG_POSTFIX=d"

elif [ -n "$mode" ] && [ -n "$(echo $mode | grep -i '^release$')" ];
then
    CMAKE_OPTIONS="-DCMAKE_BUILD_TYPE=$mode"

else
    printf 'ERROR! Usage: ./configure.sh [debug|release] [lib_deps_dir]\n\n'
    exit
fi

if [ -n "$2" ];
then
  export LIB_DEPS_DIR=$2
  printf "Directory for installation of dependencies has been set to $LIB_DEPS_DIR\n"
else
  export LIB_DEPS_DIR=/opt/3fd
  printf "Directory for installation of dependencies has been set to default: $LIB_DEPS_DIR (provide as argument to override)\n"
fi

export BUILD_DIR=$(pwd)/build

echo '#!/bin/bash' > bvars.sh
echo "export LIB_DEPS_DIR=$LIB_DEPS_DIR" >> bvars.sh
echo "export BUILD_DIR=$BUILD_DIR" >> bvars.sh
chmod +x bvars.sh

{ ls $BUILD_DIR/include/3FD || mkdir -p $BUILD_DIR/include/3FD; } &> /dev/null
{ ls $BUILD_DIR/lib || mkdir -p $BUILD_DIR/lib; } &> /dev/null
{ ls $BUILD_DIR/bin || mkdir -p $BUILD_DIR/bin; } &> /dev/null

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
