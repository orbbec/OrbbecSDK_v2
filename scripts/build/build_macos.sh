#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)

# current os
SYSTEM=$(uname -s)
if [ "${SYSTEM}" != "Darwin" ]; then
    echo "Invalid os. The current os should be macOS"
    exit 1
fi

# Default args
DEFAULT_LIB_NAME="OrbbecSDK"
SDK_LIB_NAME=${DEFAULT_LIB_NAME}
BUILD_TYPE="Release"
# Default arch is universal
ARCH="universal"
BUILD_ARM64=1
BUILD_X64=1
MERGE_LIB=1

# User args
for arg in "$@"; do
    if [[ "$arg" =~ ^--?([^=]+)=(.+)$ ]]; then
        key=$(echo "${BASH_REMATCH[1]}" | tr '[:upper:]' '[:lower:]')
        value="${BASH_REMATCH[2]}"

        case "$key" in
            arch)
                val_lower=$(echo "$value" | tr '[:upper:]' '[:lower:]')
                case "$val_lower" in
                    x86_64|x64)
                        ARCH="x86_64"
                        BUILD_ARM64=0
                        BUILD_X64=1
                        MERGE_LIB=0
                        ;;
                    aarch64|arm64)
                        ARCH="aarch64"
                        BUILD_ARM64=1
                        BUILD_X64=0
                        MERGE_LIB=0
                        ;;
                    universal)
                        ARCH="universal"
                        BUILD_ARM64=1
                        BUILD_X64=1
                        MERGE_LIB=1
                        ;;
                    *)
                        echo "Invalid arch argument, please use --arch=universal or --arch=x86_64, --arch=x64 or --arch=aarch64, --arch=arm64"
                        exit 1
                        ;;
                esac
                ;;
            config)
                val_lower=$(echo "$value" | tr '[:upper:]' '[:lower:]')
                case "$val_lower" in
                    debug)
                        BUILD_TYPE="Debug"
                        ;;
                    release)
                        BUILD_TYPE="Release"
                        ;;
                    relwithdebinfo)
                        BUILD_TYPE="RelWithDebInfo"
                        ;;
                    *)
                        echo "Invalid build config argument, please use --config=Debug, --config=Release or --config=RelWithDebInfo"
                        exit 1
                        ;;
                esac
                ;;
            libname)
                SDK_LIB_NAME="$value"
                ;;
            *)
                echo "Ignore the current unsupported parameter. key=$key,value=$value"
                ;;
        esac
    else
        echo "Invalid parameter format: $arg"
        echo "Please use -key=value format (e.g., --arch=universal --config=Release --libName=OrbbecSDK)"
        exit 1
    fi
done

echo "Building ${SDK_LIB_NAME}..."
echo ">>>>> build arch: ${ARCH}"
echo ">>>>> build config: ${BUILD_TYPE}"
echo ">>>>> SDK library name: ${SDK_LIB_NAME}"
echo ""

# Variables for version and timestamp
VERSION=$(grep -o "project(.* VERSION [0-9]*\.[0-9]*\.[0-9]*" ${PROJECT_ROOT}/CMakeLists.txt | grep -o "[0-9]*\.[0-9]*\.[0-9]*")
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

    echo "Building ${SDK_LIB_NAME} for macos_${arch}"

    # Create build directory
    rm -rf ${build_dir}
    mkdir -p ${build_dir}
    cd ${build_dir} || exit

    # Create target directory for installation
    rm -rf ${install_dir}
    mkdir -p ${install_dir}

    # Build and install OrbbecSDK
    cmake_script="cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                -DCMAKE_OSX_ARCHITECTURES=${arch} \
                -DCMAKE_INSTALL_PREFIX=${install_dir} \
                -DOB_BUILD_SOVERSION=ON \
                -DOPEN_SDK_LIB_NAME=${SDK_LIB_NAME} \
                -DOB_BUILD_EXAMPLES=OFF"

    ${cmake_script} || { echo 'Failed to run cmake'; exit 1; }
    make install -j8 || { echo 'Failed to build OpenOrbbecSDK'; exit 1; }
    
    cd ..

    echo "Done building ${SDK_LIB_NAME} for macos_${arch}"
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
PACKAGE_NAME_ARM64="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_ARM64}"
PACKAGE_NAME_X64="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_X64}"
PACKAGE_NAME_UNIVERSAL="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM_UNIVERSAL}"

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

remove_copyright_info() {
    local target_dir="$1"
    if [ "${SDK_LIB_NAME}" != "${DEFAULT_LIB_NAME}" ]; then
        if [ -d "${target_dir}" ]; then
            echo "Removing copyright info from: ${target_dir}"
            local file_count=0
            find "${target_dir}" -type f \( -name "*.sh" -o -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.txt" \) | while read -r file; do
                echo "Process file: ${file}"
                # macOS BSD sed
                sed -i '' '/Copyright/d;/Licensed/d' "${file}"
                ((file_count++))
            done
        else
            echo "Error: Directory not found - ${target_dir}"
        fi
    fi
}

if [ "${MERGE_LIB}" != "0" ]; then
    # multiple architectures. merge all library/execute files to one

    # copy arm64 dir to new install dir
    rm -fr ${BUILD_DIR_UNIVERSAL}
    rm -fr ${INSTALL_DIR_UNIVERSAL}
    mkdir -p ${INSTALL_DIR_UNIVERSAL}
    cp -R -P ${INSTALL_DIR_ARM64}/ ${INSTALL_DIR_UNIVERSAL}/

    # merge library
    FULL_LIB_NAME=lib${SDK_LIB_NAME}.${VERSION}.dylib
    OLD_PATH_ARM64=${INSTALL_DIR_ARM64}/lib/${FULL_LIB_NAME}
    OLD_PATH_X64=${INSTALL_DIR_X64}/lib/${FULL_LIB_NAME}
    NEW_PATH=${INSTALL_DIR_UNIVERSAL}/lib/${FULL_LIB_NAME}
    lipo -create "${OLD_PATH_ARM64}" "${OLD_PATH_X64}" -output "${NEW_PATH}" || { echo 'Failed to merge arm64 and x86_64 libraries to one'; exit 1; }
    
    # merge other files, such as ob_benchmark
    lipo -create "${INSTALL_DIR_ARM64}/bin/ob_benchmark" "${INSTALL_DIR_X64}/bin/ob_benchmark" -output "${INSTALL_DIR_UNIVERSAL}/bin/ob_benchmark" || { echo 'Failed to merge arm64 and x86_64 libraries to one'; exit 1; }

    # remove copyright info
    remove_copyright_info "${INSTALL_DIR_UNIVERSAL}"

    # Compress the installation directory
    cd ${INSTALL_DIR_UNIVERSAL}
    cd ..
    zip -rpy ${PACKAGE_NAME_UNIVERSAL}.zip ${PACKAGE_NAME_UNIVERSAL} || { echo 'Failed to compress installation directory'; exit 1; }

    echo "Done compressing ${SDK_LIB_NAME} for ${PLATFORM_UNIVERSAL}"
else
    # single architecture

    # remove copyright info
    remove_copyright_info "${INSTALL_DIR_UNIVERSAL}"

    # Compress the installation directory
    cd ${INSTALL_DIR}
    cd ..
    zip -rpy ${PACKAGE_NAME}.zip ${PACKAGE_NAME} || { echo 'Failed to compress installation directory'; exit 1; }

    echo "Done compressing ${SDK_LIB_NAME} for ${PLATFORM}"
fi

cd ${CURRNET_DIR}
