#!/bin/bash

UBUNTU=$(cat /etc/issue | grep -i ubuntu | awk -F" " '{print $2}' | awk -F"." '{print $1}')
AMAZON=$(cat /etc/issue | grep -i "amazon linux ami release" | awk -F" " '{print $5}' | awk -F"." '{print $1}')

# # #
# SETUP DEV ENV
#

if [ -n "$UBUNTU" ] && [ $UBUNTU -gt 14 ]; then
    apt install -y clang cmake unixodbc unixodbc-dev openssl libssl-dev
    curl https://packages.microsoft.com/keys/microsoft.asc | apt-key add -
    curl https://packages.microsoft.com/config/ubuntu/16.04/prod.list > /etc/apt/sources.list.d/mssql-release.list
    apt-get update
    ACCEPT_EULA=Y apt-get install msodbcsql
    ACCEPT_EULA=Y apt-get install mssql-tools
    echo 'export PATH="$PATH:/opt/mssql-tools/bin"' >> ~/.bash_profile
    echo 'export PATH="$PATH:/opt/mssql-tools/bin"' >> ~/.bashrc
    source ~/.bashrc

elif [ -n "$AMAZON" ] && [ $AMAZON -gt 2017 ]; then
    yum groups install -y development
    yum groups install -y development-libs
    yum install -y clang
    export CC=clang
    export CXX=clang++
    wget https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
    tar -xf cmake-3.8.2.tar.gz
    cd cmake-3.8.2
    ./bootstrap
    make
    make install
    export PATH=$PATH:/usr/local/bin
    cd ..
    rm -rf ./cmake*

else
    echo OS UNSUPPORTED
    exit
fi

# # #
# INSTALL BOOST
#
boostVersion='1.65.1'
boostLabel="boost_1_65_1"
wget "https://dl.bintray.com/boostorg/release/$boostVersion/source/$boostLabel.tar.bz2"
tar -xf "$boostLabel.tar.bz2"
cd "$boostLabel/tools/build/"
./bootstrap.sh
cd ../../
./tools/build/b2 -j 3 variant=debug link=static threading=multi toolset=clang runtime-link=shared --layout=tagged
./tools/build/b2 -j 3 variant=release link=static threading=multi toolset=clang runtime-link=shared --layout=tagged
BOOST_HOME="/opt/boost-$boostVersion"
mkdir $BOOST_HOME
mkdir $BOOST_HOME/include
mkdir $BOOST_HOME/lib
mv stage/lib/* $BOOST_HOME/lib/
mv boost $BOOST_HOME/include/
cd ..
rm -rf $boostLabel
rm "$boostLabel.tar.bz2"

# # #
# INSTALL POCO
#
pocoLabel='poco-1.7.8p3'
pocoTarFile=$pocoLabel"-all.tar.gz"
pocoXDir=$pocoLabel"-all"
wget "https://pocoproject.org/releases/poco-1.7.8/$pocoTarFile"
tar -xf $pocoTarFile
cd $pocoXDir
./configure --omit=Data/MySQL,MongoDB,PageCompiler --config=Linux-clang --static --no-tests --no-samples --prefix="/opt/$pocoLabel"
make -s -j3 || exit
make install
cd ..
rm -rf $pocoXDir
rm $pocoTarFile

