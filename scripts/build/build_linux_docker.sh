#!/bin/bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)
cd $PROJECT_ROOT

FOLDER_NAME=$(basename "$PROJECT_ROOT")

# Default args
SDK_LIB_NAME="OrbbecSDK"
BUILD_TYPE="Release"
ARCH=$(uname -m)

# User args
for arg in "$@"; do
    if [[ "$arg" =~ ^--?([^=]+)=(.+)$ ]]; then
        key="${BASH_REMATCH[1],,}"
        value="${BASH_REMATCH[2]}"

        case "$key" in
            arch)
                val_lower="${value,,}"
                case "$val_lower" in
                    x86_64|x64)
                        ARCH=x86_64
                        ;;
                    aarch64|arm64)
                        ARCH=aarch64
                        ;;
                    *)
                        echo "Invalid arch argument, please use --arch=x86_64, --arch=x64 or --arch=aarch64, --arch=arm64"
                        exit 1
                        ;;
                esac
                ;;
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
        echo "Please use -key=value format (e.g., --arch=x86_64 --config=Release --libName=OrbbecSDK)"
        exit 1
    fi
done

if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "aarch64" ] && [ "$ARCH" != "arm64" ]  && [ "$ARCH" != "arm" ]; then
    echo "Invalid architecture: $ARCH, supported architectures are x86_64, aarch64, and arm"
    exit 1
fi

platform="linux/$ARCH"
if [ "$ARCH" == "x86_64" ]; then
    platform="linux/amd64"
elif [ "$ARCH" == "aarch64" ]; then
    platform="linux/arm64"
fi

echo "Building $SDK_LIB_NAME for linux $ARCH via docker"

# check if cross compiling
if [ "$ARCH" != $(uname -m) ]; then
    echo "Cross compiling, using docker buildx to build $ARCH image"

    # if docker buildx is not installed, install it
    if [ "$(docker buildx ls | grep default)" == "" ]; then
        echo "Installing docker buildx"
        docker buildx install
    fi

    # if docker buildx plarform does not exist, create it
    if [ "$(docker buildx ls | grep $platform)" == "" ]; then
        echo "Creating docker buildx platform $platform"
        docker run --privileged --rm tonistiigi/binfmt --install all
    fi
fi

# build docker image
cd $PROJECT_ROOT/scripts/docker
docker buildx build --platform $platform -t openorbbecsdk-env.$ARCH -f $PROJECT_ROOT/scripts/docker/$ARCH.dockerfile . --load || {
    echo "Failed to build docker image openorbbecsdk-env.$ARCH"
    exit 1
}
cd $CURRNET_DIR

USER_ID=$(id -u)
GROUP_ID=$(id -g)

TTY_OPT=""
if [ -t 1 ]; then
  TTY_OPT="-t"
fi

# Define a unique container name using architecture, current timestamp, and shell PID
CONTAINER_NAME="${SDK_LIB_NAME}_Linux_${ARCH}_$(date +%s)_$$"
# Cleanup function to remove container associated with the current build
cleanup() {
    echo "Cleaning up docker container $CONTAINER_NAME"
    docker rm -f "$CONTAINER_NAME" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# run docker container and build openorbbecsdk
docker run --rm -u $USER_ID:$GROUP_ID \
    -v $PROJECT_ROOT/../:/workspace \
    -w /workspace/$FOLDER_NAME \
    --name $CONTAINER_NAME \
    $TTY_OPT \
    --entrypoint /bin/bash \
    openorbbecsdk-env.$ARCH \
    -c "cd /workspace/$FOLDER_NAME && bash ./scripts/build/build_linux.sh \
    --config=$BUILD_TYPE --libName=$SDK_LIB_NAME" &
DOCKER_PID=$!
wait $DOCKER_PID
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo "Failed to build $SDK_LIB_NAME for linux $ARCH via docker. Exit code: $EXIT_CODE"
    exit $EXIT_CODE
fi

echo "Done building $SDK_LIB_NAME for linux $ARCH via docker"
