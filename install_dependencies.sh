#!/bin/bash

UBUNTU_VERSION=$(cat /etc/issue | grep -i ubuntu | grep -o --color=never '[1-9][0-9]\.[0-9][0-9]')
UBUNTU_MAJVER=$(echo $UBUNTU_VERSION | awk -F"." '{print $1}')
AMAZON=$(cat /etc/issue | grep -i "amazon linux ami release" | awk -F" " '{print $5}' | awk -F"." '{print $1}')

USE_CLANG=$(echo $CXX | grep 'clang++$')

numCpuCores=$(grep -c ^processor /proc/cpuinfo)

# if [ -n "$1" ];
# then
#   INSTALL_DIR=$1
# else
#   INSTALL_DIR="/opt/3fd"
# fi
#
# if [ ! -d $INSTALL_DIR ];
# then
#   printf "Installation directory does not exist! ${INSTALL_DIR}\n"
#   exit
# fi
#
# DEPS=$(pwd)/build
# { ls $DEPS/include || mkdir -p $DEPS/include; } &> /dev/null
#
# if touch $DEPS/test &> /dev/null;
# then
#   cd $DEPS
#   rm test
# else
#   printf "Cannot write to installation directory! ${INSTALL_DIR}\n"
#   exit
# fi

export SetColorToYELLOW='\033[0;33m';
export SetNoColor='\033[0m';

# # #
# DOWNLOAD WITH RETRY
#
function download()
{
    for count in 1 .. 3
    do
        wget --retry-connrefused --waitretry=1 --read-timeout=15 --timeout=15 -t 2 --continue --no-dns-cache $1

        if [ $? = 0 ];
        then
            break
        fi

        sleep 1s;
    done
}

# # #
# INSTALL MSSQL TOOLS & ODBC DRIVER
#
function installMsSqlOdbc()
{
    printf "${SetColorToYELLOW}Installing MSSQL tools...${SetNoColor}\n"
    curl "http://packages.microsoft.com/keys/microsoft.asc" | sudo apt-key add -
    curl "https://packages.microsoft.com/config/ubuntu/${UBUNTU_VERSION}/prod.list" > mssql-release.list
    sudo mv mssql-release.list /etc/apt/sources.list.d/
    sudo apt-get update
    sudo apt install unixodbc-dev
    ACCEPT_EULA=Y sudo apt install --assume-yes msodbcsql
    ACCEPT_EULA=Y sudo apt install --assume-yes mssql-tools
    sudo sed -i 's|\"$|:/opt/mssql-tools/bin\"|g' /etc/environment
}

# # #
# INSTALL PACKAGES & TOOLCHAIN
#

if [ -n "$UBUNTU_MAJVER" ] && [ $UBUNTU_MAJVER -ge 20 ];
then

    if [ -z $CXX ] && [ -z "$(dpkg -l | grep 'g++')" ];
    then
        printf "${SetColorToYELLOW}Installing GNU C++ compiler...${SetNoColor}\n"
        sudo apt install --assume-yes g++
    fi

    printf "${SetColorToYELLOW}Checking dependencies...${SetNoColor}\n"
    sudo apt install --assume-yes cmake

    HAS_MSSQL_TOOLS=$(dpkg -l | grep "mssql-tools")

    # MSSQL TOOLS not yet installed?
    if [ -z "$HAS_MSSQL_TOOLS" ];
    then
        printf "${SetColorToYELLOW}Do you wish to install Microsoft SQL Server ODBC driver?${SetNoColor}"
        while true; do
            read -p " [yes/no] " yn
            case $yn in
                [Yy]* ) installMsSqlOdbc
                        break;;
                [Nn]* ) break;;
                * ) printf "Please answer yes or no.";;
            esac
        done
    fi

elif [ -n "$AMAZON" ] && [ $AMAZON -gt 2017 ];
then
    printf "${SetColorToYELLOW}Checking dependencies...${SetNoColor}\n"
    yum groups install -y development
    yum groups install -y development-libs
    download "https://cmake.org/files/v3.10/cmake-3.10.3.tar.gz"
    tar -xf cmake-3.10.3.tar.gz
    cd cmake-3.10.3
    ./bootstrap
    make && make install
    export PATH=$PATH:/usr/local/bin
    cd ..
    rm -rf ./cmake*

else
    echo OS UNSUPPORTED
    exit
fi

# # #
# INSTALL VCPKG
#
# cd /opt
# { ls vcpkg || git clone https://github.com/Microsoft/vcpkg; } &> /dev/null
# cd vcpkg
# if [ ! -f ./vcpkg ];
# then
#     printf "${SetColorToYELLOW}Installing vcpkg...${SetNoColor}\n"
#     ./bootstrap-vcpkg.sh
# fi
#
# printf "${SetColorToYELLOW}Checking dependencies packages...${SetNoColor}\n"
# ./vcpkg install boost-lockfree boost-regex poco rapidxml sqlite3
# cd ..

# # #
# BUILD RAPIDXML
#
function buildRapidxml()
{
    find . -type d | grep 'rapidxml' | xargs rm -rf
    RAPIDXML='rapidxml-1.13'
    download "https://netcologne.dl.sourceforge.net/project/rapidxml/rapidxml/rapidxml%201.13/${RAPIDXML}.zip"
    unzip "${RAPIDXML}.zip"
    { ls include/rapidxml || mkdir -p include/rapidxml; } &> /dev/null
    mv $RAPIDXML/*.hpp include/rapidxml/
    rm -rf $RAPIDXML*
}

# # #
# BUILD SQLITE3
#
function buildSqlite3()
{
    find . | grep 'sqlite' | xargs rm -rf
    local SQLITE='sqlite-autoconf-3270200'
    download "http://sqlite.org/2019/sqlite-autoconf-3270200.tar.gz"
    tar -xf $SQLITE".tar.gz"
    cd $SQLITE
    printf "${SetColorToYELLOW}Building SQLite3 as static library (release)...${SetNoColor}\n"
    ./configure --enable-shared=no --prefix=$DEPS
    make -j $numCpuCores && make install
    cd ..
    rm -rf $SQLITE*
}

# if [ -d $INSTALL_DIR/include ]; then
#     printf "${SetColorToYELLOW}Do you wish to download and (re)build RAPIDXML and SQLITE3 from source?${SetNoColor}"
#     while true; do
#         read -p " [yes/no] " yn
#         case $yn in
#             [Yy]* ) buildRapidxml
#                     buildSqlite3
#                     break;;
#             [Nn]* ) break;;
#             * ) printf "Please answer yes or no.";;
#         esac
#     done
# else
#     buildRapidxml
#     buildSqlite3
# fi

# # #
# BUILD BOOST
#
function buildBoost()
{
    find ./lib | grep boost | xargs rm
    { ls include/boost && rm -rf include/boost; } &> /dev/null
    printf "${SetColorToYELLOW}Downloading Boost source...${SetNoColor}\n"
    local boostVersion='1.67.0'
    local boostLabel="boost_1_67_0"
    download "https://dl.bintray.com/boostorg/release/${boostVersion}/source/${boostLabel}.tar.gz"
    printf "${SetColorToYELLOW}Unpacking Boost source...${SetNoColor}\n"
    tar -xf "${boostLabel}.tar.gz"
    printf "${SetColorToYELLOW}Building Boost...${SetNoColor}\n"
    cd $boostLabel/tools/build
    ./bootstrap.sh
    cd $DEPS/$boostLabel

    local toolset='toolset=gcc'
    if [ -n "$USE_CLANG" ];
    then
        toolset='toolset=clang'
    fi

    ./tools/build/b2 -j $numCpuCores $toolset variant=debug   link=static threading=multi runtime-link=shared --layout=tagged --prefix=$DEPS --with=system,thread,regex install
    ./tools/build/b2 -j $numCpuCores $toolset variant=release link=static threading=multi runtime-link=shared --layout=tagged --prefix=$DEPS --with=system,thread,regex install
    cd ..
    rm -rf $boostLabel
    rm "${boostLabel}.tar.gz"
}

# if [ -d $INSTALL_DIR/include/boost ];
# then
#     printf "${SetColorToYELLOW}Do you wish to download and (re)build Boost library dependencies from source?${SetNoColor}"
#     while true; do
#         read -p " [yes/no] " yn
#         case $yn in
#             [Yy]* ) buildBoost
#                     break;;
#             [Nn]* ) break;;
#             * ) printf "Please answer yes or no.";;
#         esac
#     done
# else
#     buildBoost
# fi

# # #
# BUILD POCO
#
function buildPoco()
{
    find ./lib | grep Poco | xargs rm
    { ls include/Poco && rm -rf include/Poco; } &> /dev/null
    printf "${SetColorToYELLOW}Downloading POCO C++ libs source...${SetNoColor}\n"
    local pocoLabel='poco-1.9.4'
    local pocoXDir=$pocoLabel"-all"
    local pocoTarFile=$pocoXDir".tar.gz"
    download "https://pocoproject.org/releases/${pocoLabel}/${pocoTarFile}"
    printf "${SetColorToYELLOW}Unpacking POCO C++ libs source...${SetNoColor}\n"
    tar -xf $pocoTarFile
    cd $pocoXDir
    printf "${SetColorToYELLOW}Building POCO C++ libs...${SetNoColor}\n"

    local toolset='--config=Linux'
    if [ -n "$USE_CLANG" ];
    then
        toolset='--config=Linux-clang'
    fi

    ./configure --omit=Data/MySQL,Data/SQLite,Dynamic,JSON,MongoDB,PageCompiler,Redis $toolset --static --no-tests --no-samples --prefix=$DEPS
    make -s -j $numCpuCores || exit
    make install
    cd ..
    rm -rf $pocoXDir
    rm $pocoTarFile
}

# if [ -d $INSTALL_DIR/include/Poco ];
# then
#     printf "${SetColorToYELLOW}Do you wish to download and (re)build POCO C++ library dependencies from source?${SetNoColor}"
#     while true; do
#         read -p " [yes/no] " yn
#         case $yn in
#             [Yy]* ) buildPoco
#                     break;;
#             [Nn]* ) break;;
#             * ) printf "Please answer yes or no.";;
#         esac
#     done
# else
#     buildPoco
# fi

# # #
# BUILD GTEST FRAMEWORK
#
function buildGTestFramework()
{
    find . | grep 'googletest' | xargs rm -rf
    local gtestVersion='release-1.8.0'
    local gtestPackage="${gtestVersion}.tar.gz"
    download "https://github.com/google/googletest/archive/${gtestPackage}"
    tar -xf $gtestPackage
    local gtestRootDir="googletest-${gtestVersion}"
    cd "${gtestRootDir}/googletest"
    printf "${SetColorToYELLOW}Building Google Test Framework as static library (debug)...${SetNoColor}\n"
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DEBUG_POSTFIX=d
    make -j $numCpuCores && mv libgtest* $DEPS/lib/
    make clean
    printf "${SetColorToYELLOW}Building Google Test Framework as static library (release)...${SetNoColor}\n"
    cmake -DCMAKE_BUILD_TYPE=Release
    make -j $numCpuCores && mv libgtest* $DEPS/lib/
    make clean
    mv include/gtest $DEPS/include/
    cd $DEPS
    rm -rf $gtestRootDir
    rm -rf $gtestPackage
}

# if [ -d $INSTALL_DIR/include/gtest ];
# then
#     printf "${SetColorToYELLOW}Do you wish to download and (re)build Google Test Framework from source?${SetNoColor}"
#     while true; do
#         read -p " [yes/no] " yn
#         case $yn in
#             [Yy]* ) buildGTestFramework
#                     break;;
#             [Nn]* ) break;;
#             * ) printf "Please answer yes or no.";;
#         esac
#     done
# else
#     buildGTestFramework
# fi

# printf "${SetColorToYELLO}Moving build of dependencies to installation directory...${SetNoColor}\n"
# sudo mv $DEPS/* $INSTALL_DIR
