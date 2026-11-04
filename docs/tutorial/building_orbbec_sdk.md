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
git clone https://github.com/Orbbec/OrbbecSDK_v2.git
```

## Build Orbbec SDK

You can build Orbbec SDK by using the following commands:

- Windows: Use PowerShell to execute the build commands.

- Linux: Use a terminal emulator to execute the build commands

```shell
cd OrbbecSDK_v2
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

If you are using Docker, you can using the script we provide to build Orbbec SDK.

```shell
cd OrbbecSDK_v2/scripts/build
./build_linux_docker.sh
# `./build_linux_docker.sh  aarch64` for arm64
```

> More information about how to use Docker, please refer to [README.md](../../scripts/docker/README.md).

### Native Linux RISC-V 64-bit build

The SDK can be built with a native `riscv64` toolchain. For example, on
Debian/Ubuntu-based RISC-V systems, install the build dependencies with:

```shell
sudo apt-get install build-essential cmake git pkg-config libusb-1.0-0-dev libopencv-dev
```

From the repository root, configure, build and install into a local staging directory:

```shell
cmake -S . -B build_riscv64 -DCMAKE_BUILD_TYPE=Release -DOB_BUILD_DOCS=OFF
cmake --build build_riscv64 -j2
cmake --install build_riscv64 --prefix "$PWD/build_riscv64/install"
```

Executables and libraries are written to `build_riscv64/linux_riscv64/bin` and
`build_riscv64/linux_riscv64/lib`. Install the udev rules as described below
before running the USB examples as a regular user. GUI examples require a display.
The same architecture is recognized when cross-compiling with a CMake toolchain
that sets `CMAKE_SYSTEM_NAME=Linux` and `CMAKE_SYSTEM_PROCESSOR=riscv64`.

The proprietary `frameprocessor`, `filters`, `depthengine` and `firmwareupdater`
extensions currently have no RISC-V binaries. Missing RISC-V extension directories
are skipped during build and installation. When the frame processor library is
absent on RISC-V, frames use the existing pass-through copy path; this does not
provide the proprietary processing algorithms. A present but incompatible or
broken frame processor library still reports an error. Devices that require a
proprietary depth engine or host-side disparity conversion are not covered by
this fallback. Firmware updating and proprietary filters remain unavailable.
Software D2C/C2D alignment uses the scalar implementation on targets without
SSSE3 or NEON; its throughput depends on the selected resolution and calibration.

Hardware validation is limited to a SpacemiT K3 running Bianbu Linux with a
Gemini 335 connected over USB 3. Other RISC-V boards and camera models need
separate validation.

The scalar alignment regression tests run without a camera, including on x86
and ARM hosts:

```shell
cmake -S . -B build_riscv64 -DOB_BUILD_DOCS=OFF -DOB_BUILD_TESTS=ON
cmake --build build_riscv64 -j2 --target align_generic_test
ctest --test-dir build_riscv64 -R '^GenericAlign\.' --output-on-failure
```

## Run the Sample

After you build the Orbbec SDK, you can find the executable file in the directory `OrbbecSDK_v2/build/win_XX/bin` or `OrbbecSDK_v2/build/linux_XX/bin`.

### Environment Setup

#### Windows

##### Metadata registration

For windows,you need to register the metadata associated with frames (this includes things like timestamps and other information about the video frame).

- Metadata registration follow this:[/scripts/env_setup/obsensor_metadata_win10.md](../../scripts/env_setup/obsensor_metadata_win10.md)

#### Linux

 Please install it using the following commands:[/scripts/env_setup/install_udev_rules.sh](../../scripts/env_setup/install_udev_rules.sh)

```shell
cd OrbbecSDK_v2/scripts/env_setup
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
