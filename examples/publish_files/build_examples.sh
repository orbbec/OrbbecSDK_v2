#!/bin/bash

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
    echo "Installing compiler and tools..."
    sudo apt-get install -y build-essential cmake git

    #install dependencies
    echo "Installing dependencies..."
    sudo apt-get install -y libopencv-dev
else
    echo "Cannot install dependencies and build tools. Build examples may be failed."
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
cmake -DCMAKE_BUILD_TYPE=Release $examples_dir
make -j4

# copy the executable files to the project directory
cp bin/* $project_dir/bin

# clean up
cd $project_dir
rm -rf build

echo "OrbbecSDK examples built successfully!"
echo "The executable files located in: $project_dir/bin"

cd $current_dir