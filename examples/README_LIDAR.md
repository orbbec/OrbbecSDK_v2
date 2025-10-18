## Introduction

There are several examples codes for users learn how to use Orbbec LiDARs. Here is a brief introduction to each sample code:

| Level | Sample                                                       | Description                                                  |
| ----- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| lidar | [lidar_quik_start](lidar_examples/0.lidar_quik_start)        | Use the SDK interface to quickly obtain LiDAR point cloud data and save it to a PLY file when triggered by user input. |
| lidar | [lidar_stream](lidar_examples/1.lidar_stream)                | This sample demonstrates how to configure and start LiDAR and IMU sensor streams, process the data frames through a callback function, and display sensor information. |
| lidar | [lidar_device_control](lidar_examples/2.lidar_device_control) | This sample demonstrates how to interactively control LiDAR device properties through a command-line interface, allowing users to get and set various device parameters. |
| lidar | [lidar_record](lidar_examples/3.lidar_record)                | This sample demonstrates how to record LiDAR and IMU sensor data to a bag file format, providing real-time data capture with pause/resume functionality for flexible recording sessions. |
| lidar | [lidar_playback](lidar_examples/4.lidar_playback)            | This sample demonstrates how to play back recorded LiDAR and IMU sensor data from bag files, providing full control over playback including pause/resume functionality and automatic replay capabilities. |

### C language examples

The listed examples at previous section are written in C++ language. Here is a brief introduction to c language examples:

| Level | Sample                                                    | Description                                                  |
| ----- | --------------------------------------------------------- | ------------------------------------------------------------ |
| lidar | [c_lidar_quik_start](lidar_examples/0.c_lidar_quik_start) | This C language sample demonstrates how to quickly obtain LiDAR point cloud data using the Orbbec SDK C API and save it to a PLY file when triggered by user input. |
| lidar | [c_lidar_stream](lidar_examples/1.c_lidar_stream)         | This C language sample demonstrates how to configure and stream LiDAR and IMU sensor data using the Orbbec SDK C API, providing interactive sensor selection and real-time data monitoring. |

## Append

### The utils functions and classes used in the examples code

***utils***

It contains functions to wait for a key press with an optional timeout, get the current time in milliseconds, and parse the input option from a key press event.This is done with C++.

***utils_c***

It contains functions to get the current system timestamp and wait for keystrokes from the user, as well as a macro to check for and handle errors. These capabilities can be used in scenarios such as time measurement, user interaction, and error handling.This is done with C.

### The Error Handling in the examples code

When using the Orbbec SDK, if an error occurs, the SDK reports the error by throwing an exception of type ob::Error. The ob::Error exception class typically contains detailed information about the error, which can help developers diagnose the problem.
The example uses a 'try' block to wrap the entire main function.If an exception of type ob::Error is thrown, the program will catch it and print the error message to the console.
Here is the information that can be obtained from an ob::Error:

**Function Name (getFunction()):**
Indicates the name of the function where the exception was thrown. This helps pinpoint the location of the error within the code.

**Arguments (getArgs()):**
Provides information about the arguments passed to the function when the exception occurred. This context can be useful for understanding the specific conditions under which the error happened.

**Error Message (what()):**
Returns a string describing the nature of the error. This is often the most important piece of information, as it directly explains what went wrong.

**Exception Type (getExceptionType()):**
Specifies the type of the exception. This can help categorize the error and determine appropriate handling strategies.Read the comments of the OBExceptionType enum in the libobsensor/h/ObTypes.h file for more information.

**Example Code** in C++

```cpp
catch(ob::Error &e) {
    std::cerr << "function: " << e.getFunction() << std::endl;
    std::cerr << "args: " << e.getArgs() << std::endl;
    std::cerr << "message: " << e.what() << std::endl;
    std::cerr << "type: " << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}
```
