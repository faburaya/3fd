#!/bin/bash

UBUNTU_VERSION=$(cat /etc/issue | grep -i ubuntu | awk -F" " '{print $2}' | sed 's/.[0-9]*$//g')
UBUNTU_MAJVER=$(echo $UBUNTU_VERSION | awk -F"." '{print $1}')
AMAZON=$(cat /etc/issue | grep -i "amazon linux ami release" | awk -F" " '{print $5}' | awk -F"." '{print $1}')

USE_CLANG=$(echo $CXX | grep 'clang++$')

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
    ACCEPT_EULA=Y sudo apt install --assume-yes msodbcsql
    ACCEPT_EULA=Y sudo apt install --assume-yes mssql-tools

    if [ -f $HOME/.bash_profile ] && [ -z "$(cat $HOME/.bash_profile | grep '$PATH:/opt/mssql-tools/bins')" ];
    then
        echo 'export PATH="$PATH:/opt/mssql-tools/bin"' >> $HOME/.bash_profile
    fi

    if [ -f $HOME/.profile ] && [ -z "$(cat $HOME/.profile | grep '$PATH:/opt/mssql-tools/bins')" ];
    then
        echo 'export PATH="$PATH:/opt/mssql-tools/bin"' >> $HOME/.profile
    fi

    export PATH="$PATH:/opt/mssql-tools/bin"
}

# # #
# INSTALL PACKAGES & TOOLCHAIN
#

if [ -n "$UBUNTU_MAJVER" ] && [ $UBUNTU_MAJVER -gt 14 ];
then

    if [ -z $CXX ] && [ -z "$(dpkg -l | grep 'g++')" ];
    then
        printf "${SetColorToYELLOW}Installing GNU C++ compiler...${SetNoColor}\n"
        sudo apt install --assume-yes g++
    fi

    printf "${SetColorToYELLOW}Checking dependencies...${SetNoColor}\n"
    sudo apt install --assume-yes unzip cmake unixodbc unixodbc-dev openssl libssl-dev

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

{ ls build || mkdir build; } &> /dev/null
cd build

# # #
# BUILD RAPIDXML
#
function buildRapidxml()
{
    find . | grep 'rapidxml' | xargs rm -rf
    RAPIDXML='rapidxml-1.13'
    download "https://netcologne.dl.sourceforge.net/project/rapidxml/rapidxml/rapidxml%201.13/${RAPIDXML}.zip"
    unzip "${RAPIDXML}.zip"
    mv $RAPIDXML/*.hpp include/
    rm -rf $RAPIDXML*
}

# # #
# BUILD SQLITE3
#
function buildSqlite3()
{
    find . | grep 'sqlite' | xargs rm -rf
    INSTALLDIR=$(pwd)
    SQLITE='sqlite-autoconf-3240000'
    download "http://sqlite.org/2018/${SQLITE}.tar.gz"
    tar -xf "${SQLITE}.tar.gz"
    cd $SQLITE
    printf "${SetColorToYELLOW}Building SQLite3 as static library (release)...${SetNoColor}\n"
    ./configure --enable-shared=no --prefix=$INSTALLDIR
    make -j $numCpuCores && make install
    cd ..
    rm -rf $SQLITE*
}

if [ -d include ]; then
    printf "${SetColorToYELLOW}Do you wish to download and (re)build RAPIDXML and SQLITE3 from source?${SetNoColor}"
    while true; do
        read -p " [yes/no] " yn
        case $yn in
            [Yy]* ) buildRapidxml
                    buildSqlite3
                    break;;
            [Nn]* ) break;;
            * ) printf "Please answer yes or no.";;
        esac
    done
else
    buildRapidxml
    buildSqlite3
fi

numCpuCores=$(grep -c ^processor /proc/cpuinfo)

# # #
# BUILD BOOST
#
function buildBoost()
{
    find ./lib | grep boost | xargs rm
    { ls include/boost && rm -rf include/boost; } &> /dev/null
    printf "${SetColorToYELLOW}Downloading Boost source...${SetNoColor}\n"
    boostVersion='1.67.0'
    boostLabel="boost_1_67_0"
    download "https://dl.bintray.com/boostorg/release/${boostVersion}/source/${boostLabel}.tar.gz"
    printf "${SetColorToYELLOW}Unpacking Boost source...${SetNoColor}\n"
    tar -xf "${boostLabel}.tar.gz"
    printf "${SetColorToYELLOW}Building Boost...${SetNoColor}\n"
    cd $boostLabel/tools/build
    ./bootstrap.sh
    cd ../../

    toolset=''
    if [ -n $USE_CLANG ];
    then
        toolset='toolset=clang'
    fi

    prefixDir=$(cd ../; pwd)
    ./tools/build/b2 -j $numCpuCores $toolset variant=debug   link=static threading=multi runtime-link=shared --layout=tagged --prefix=$prefixDir --with=system,thread,regex install
    ./tools/build/b2 -j $numCpuCores $toolset variant=release link=static threading=multi runtime-link=shared --layout=tagged --prefix=$prefixDir --with=system,thread,regex install
    cd ..
    rm -rf $boostLabel
    rm "${boostLabel}.tar.gz"
}

if [ -d include/boost ]; then
    printf "${SetColorToYELLOW}Do you wish to download and (re)build Boost library dependencies from source?${SetNoColor}"
    while true; do
        read -p " [yes/no] " yn
        case $yn in
            [Yy]* ) buildBoost
                    break;;
            [Nn]* ) break;;
            * ) printf "Please answer yes or no.";;
        esac
    done
else
    buildBoost
fi

# # #
# BUILD POCO
#
function buildPoco()
{
    find ./lib | grep Poco | xargs rm
    { ls include/Poco && rm -rf include/Poco; } &> /dev/null
    printf "${SetColorToYELLOW}Downloading POCO C++ libs source...${SetNoColor}\n"
    pocoLabel='poco-1.9.0'
    pocoTarFile=$pocoLabel"-all.tar.gz"
    pocoXDir=$pocoLabel"-all"
    download "https://pocoproject.org/releases/${pocoLabel}/${pocoTarFile}"
    printf "${SetColorToYELLOW}Unpacking POCO C++ libs source...${SetNoColor}\n"
    tar -xf $pocoTarFile
    cd $pocoXDir
    printf "${SetColorToYELLOW}Building POCO C++ libs...${SetNoColor}\n"

    toolset=''
    if [ -n $USE_CLANG ];
    then
        toolset='--config=Linux-clang'
    fi

    ./configure --omit=Data/MySQL,Data/SQLite,Dynamic,JSON,MongoDB,PageCompiler,Redis $toolset --static --no-tests --no-samples --prefix=$(cd ..; pwd)
    make -s -j $numCpuCores || exit
    make install
    cd ..
    rm -rf $pocoXDir
    rm $pocoTarFile
}

if [ -d include/Poco ]; then
    printf "${SetColorToYELLOW}Do you wish to download and (re)build POCO C++ library dependencies from source?${SetNoColor}"
    while true; do
        read -p " [yes/no] " yn
        case $yn in
            [Yy]* ) buildPoco
                    break;;
            [Nn]* ) break;;
            * ) printf "Please answer yes or no.";;
        esac
    done
else
    buildPoco
fi

# # #
# BUILD GTEST FRAMEWORK
#
function buildGTestFramework()
{
    find . | grep 'googletest' | xargs rm -rf
    INSTALLDIR=$(pwd)
    gtestVersion='release-1.8.0'
    gtestPackage="${gtestVersion}.tar.gz"
    download "https://github.com/google/googletest/archive/${gtestPackage}"
    tar -xf $gtestPackage
    gtestRootDir="googletest-${gtestVersion}"
    cd "${gtestRootDir}/googletest"
    printf "${SetColorToYELLOW}Building Google Test Framework as static library (debug)...${SetNoColor}\n"
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DEBUG_POSTFIX=d
    make -j $numCpuCores && mv libgtest* $INSTALLDIR/lib/
    make clean
    printf "${SetColorToYELLOW}Building Google Test Framework as static library (release)...${SetNoColor}\n"
    cmake -DCMAKE_BUILD_TYPE=Release
    make -j $numCpuCores && mv libgtest* $INSTALLDIR/lib/
    make clean
    mv include/gtest $INSTALLDIR/include/
    cd $INSTALLDIR
    rm -rf $gtestRootDir
    rm -rf $gtestPackage
}

if [ -d include ]; then
    printf "${SetColorToYELLOW}Do you wish to download and (re)build Google Test Framework from source?${SetNoColor}"
    while true; do
        read -p " [yes/no] " yn
        case $yn in
            [Yy]* ) buildGTestFramework
                    break;;
            [Nn]* ) break;;
            * ) printf "Please answer yes or no.";;
        esac
    done
else
    buildGTestFramework
fi