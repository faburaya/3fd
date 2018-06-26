#!/bin/bash

numCpuCores=$(grep -c ^processor /proc/cpuinfo)

{ ls build || printf 'Run configure.sh first!'; } &> /dev/null

cd 3FD
make -j $numCpuCores && make install
cd ../UnitTests
make -j $numCpuCores && make install
cd ../IntegrationTests
make -j $numCpuCores && make install
cd ../
find 3FD/* | grep -v '_impl' | grep '\.h$' | xargs -I{} cp {} build/include/3FD/
cp -rf btree  build/include/
cp -rf OpenCL build/include/
cp CreateMsSqlSvcBrokerDB.sql build/
cp Acknowledgements.txt       build/
cp LICENSE build/
cp README  build/
