# C++ Sample: 4.msic.loggger

## Overview
Use the SDK context class or modify the XML configuration file to set the SDK log level, including the log level output to the terminal and the log level output to the file.

### Knowledge
Context, which serves as the entry point to the underlying SDK. It is used to query device lists, handle device callbacks, and set the log level.

## Code Overview

1. Configure the log level output to the terminal

    ```cpp
    // Configure the log level output to the terminal.
    ob::Context::setLoggerToConsole(OB_LOG_SEVERITY_ERROR);
    ```

2. Configure the log level output to the file.
The default output path settings(Windows/Linux) is `${CWD}/Log/OrbbecSDK.log`, where `${CWD}` represents the Current Working Directory. For Android, the path is `/sdcard/orbbec/Log`.

    ```cpp
    // Configure the log level and path output to the file.
    // The first parameter is the log level, and the second parameter is the output path.
    // If the output path is empty, the existing settings will continue to be used (if the existing configuration is also empty, the log will not be output to the file).
    ob::Context::setLoggerToFile(OB_LOG_SEVERITY_DEBUG, "Log/Custom/");
    ```

3. Registering a log callback

    ```cpp
    // Register a log callback, you can get log information in the callback.
    // The first parameter is the log level, and the second parameter is the callback function.
    ob::Context::setLoggerToCallback(OB_LOG_SEVERITY_DEBUG, [](OBLogSeverity severity, const char *logMsg) {
        std::cout << "[CallbackMessage][Level:" << severity << "]" << logMsg;
    });
    ```

## Configuration via XML

### Priority of Log Level Configuration
**1. API Overrides:**
If the log level is set through the SDK's API, it overrides the XML configuration. The final log level will always follow the API setting, even if the XML file specifies a different level.

**2. XML Configuration Priority:**
The <Log> node in the XML file sets the default log level and output parameters. This setting is used if the log level is not explicitly set via the API.

### Overview of XML Configuration
The log level and other logging parameters can be configured via the XML configuration file. After compiling and installing the project using `cmake install`, you will find the configuration files (`OrbbecSDKConfig.xml` and `OrbbecSDKConfig.md`) in the following locations:

- `bin` directory
- `shared` directory

Additionally, the original configuration file can be located in the source directory at:
```bash
    OrbbecSDK_v2/src/shared/environment/OrbbecSDKConfig.xml
```
**To ensure proper loading, place the `OrbbecSDKConfig.xml` file in the same directory as your executable.**

### Log Level Configuration
Open the `OrbbecSDKConfig.xml` file and locate the `<Log>` node. The configuration should look like the following example:

```xml
    <Log>
        <!-- Log output level, int type, optional values: 0-DEBUG, 1-INFO, 2-WARN, 3-ERROR, 4-FATAL,
        5-OFF -->
        <!-- File log output level -->
        <FileLogLevel>5</FileLogLevel>
        <!-- Console log output level -->
        <ConsoleLogLevel>3</ConsoleLogLevel>
        <!-- Default log output file path, string type. If this item is not configured, the default
        path will be used: Win/Linux: "./Log"; Android: "/sdcard/orbbec/Log" -->
        <!-- <OutputDir>./log</OutputDir> -->
        <!-- Default log output file size, int type, unit: MB -->
        <MaxFileSize>100</MaxFileSize>
        <!-- Default log output file number (circular overwrite), int type -->
        <MaxFileNum>3</MaxFileNum>
        <!-- Log asynchronous output, changing to asynchronous output can reduce the blocking time
        of printing logs, but some logs may be lost when the program exits abnormally; true-enable,
        false-disable (default) -->
        <Async>false</Async>
    </Log>
```

#### Configuration Details
**1. Log Levels**:
- `FileLogLevel`: Controls the logging level for file output.
- `ConsoleLogLevel`: Controls the logging level for console output.

**2. File Output Parameters**:  
   - `MaxFileSize`: Maximum size of a single log file in MB.  
   - `MaxFileNum`: Maximum number of log files before old logs are overwritten (circular overwrite).

**3. Asynchronous Logging**:  
- Enabling asynchronous logging (`<Async>true</Async>`) can reduce blocking during log output but may result in log loss if the program exits abnormally.

## Run Sample
If you are on Windows, you can switch to the directory `OrbbecSDK_v2/build/win_XX/bin` to find the `ob_logger.exe`.

If you are on linux, you can switch to the directory `OrbbecSDK_v2/build/linux_XX/bin` to find the `ob_logger`.

### Result
![result](/docs/resource/logger.jpg)
