#!/bin/bash

numCpuCores=$(grep -c ^processor /proc/cpuinfo)

{ ls bvars.sh || printf 'Run configure.sh first!'; } &> /dev/null
{ ls build || printf 'Run configure.sh first!'; } &> /dev/null

source bvars.sh

cd 3FD
make -j $numCpuCores && make install
cd ../UnitTests
make -j $numCpuCores && make install
cd ../IntegrationTests
make -j $numCpuCores && make install
cd ../
find 3FD/* | grep -v '_impl' | grep '\.h$' | xargs -I{} cp {} build/include/3FD/
cp -rf btree  $BUILD_DIR/include/
cp -rf OpenCL $BUILD_DIR/include/
cp CreateMsSqlSvcBrokerDB.sql $BUILD_DIR/
cp Acknowledgements.txt       $BUILD_DIR/
cp LICENSE $BUILD_DIR/
cp README  $BUILD_DIR/
