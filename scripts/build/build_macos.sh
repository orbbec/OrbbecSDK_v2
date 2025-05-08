#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)

# current os
SYSTEM=$(uname -o)
if [ "${SYSTEM}" != "Darwin" ]; then
    echo "Invalid os. The current os should be macOS"
    exit 1
fi

# get arch from arg
ARCH=$1
if [ -z "${ARCH}" ]; then
    # default is universal
    BUILD_ARM64=1
    BUILD_X64=1
    MERGE_LIB=1
elif [ "${ARCH}" = "x86_64" ]; then
    BUILD_ARM64=0
    BUILD_X64=1
    MERGE_LIB=0
elif [ "${ARCH}" = "arm64" ]; then
    BUILD_ARM64=1
    BUILD_X64=0
    MERGE_LIB=0
else
    echo "Invalid architecture: ${ARCH}, supported architectures are x86_64, arm64, and universal"
    exit 1
fi

# Variables for version and timestamp
VERSION=$(grep -o "project(\w* VERSION [0-9]*\.[0-9]*\.[0-9]*" ${PROJECT_ROOT}/CMakeLists.txt | grep -o "[0-9]*\.[0-9]*\.[0-9]*")
TIMESTAMP=$(date +"%Y%m%d%H%M")
# commit hash
COMMIT_HASH=$(git rev-parse --short HEAD)

# Function: build sdk
# $1: arch: x86_64 or arm64
# $2: build dir
# $3: install dir
build_sdk(){
    local arch=$1
    local build_dir=$2
    local install_dir=$3

    echo "Building OrbbecSDK for macos_${arch}"

    # Create build directory
    rm -rf ${build_dir}
    mkdir -p ${build_dir}
    cd ${build_dir} || exit

    # Create target directory for installation
    rm -rf ${install_dir}
    mkdir -p ${install_dir}

    # Build and install OrbbecSDK
    cmake_script="cmake .. -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_OSX_ARCHITECTURES=${arch} \
                -DCMAKE_INSTALL_PREFIX=${install_dir} \
                -DOB_BUILD_SOVERSION=ON \
                -DOB_BUILD_EXAMPLES=OFF"

    ${cmake_script} || { echo 'Failed to run cmake'; exit 1; }
    make install -j8 || { echo 'Failed to build OpenOrbbecSDK'; exit 1; }
    
    cd ..

    echo "Done building OrbbecSDK for macos_${arch}"
}

# Platform universal
PLATFORM_ARM64="macOS_arm64"
PLATFORM_X64="macOS_x64"
PLATFORM_UNIVERSAL="macOS"

# Build dir
BUILD_DIR_ARM64="build_${PLATFORM_ARM64}"
BUILD_DIR_X64="build_${PLATFORM_X64}"
BUILD_DIR_UNIVERSAL="build_${PLATFORM_UNIVERSAL}"

# package name
PACKAGE_NAME_ARM64="OrbbecSDK_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_ARM64}"
PACKAGE_NAME_X64="OrbbecSDK_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_X64}"
PACKAGE_NAME_UNIVERSAL="OrbbecSDK_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_UNIVERSAL}"

# Install dir
INSTALL_DIR_ARM64="${PROJECT_ROOT}/${BUILD_DIR_ARM64}/install/${PACKAGE_NAME_ARM64}"
INSTALL_DIR_X64="${PROJECT_ROOT}/${BUILD_DIR_X64}/install/${PACKAGE_NAME_X64}"
INSTALL_DIR_UNIVERSAL="${PROJECT_ROOT}/${BUILD_DIR_UNIVERSAL}/install/${PACKAGE_NAME_UNIVERSAL}"

# Build OpenOrbbecSDK arm64
if [ "${BUILD_ARM64}" != "0" ]; then
    cd ${PROJECT_ROOT}
    build_sdk arm64 "${BUILD_DIR_ARM64}" "${INSTALL_DIR_ARM64}"
    PACKAGE_NAME=${PACKAGE_NAME_ARM64}
    INSTALL_DIR=${INSTALL_DIR_ARM64}
    PLATFORM=${PLATFORM_ARM64}
fi

# Build OpenOrbbecSDK x86_64
if [ "${BUILD_X64}" != "0" ]; then
    cd ${PROJECT_ROOT}
    build_sdk x86_64 "${BUILD_DIR_X64}" "${INSTALL_DIR_X64}"
    PACKAGE_NAME=${PACKAGE_NAME_X64}
    INSTALL_DIR=${INSTALL_DIR_X64}
    PLATFORM=${PLATFORM_X64}
fi

if [ "${MERGE_LIB}" != "0" ]; then
    # multiple architectures. merge all library/execute files to one

    # copy arm64 dir to new install dir
    rm -fr ${BUILD_DIR_UNIVERSAL}
    rm -fr ${INSTALL_DIR_UNIVERSAL}
    mkdir -p ${INSTALL_DIR_UNIVERSAL}
    cp -R -P ${INSTALL_DIR_ARM64}/ ${INSTALL_DIR_UNIVERSAL}/

    # merge library
    SDK_LIB_NAME=libOrbbecSDK.${VERSION}.dylib
    OLD_PATH_ARM64=${INSTALL_DIR_ARM64}/lib/${SDK_LIB_NAME}
    OLD_PATH_X64=${INSTALL_DIR_X64}/lib/${SDK_LIB_NAME}
    NEW_PATH=${INSTALL_DIR_UNIVERSAL}/lib/${SDK_LIB_NAME}
    lipo -create "${OLD_PATH_ARM64}" "${OLD_PATH_X64}" -output "${NEW_PATH}" || { echo 'Failed to merge arm64 and x86_64 libraries to one'; exit 1; }
    cp -fr "${NEW_PATH}" "$INSTALL_DIR_UNIVERSAL/bin/$SDK_LIB_NAME"
    # merge other files, such as ob_benchmark
    lipo -create "${INSTALL_DIR_ARM64}/bin/ob_benchmark" "${INSTALL_DIR_X64}/bin/ob_benchmark" -output "${INSTALL_DIR_UNIVERSAL}/bin/ob_benchmark" || { echo 'Failed to merge arm64 and x86_64 libraries to one'; exit 1; }

    # Compress the installation directory
    cd ${INSTALL_DIR_UNIVERSAL}
    cd ..
    zip -rpy ${PACKAGE_NAME_UNIVERSAL}.zip ${PACKAGE_NAME_UNIVERSAL} || { echo 'Failed to compress installation directory'; exit 1; }

    echo "Done compressing OrbbecSDK for ${PLATFORM_UNIVERSAL}"
else
    # single architecture
    # Compress the installation directory
    cd ${INSTALL_DIR}
    cd ..
    zip -rpy ${PACKAGE_NAME}.zip ${PACKAGE_NAME} || { echo 'Failed to compress installation directory'; exit 1; }

    echo "Done compressing OrbbecSDK for ${PLATFORM}"
fi

cd ${CURRNET_DIR}
