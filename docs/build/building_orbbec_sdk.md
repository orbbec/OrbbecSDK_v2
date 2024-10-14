# Building Orbbec SDK

## Requirements

For windows, you need:

- Windows 10 or later
- Visual Studio 2017 or later with MSVC v141 toolset
- CMake v3.15 or later
- Git
- OpenCV v3.4 is recommended

For Linux, it is recommended to use Docker to build the Orbbec SDK, therefor, you only need:

- Docker

For linux, if you want to build Orbbec SDK by using native toolchain, you need:

- Ubuntu 18.04 or later
- GCC 7.4
- CMake v3.15 or later
- Git
- OpenCV v3.4 is recommended

## Clone Orbbec SDK

The Orbbec SDK souce code are available on GitHub. You can clone the Orbbec SDK by using the following commands:

```shell
git clone https://github.com/Orbbec/OrbbecSDK-dev.git
```

## Build Orbbec SDK

You can build Orbbec SDK by using the following commands:

- Windows: Use PowerShell to execute the build commands.

- Linux: Use a terminal emulator to execute the build commands

```shell
cd OrbbecSDK-dev
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

If you are using Docker, you can using the script we provide to build Orbbec SDK.

```shell
cd OrbbecSDK-dev/scripts/build
./build_linux_docker.sh
# `./build_linux_docker.sh  aarch64` for arm64
```

> More information about how to use Docker, please refer to [README.md](../../scripts/docker/README.md).

## Run the Sample

After you build the Orbbec SDK, you can find the executable file in the directory `OrbbecSDK-dev/build/win_XX/bin` or `OrbbecSDK-dev/build/linux_XX/bin`.

### Environment Setup

#### Windows

##### Metadata registration

For windows,you need to register the metadata associated with frames (this includes things like timestamps and other information about the video frame).

- Metadata registration follow this:[/scripts/env_setup/obsensor_metadata_win10.md](../..//scripts/env_setup/obsensor_metadata_win10.md)

#### Linux

 Please install it using the following commands:[/scripts/env_setup/install_udev_rules.sh](../../scripts/env_setup/install_udev_rules.sh)

```shell
cd OrbbecSDK-dev/scripts/env_setup
sudo chmod +x ./install_udev_rules.sh
sudo ./install_udev_rules.sh
sudo udevadm control --reload-rules && udevadm trigger
```

### Connect to the camera

If the camera needs to be powered on, plug in the power cable and connect the camera to the host using USB.

### Running the sample

- Enter the bin directory , open the terminal (power shell for windows), and run the example

```shell
cd bin
.\ob_XXX  #  .\ob_XXX.exe for windows
```

> More information about the examples of Orbbec SDK, please refer to [examples](../../examples/README.md).
