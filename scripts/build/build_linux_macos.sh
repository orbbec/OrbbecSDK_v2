#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)

# Define the platform
system=$(uname -o)
if [ "$system" = "Darwin" ]; then
    system=macos
else
    system=linux
fi

arch=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    PLATFORM=${system}_arm64
elif [ "$ARCH" = "armv7l" ]; then
    PLATFORM=${system}_arm32
else
    PLATFORM=${system}_$arch
fi

echo "Building OrbbecSDK for $PLATFORM"

# Variables for version and timestamp
if [ "$system" = "macos" ]; then
    VERSION=$(grep -o "project(\w* VERSION [0-9]*\.[0-9]*\.[0-9]*" $PROJECT_ROOT/CMakeLists.txt | grep -o "[0-9]*\.[0-9]*\.[0-9]*")
else
    VERSION=$(grep -oP 'project\(\w+\s+VERSION\s+\d+\.\d+\.\d+' $PROJECT_ROOT/CMakeLists.txt | grep -oP '\d+\.\d+\.\d+')
fi

TIMESTAMP=$(date +"%Y%m%d%H%M")


COMMIT_HASH=$(git rev-parse --short HEAD)
PACKAGE_NAME="OrbbecSDK_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM}"

# Create build directory
rm -rf build_$PLATFORM
mkdir -p build_$PLATFORM
cd build_$PLATFORM || exit

# Create target directory for installation
INSTALL_DIR=$(pwd)/install/$PACKAGE_NAME
mkdir -p $INSTALL_DIR

# Build and install OrbbecSDK
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DOB_BUILD_EXAMPLES=OFF || { echo 'Failed to run cmake'; exit 1; }
make install -j8 || { echo 'Failed to build OrbbecSDK'; exit 1; }

# Compress the installation directory
cd $INSTALL_DIR
cd ..
zip -rpy ${PACKAGE_NAME}.zip ${PACKAGE_NAME} || { echo 'Failed to compress installation directory'; exit 1; }

echo "Done building and compressing OrbbecSDK for $PLATFORM"
echo "Done building OrbbecSDK for $PLATFORM"

cd $CURRNET_DIR

