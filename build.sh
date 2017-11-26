#!/bin/bash

{ ls build || printf 'Run configure.sh first!'; } &> /dev/null

cd gtest
make install
cd ../3FD
make install
cd ../UnitTests
make install
cd ../IntegrationTests
make install
cd ../
cp 3FD/*.h build/include/3FD
cp -rf btree  build/include/
cp -rf OpenCL build/include/
cp CreateMsSqlSvcBrokerDB.sql build/
cp Acknowledgements.txt       build/
cp LICENSE build/
cp README  build/
