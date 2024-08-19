# How to build Open Orbbec SDK

## Requirements

For windows, you need:

- Windows 10 or later
- Visual Studio 2017 or later with MSVC v141 toolset
- CMake v3.15 or later
- Git
- OpenCV v3.4 is recommended

For Linux, it is recommended to use Docker to build the Open Orbbec SDK, therefor, you only need:

- Docker

For linux, if you want to build Open Orbbec SDK by using native toolchain, you need:

- Ubuntu 18.04 or later
- GCC 7.4
- CMake v3.15 or later
- Git
- OpenCV v3.4 is recommended

## Build Open Orbbec SDK

You can build Open Orbbec SDK by using the following commands:

```shell
cd OpenOrbbecSDK && mkdir build && cd build && cmake .. && cmake --build . --config Release
```

If you are using Docker, you can using the script we provide to build Open Orbbec SDK.

```shell
cd OpenOrbbecSDK/scripts/build
./build_linux_docker.sh
```

> More information about how to use Docker, please refer to [README.md](/scripts/docker/README.md).

## Run the Sample

### Where to find the executable file

If you are on Windows, you can switch to the directory `/OpenOrbbecSDK/build/win_XX/bin` to find the `.exe`.

If you are on linux, you can switch to the directory `/OpenOrbbecSDK/build/linux_XX/bin` to find the `.exe`.

### Environment Setup

For windows, you need to register a timestamp.
Timestamp registration:[follow this](/scripts/env_setup/obsensor_metadata_win10.md)

For linux, if you installed via a debian package, you can skip the installation of the udev rules file. If not, please install it using the following commands:

```shell
cd OpenOrbbecSDK/scripts/env_setup
sudo chmod +x ./install_udev_rules.sh ./setup.sh
sudo ./install_udev_rules.sh
sudo ./setup.sh
sudo udevadm control --reload-rules && udevadm trigger
```

### Connect to the camera

If the camera needs to be powered on, plug in the power cable and connect the camera to the host using USB.

### Running the sample

Now, you can run the sample.
