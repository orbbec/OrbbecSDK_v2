#!/bin/bash

# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

CURRNET_DIR=$(pwd)

SCRIPT_DIR=$(dirname "$0")
cd $SCRIPT_DIR/../../

PROJECT_ROOT=$(pwd)
cd $PROJECT_ROOT

FOLDER_NAME=$(basename "$PROJECT_ROOT")

# get arch from arg
ARCH=$1

if [ "$ARCH" == "" ]; then
    ARCH=$(uname -m)
fi

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

echo "Building openorbbecsdk for linux $ARCH via docker"


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
CONTAINER_NAME="OrbbecSDK_Linux_${ARCH}_$(date +%s)_$$"
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
    -c "cd /workspace/$FOLDER_NAME && bash ./scripts/build/build_linux.sh" &
DOCKER_PID=$!
wait $DOCKER_PID
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    echo "Failed to build openorbbecsdk for linux $ARCH via docker. Exit code: $EXIT_CODE"
    exit $EXIT_CODE
fi

echo "Done building openorbbecsdk for linux $ARCH via docker"
