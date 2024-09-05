# Installation and Development Guide

This is a comprehensive guide on installing the SDK, setting up the development environment, running the sample programs and developing the application.

## Step-by-Step Guide

### 1.Download the precompiled SDK package

- Download the zip package "OrbbecSDK_v2.x.x_xxx.zip" from [https://github.com/orbbec/OrbbecSDK-dev/releases](https://github.com/orbbec/OrbbecSDK-dev/releases).
- Unzip the OrbbecSDK to the directory you want

### 2.Environment Setup

#### windows

For windows, you need to register the metadata associated with frames (this includes things like timestamps and other information about the video frame).

- Metadata registration follow this:[/scripts/env_setup/obsensor_metadata_win10.md](/scripts/env_setup/obsensor_metadata_win10.md)

#### Linux

For Linux, we have provided a script to help you set up the environment. You can run the script as follows:

```bash
cd scripts/env_setup
./setup.sh
```

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

Connect the device to the computer and run the sample program at your unzip directory.

```shell
cd bin
./ob_color  #  ./ob_color for linux
```

> More information about the samples of Orbbec SDK, please refer to [samples](/examples/README.md).

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

### 2.Add the OrbbecSDK to the Project

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
    └── orbbecsdk
        └── (Orbbec SDK library files)
```

### 3.Config the CMakeLists.txt to Link the OrbbecSDK

Add the following lines to the CMakeLists.txt file:

```cmake
add_executable(MyOrbbecApp src/main.cpp)

set(OrbbecSDK_DIR lib/orbbecsdk/lib)
find_package(OrbbecSDK REQUIRED)

target_link_libraries(MyOrbbecApp ob::OrbbecSDK)
```
