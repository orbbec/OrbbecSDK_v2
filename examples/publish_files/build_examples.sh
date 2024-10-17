#!/bin/bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

echo "Checking if apt-get is workable ..."
apt_workable=1
#  Check if apt-get is installed
if ! command -v apt-get &> /dev/null
then
    echo "apt-get could not be found."
    apt_workable=0
fi

# check if apt-get is working
if ! command -v sudo apt-get update &> /dev/null
then
    echo "apt-get update failed. apt-get may not be working properly."
    apt_workable=0
fi

if [ $apt_workable -eq 1 ]
then
    #install compiler and tools
    if ! g++ --version &> /dev/null || ! make --version &> /dev/null
    then
        echo "C++ Compiler and tools could not be found. It is required to build the examples."
        echo "Do you want to install it via install build-essential? (y/n)"
        read answer
        if [ "$answer" == "y" ]
        then
            sudo apt-get install -y build-essential
        fi
    else
        echo "C++ Compiler and tools is installed."
    fi

    # install cmake
    if ! cmake --version &> /dev/null
    then
        echo "Cmake could not be found. It is required to build the examples."
        echo "Do you want to install cmake? (y/n)"
        read answer
        if [ "$answer" == "y" ]
        then
            sudo apt-get install -y cmake
        fi
    else
        echo "cmake is installed."
    fi

    # install libopencv-dev
    if ! dpkg -l | grep libopencv-dev &> /dev/null ||  ! dpkg -l | grep libopencv &> /dev/null
    then
        echo "libopencv-dev or libopencv could not be found. Without opencv, part of the examples may not be built successfully."
        echo "Do you want to install libopencv-dev and libopencv? (y/n)"
        read answer
        if [ "$answer" == "y" ]
        then
            sudo apt-get install -y libopencv
            sudo apt-get install -y libopencv-dev
        fi
    else
        echo "libopencv-dev is installed."
    fi
else
    echo "apt-get is not workable, network connection may be down or the system may not have internet access. Build examples may not be successful."
fi

# restore current directory
current_dir=$(pwd)

# cd to the directory where this script is located
cd "$(dirname "$0")"
project_dir=$(pwd)
examples_dir=$project_dir/examples

#cmake
echo "Building examples..."
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DOB_BUILD_LINUX=ON $examples_dir
cmake --build . -j8
# install the executable files to the project directory
cmake --install . --prefix=$project_dir

# clean up
cd $project_dir
rm -rf build

echo "OrbbecSDK examples built successfully!"
echo "The executable files located in: $project_dir/bin"

cd $current_dir
