#!/bin/bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

echo "Checking if homebrew is workable ..."
brew_workable=1
#  Check if homebrew is installed
if ! command -v brew &> /dev/null
then
    echo "homebrew could not be found."
    brew_workable=0
fi

# check if homebrew is working
if ! command -v brew update &> /dev/null
then
    echo "homebrew update failed. homebrew may not be working properly."
    brew_workable=0
fi

if [ $brew_workable -eq 1 ]
then
    # install cmake
    if ! cmake --version &> /dev/null
    then
        echo "Cmake could not be found. It is required to build the examples."
        echo "Do you want to install cmake? (y/n)"
        read answer
        if [ "$answer" == "y" ]
        then
            brew install cmake
        fi
    else
        echo "cmake is already installed."
    fi

    # install opencv
    if ! brew list opencv &> /dev/null
    then
        echo "opencv could not be found. Without opencv, part of the examples may not be built successfully."
        echo "Do you want to install opencv? (y/n)"
        read answer
        if [ "$answer" == "y" ]
        then
            brew install opencv
        fi
    else
        echo "opencv is already installed."
    fi
else
    echo "homebrew is not workable, network connection may be down or the system may not have internet access. Build examples may not be successful."
fi

# restore current directory
current_dir=$(pwd)

# cd to the directory where this script is located
cd "$(dirname "$0")"
project_dir=$(pwd)
examples_dir=$project_dir/examples

#detect cpu core count
cpu_count=$(sysctl -n hw.ncpu)
half_cpu_count=$((cpu_count/2))
if [ $half_cpu_count -eq 0 ]
then
    half_cpu_count=1
fi

#cmake
echo "Building examples..."
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DOB_BUILD_MACOS=ON -DCMAKE_INSTALL_PREFIX="$project_dir" "$examples_dir"
echo "Building examples with $half_cpu_count threads..."
cmake --build . -- -j$half_cpu_count # build with thread count equal to half of cpu count
# install the executable files to the project directory
make install

# clean up
cd $project_dir
rm -rf build

echo "OrbbecSDK examples built successfully!"
echo "The executable files located in: $project_dir/bin"

cd $current_dir
