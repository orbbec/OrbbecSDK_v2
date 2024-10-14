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

# run docker container and build openorbbecsdk
docker run --rm -u $USER_ID:$GROUP_ID \
    -v $PROJECT_ROOT/../:/workspace \
    -w /workspace/$FOLDER_NAME \
    --name OpenOrbbecSDK_Build_Liunx_$ARCH \
    -it \
    --entrypoint /bin/bash \
    openorbbecsdk-env.$ARCH \
    -c "cd /workspace/$FOLDER_NAME && bash ./scripts/build/build_linux.sh"

echo "Done building openorbbecsdk for linux $ARCH via docker"

