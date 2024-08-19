# Tutorial

This is a comprehensive guide on installing the SDK, setting up the development environment, running the sample programs and developing the application.

## Step-by-Step Guide

### 1.Download and Install

- Download the zip package "openorbbecsdk_v2.x.x_xxx.zip" from [https://github.com/orbbec/OpenOrbbecSDK/releases](https://github.com/orbbec/OpenOrbbecSDK/releases).
- Unzip the openorbbecsdk to the directory you want

### 2.Environment Setup

#### windows

##### 1. Configure OpenCV

> Examples dependency, This is not required, but you will have to install OpenCV if you want to build the examples yourself.

Data rendering relies on the third-party library OpenCV. Here we take OpenCV 4.3.0 as an example to demonstrate the installation configuration.

- Execute the OpenCV installation file, select the directory where opencv is to be installed, and click extract to execute the installation.
- Add the path of OpenCV in the environment variables of the system, enter OpenCV_DIR for the variable name, pay attention to the capitalization of the letters, and the variable value is the path to the build folder of the OpenCV installation directory.

##### 2. Metadata registration

For windows,you need to register the metadata associated with frames (this includes things like timestamps and other information about the video frame).

- Metadata registration follow this:[/scripts/env_setup/obsensor_metadata_win10.md](/scripts/env_setup/obsensor_metadata_win10.md)

#### Linux

##### Execute the script setup.sh

**Purpose**:

1. Install compilation tools and dependencies (e.g., OpenCV).
2. Compile the sample program.
3. Install libusb rules file.

> It's not necessary. If you don't, there will be no sample executable, and you'll need to run install libusb rules file manually

**`setup.sh` Script Requirements**:

1. **Internet Connection**: The script may need to download compilation tools and dependencies from the internet.
2. **sudo Permissions**: The installation process may require system-level permissions to install software packages or modify system files.

**Expected Outcome**:

When the script executes successfully, it will generate the sample executable in the project's bin directory. This file is the result of compiling the sample program and can be used to demonstrate the functionality.

### 4.Run example

> link to sample readme [/examples/README.md](/examples/README.md)

- If the camera needs an independent power supply, plug it in first, and then connect the camera to the host computer with a usb cable.

- Enter the bin directory of the openorbbecsdk unzipper directory, open the terminal (power shell for windows), and run the example

```shell
cd bin
.\ob_color  #  .\ob_color.exe for windows
```

## Develop a New Application with the SDK

### 1.Create a new CMake Project

First, create a new folder for your project root directory and set up the basic CMake files and source code files within it.

Example Project Folder Structure:

```plaintext
MyOrbbecApp/
├── CMakeLists.txt
├── src/
│   └── main.cpp
└── include/
    └── (optional custom header files)
```

### 2.Add the OpenOrbbecSDK to the Project

Create a lib directory in the project and unpack the Orbbec SDK library files into this lib directory.

Example Project Folder Structure:

```plaintext
MyOrbbecApp/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── include/
│   └── (optional custom header files)
└── lib/
    └── openorbbecsdk
        └── (Orbbec SDK library files)
```

### 3.Config the CMakeLists.txt to Link the OpenOrbbecSDK

Add the following lines to the CMakeLists.txt file:

```cmake
add_executable(MyOrbbecApp src/main.cpp)

set(openobsdk_DIR lib/openorbbecsdk/lib)
find_package(openobsdk REQUIRED)

target_link_libraries(MyOrbbecApp ob::openobsdk)
```
