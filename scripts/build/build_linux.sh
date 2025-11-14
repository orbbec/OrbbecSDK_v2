#!/usr/bin/env bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)

SYSTEM=linux
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    PLATFORM=${SYSTEM}_arm64
elif [ "$ARCH" = "armv7l" ]; then
    PLATFORM=${SYSTEM}_arm32
else
    PLATFORM=${SYSTEM}_$ARCH
fi

# Default args
DEFAULT_LIB_NAME="OrbbecSDK"
SDK_LIB_NAME=$DEFAULT_LIB_NAME
BUILD_TYPE="Release"

# User args
for arg in "$@"; do
    if [[ "$arg" =~ ^--?([^=]+)=(.+)$ ]]; then
        key="${BASH_REMATCH[1],,}"
        value="${BASH_REMATCH[2]}"

        case "$key" in
            config)
                val_lower="${value,,}"
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
        echo "Please use -key=value format (e.g., --config=Release --libName=OrbbecSDK)"
        exit 1
    fi
done

echo "Building $SDK_LIB_NAME for $PLATFORM"
echo "Build Configuration:"
echo ">>>>> build config: $BUILD_TYPE"
echo ">>>>> SDK library name: $SDK_LIB_NAME"
echo ""

# Variables for version and timestamp
VERSION=$(grep -oP 'project\(.*\s+VERSION\s+\d+\.\d+\.\d+' $PROJECT_ROOT/CMakeLists.txt | grep -oP '\d+\.\d+\.\d+')
TIMESTAMP=$(date +"%Y%m%d%H%M")
COMMIT_HASH=$(git rev-parse --short HEAD)
PACKAGE_NAME="${SDK_LIB_NAME}_v${VERSION}_${TIMESTAMP}_${COMMIT_HASH}_${PLATFORM}"

# Create build directory
rm -rf build_$PLATFORM
mkdir -p build_$PLATFORM
cd build_$PLATFORM || exit

# Create target directory for installation
INSTALL_DIR=$(pwd)/install/$PACKAGE_NAME
mkdir -p $INSTALL_DIR

# Build and install OrbbecSDK
cmake_script="cmake .. 
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
    -DOPEN_SDK_LIB_NAME=$SDK_LIB_NAME \
    -DOB_BUILD_EXAMPLES=OFF"

$cmake_script || { echo 'Failed to run cmake'; exit 1; }

make install -j8 || { echo 'Failed to build $SDK_LIB_NAME'; exit 1; }

# Remove copyright and license description for OEM build
if [ "$SDK_LIB_NAME" != "$DEFAULT_LIB_NAME" ]; then
    if [ -d "$INSTALL_DIR" ]; then
        find "$INSTALL_DIR" -type f \( -name "*.sh" -o -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.txt" \) | while read -r file; do
            echo "Process file: $file"
            sed -i '/Copyright/d;/Licensed/d' "$file"
	    done
    fi
fi

# Compress the installation directory
cd $INSTALL_DIR
cd ..
zip -rpy ${PACKAGE_NAME}.zip ${PACKAGE_NAME} || { echo 'Failed to compress installation directory'; exit 1; }

echo "Done building and compressing $SDK_LIB_NAME for $PLATFORM"
echo "Done building $SDK_LIB_NAME for $PLATFORM"

cd $CURRNET_DIR
