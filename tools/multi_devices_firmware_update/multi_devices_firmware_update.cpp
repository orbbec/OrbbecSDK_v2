// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Inline minimal utils functions to remove dependency on examples/utils
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#endif

#include <libobsensor/ObSensor.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>

// Constants for magic numbers
namespace {
constexpr uint32_t MS_PER_SECOND            = 1000;
constexpr uint32_t US_PER_MS                = 1000;
constexpr uint32_t POLL_INTERVAL_US         = 100;
constexpr uint32_t MAX_RECONNECT_TIMEOUT_MS = 12000;
constexpr uint32_t POLL_INTERVAL_MS         = 1000;
constexpr size_t   MIN_EXTENSION_LENGTH     = 4;
constexpr uint32_t CURSOR_MOVE_UP_LINES     = 3;
}  // namespace

// Internal implementation of waitForKeyPressed
static char obSmplWaitForKeyPressInternal(uint32_t timeoutMs) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if(hStdin == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD mode = 0;
    if(!GetConsoleMode(hStdin, &mode)) {
        return 0;
    }
    mode &= ~ENABLE_ECHO_INPUT;
    if(!SetConsoleMode(hStdin, mode)) {
        return 0;
    }
    DWORD startTime = GetTickCount();
    while(true) {
        if(_kbhit()) {
            char ch = (char)_getch();
            SetConsoleMode(hStdin, mode);
            return ch;
        }
        if(timeoutMs > 0 && GetTickCount() - startTime > timeoutMs) {
            SetConsoleMode(hStdin, mode);
            return 0;
        }
        Sleep(1);
    }
#else
    struct timeval te;
    long long      startTime;
    gettimeofday(&te, NULL);
    startTime = te.tv_sec * MS_PER_SECOND + te.tv_usec / US_PER_MS;

    struct termios oldt, newt;
    int            ch;
    int            oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    while(true) {
        ch = getchar();
        if(ch != EOF) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            fcntl(STDIN_FILENO, F_SETFL, oldf);
            return (char)ch;
        }
        gettimeofday(&te, NULL);
        long long currentTime = te.tv_sec * MS_PER_SECOND + te.tv_usec / US_PER_MS;
        if(timeoutMs > 0 && currentTime - startTime > timeoutMs) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            fcntl(STDIN_FILENO, F_SETFL, oldf);
            return 0;
        }
        usleep(POLL_INTERVAL_US);
    }
#endif
}

// Internal implementation of supportAnsiEscape
static bool obSmplSupportAnsiEscapeInternal() {
#ifdef _WIN32
    if(_isatty(_fileno(stdout)) == 0) {
        return false;
    }
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if(hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    if(!GetConsoleMode(hOut, &mode)) {
        return false;
    }
    if((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
        return false;
    }
    return true;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

// Provide namespace-compatible wrappers
namespace obSmpl {
inline char waitForKeyPressed(uint32_t timeoutMs = 0) {
    return obSmplWaitForKeyPressInternal(timeoutMs);
}
inline bool supportAnsiEscape() {
    return obSmplSupportAnsiEscapeInternal();
}
}  // namespace obSmpl

bool getFirmwarePathFromCommandLine(int argc, char **argv, std::string &firmwarePath);
void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent);
void printDeviceList();
void updateFirmware(std::shared_ptr<ob::Device> device, const std::string &firmwarePath, bool &updateSucceeded);

// Device upgrade context structure
struct DeviceUpgradeContext {
    std::shared_ptr<ob::Device> device;
    std::string                 serialNumber;
    std::string                 name;
    bool                        needReUpdate    = false;
    bool                        firstUpdateDone = false;
    bool                        finalSuccess    = false;
    bool                        finalMismatch   = false;
    bool                        finalFailure    = false;
};

bool firstCall           = true;
bool needReUpdate        = false;
bool ansiEscapeSupported = false;
bool updateFailed        = false;  // Flag to track if update failed
bool isMismatch          = false;  // Flag to track if firmware mismatch

std::vector<DeviceUpgradeContext> totalDevices{};
std::vector<DeviceUpgradeContext> successDevices{};
std::vector<DeviceUpgradeContext> mismatchDevices{};
std::vector<DeviceUpgradeContext> failedDevices{};

// For device reconnection
std::mutex                  reconnectMutex;
std::condition_variable     reconnectCv;
std::shared_ptr<ob::Device> deviceAfterReboot;
std::string                 currentReconnectSerial;

int main(int argc, char *argv[]) try {
    std::string firmwarePath;
    if(!getFirmwarePathFromCommandLine(argc, argv, firmwarePath)) {
        std::cout << "Press any key to exit..." << std::endl;
        exit(EXIT_FAILURE);
    }

    ansiEscapeSupported = obSmpl::supportAnsiEscape();

    // Create a context to access the devices
    std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();

#if defined(__linux__)
    // On Linux, it is recommended to use the libuvc backend for device access as v4l2 is not always reliable on some systems for firmware update.
    context->setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    // Query the device list
    std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No device found. Please connect a device first!" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        obSmpl::waitForKeyPressed();
        exit(EXIT_FAILURE);
    }

    // Get all devices
    for(uint32_t i = 0; i < deviceList->getCount(); ++i) {
        try {
            DeviceUpgradeContext ctx;
            auto                 dev = deviceList->getDevice(i);
            ctx.device               = dev;
            ctx.serialNumber         = dev->getDeviceInfo()->getSerialNumber();
            ctx.name                 = dev->getDeviceInfo()->getName();
            totalDevices.push_back(ctx);
        }
        catch(ob::Error &e) {
            std::cerr << "Failed to get device, ignoring it." << std::endl;
            std::cerr << "Error message: " << e.what() << std::endl;
        }
    }
    printDeviceList();

    // Set device change callback for detecting device reconnection
    context->setDeviceChangedCallback([&](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> addedList) {
        (void)removedList;  // Unused parameter
        std::shared_ptr<ob::Device> device;
        try {
            std::lock_guard<std::mutex> lock(reconnectMutex);
            if(!currentReconnectSerial.empty()) {
                device = addedList->getDeviceBySN(currentReconnectSerial.c_str());
                if(device) {
                    deviceAfterReboot = device;
                    reconnectCv.notify_one();
                }
            }
        }
        catch(ob::Error &e) {
            std::cerr << "Device change callback error: " << e.what() << std::endl;
        }
    });

    // Upgrade each device
    for(uint32_t i = 0; i < totalDevices.size(); ++i) {
        firstCall    = true;
        needReUpdate = false;

        // Reset device context statuss
        totalDevices[i].needReUpdate    = false;
        totalDevices[i].firstUpdateDone = false;
        totalDevices[i].finalSuccess    = false;
        totalDevices[i].finalMismatch   = false;
        totalDevices[i].finalFailure    = false;

        try {
            std::cout << "\nUpgrading device: " << i + 1 << "/" << totalDevices.size() << " - " << totalDevices[i].name << std::endl;

            // Upgrade the device
            bool updateSucceeded = false;
            updateFirmware(totalDevices[i].device, firmwarePath, updateSucceeded);

            if(!updateSucceeded) {
                // Update failed - check if it's a mismatch or other error
                if(isMismatch) {
                    totalDevices[i].finalMismatch = true;
                }
                else {
                    totalDevices[i].finalFailure = true;
                }
            }

            // Check if second update is required
            if(needReUpdate) {
                // Some devices require another update after a reboot
                std::cout << "Firmware update completed but not finalized. The device will reboot now and then perform the second update..." << std::endl;
                totalDevices[i].device->reboot();
                totalDevices[i].device = nullptr;  // Invalidate the old device handle

                // Set the serial number to wait for
                {
                    std::unique_lock<std::mutex> lock(reconnectMutex);
                    currentReconnectSerial = totalDevices[i].serialNumber;
                }

                // Wait for device to come back online - auto retry with timeout
                const uint32_t maxWaitTimeMs  = MAX_RECONNECT_TIMEOUT_MS;
                const uint32_t pollIntervalMs = POLL_INTERVAL_MS;
                uint32_t       elapsedTime    = 0;
                bool           reconnected    = false;

                while(elapsedTime < maxWaitTimeMs) {
                    // Wait for callback notification (1 second timeout)
                    {
                        std::unique_lock<std::mutex> lock(reconnectMutex);
                        reconnectCv.wait_for(lock, std::chrono::milliseconds(pollIntervalMs), [&]() { return deviceAfterReboot != nullptr; });
                    }

                    if(deviceAfterReboot != nullptr) {
                        // Device reconnected via callback
                        std::cout << "\nThe device is online now, starting the second update..." << std::endl;
                        totalDevices[i].device = deviceAfterReboot;
                        reconnected            = true;
                        break;
                    }

                    // Auto poll device list
                    try {
                        auto newDeviceList = context->queryDeviceList();
                        auto newDevice     = newDeviceList->getDeviceBySN(totalDevices[i].serialNumber.c_str());
                        if(newDevice) {
                            deviceAfterReboot = newDevice;
                            std::cout << "\nThe device is online now, starting the second update..." << std::endl;
                            totalDevices[i].device = deviceAfterReboot;
                            reconnected            = true;
                            break;
                        }
                    }
                    catch(...) {
                        // Device not found yet, continue waiting
                    }

                    elapsedTime += pollIntervalMs;
                    std::cout << "Waiting for device to reconnect... (" << elapsedTime << "/" << maxWaitTimeMs << " ms)" << std::endl;
                }

                if(!reconnected) {
                    std::cout << "\nDevice failed to reconnect within timeout. Marking as failure." << std::endl;
                    totalDevices[i].finalFailure = true;
                }

                // Clear reconnection state
                {
                    std::lock_guard<std::mutex> lock(reconnectMutex);
                    currentReconnectSerial = "";
                    deviceAfterReboot      = nullptr;
                }

                // Perform second update if device is available
                if(totalDevices[i].device != nullptr && !totalDevices[i].finalFailure) {
                    needReUpdate               = false;
                    firstCall                  = true;
                    bool secondUpdateSucceeded = false;
                    updateFirmware(totalDevices[i].device, firmwarePath, secondUpdateSucceeded);
                    if(!secondUpdateSucceeded) {
                        if(isMismatch) {
                            totalDevices[i].finalMismatch = true;
                        }
                        else {
                            totalDevices[i].finalFailure = true;
                        }
                    }
                }
            }

            // Record final status
            if(needReUpdate == false && totalDevices[i].firstUpdateDone) {
                totalDevices[i].finalSuccess = true;
            }
        }
        catch(ob::Error &e) {
            // Unexpected situations, such as device disconnection, will typically throw an exception.
            // Note that common issues like verification failures are usually reported through the callback status.
            std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType()
                      << std::endl;
            totalDevices[i].finalFailure = true;
        }

        // Add to result lists
        if(totalDevices[i].finalSuccess) {
            successDevices.push_back(totalDevices[i]);
        }
        else if(totalDevices[i].finalMismatch) {
            mismatchDevices.push_back(totalDevices[i]);
        }
        else if(totalDevices[i].finalFailure) {
            failedDevices.push_back(totalDevices[i]);
        }
    }

    std::cout << "\nUpgrade Summary:\n";
    std::cout << "==================================================\n";

    std::cout << "Success (" << successDevices.size() << "):\n";
    for(const auto &ctx: successDevices) {
        std::cout << "  - Name: " << ctx.name << " | SN: " << ctx.serialNumber << std::endl;
    }

    std::cout << "\nMismatch (" << mismatchDevices.size() << "):\n";
    for(const auto &ctx: mismatchDevices) {
        std::cout << "  - Name: " << ctx.name << " | SN: " << ctx.serialNumber << std::endl;
    }
    if(mismatchDevices.size() > 0) {
        std::cout << "Please check use the correct firmware version and retry the upgrade." << std::endl;
    }

    std::cout << "\nFailure (" << failedDevices.size() << "):\n";
    for(const auto &ctx: failedDevices) {
        std::cout << "  - Name: " << ctx.name << " | SN: " << ctx.serialNumber << std::endl;
    }

    std::cout << "\nUpgrade process completed." << std::endl;
    for(auto &ctx: successDevices) {
        try {
            ctx.device->reboot();
        }
        catch(ob::Error &e) {
            std::cerr << "Failed to reboot device: " << ctx.name << ", error: " << e.what() << std::endl;
        }
    }

    std::cout << "Press any key to exit..." << std::endl;
    obSmpl::waitForKeyPressed();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    obSmpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent) {
    if(firstCall) {
        firstCall = false;
    }
    else {
        if(ansiEscapeSupported) {
            std::cout << "\033[" << CURSOR_MOVE_UP_LINES << "F";  // Move cursor up 3 lines
        }
    }

    if(ansiEscapeSupported) {
        std::cout << "\033[K";  // Clear the current line
    }
    std::cout << "Progress: " << static_cast<uint32_t>(percent) << "%" << std::endl;

    if(ansiEscapeSupported) {
        std::cout << "\033[K";
    }
    std::cout << "Status  : ";
    switch(state) {
    case STAT_VERIFY_SUCCESS:
        std::cout << "Image file verification success" << std::endl;
        break;
    case STAT_FILE_TRANSFER:
        std::cout << "File transfer in progress" << std::endl;
        break;
    case STAT_DONE:
        std::cout << "Update completed" << std::endl;
        break;
    case STAT_DONE_REBOOT_AND_REUPDATE:
        needReUpdate = true;
        std::cout << "Update completed" << std::endl;
        break;
    case STAT_DONE_WITH_DUPLICATES:
        std::cout << "Update completed (some files were duplicated and ignored)" << std::endl;
        break;
    case STAT_IN_PROGRESS:
        std::cout << "Upgrade in progress" << std::endl;
        break;
    case STAT_START:
        std::cout << "Starting the upgrade" << std::endl;
        break;
    case STAT_VERIFY_IMAGE:
        std::cout << "Verifying image file" << std::endl;
        break;
    case ERR_MISMATCH:
        std::cout << "Mismatch between device and image file" << std::endl;
        break;
    default:
        std::cout << "Unknown status or error" << std::endl;
        break;
    }

    if(ansiEscapeSupported) {
        std::cout << "\033[K";
    }
    std::cout << "Message : " << message << std::endl << std::flush;

    if(state == STAT_DONE || state == STAT_DONE_WITH_DUPLICATES) {
        // First update completed successfully
        needReUpdate = false;
    }
    else if(state == STAT_DONE_REBOOT_AND_REUPDATE) {
        // Device requires a second update after reboot
        needReUpdate = true;
    }
    else if(state == ERR_MISMATCH) {
        // If the device's firmware version does not match the image file, the callback status will be ERR_MISMATCH.
        // Set the flag for mismatch - will be handled by caller
        updateFailed = true;
        isMismatch   = true;
    }
    else if(state < 0) {
        // While state < 0, it means an error occurred.
        // Error will be handled by caller
        updateFailed = true;
    }
}

bool getFirmwarePathFromCommandLine(int argc, char **argv, std::string &firmwarePath) {
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <firmware_file_path>" << std::endl;
        std::cerr << "Example: " << argv[0] << " /path/to/firmware.bin" << std::endl;
        return false;
    }

    std::vector<std::string> validExtensions = { ".bin", ".img" };
    firmwarePath                             = argv[1];

    if(firmwarePath.size() > MIN_EXTENSION_LENGTH) {
        std::string extension = firmwarePath.substr(firmwarePath.size() - MIN_EXTENSION_LENGTH);

        auto result = std::find_if(validExtensions.begin(), validExtensions.end(),
                                   [extension](const std::string &validExtension) { return extension == validExtension; });
        if(result != validExtensions.end()) {
            std::cout << "Firmware file confirmed: " << firmwarePath << std::endl << std::endl;
            return true;
        }
    }

    std::cout << "Invalid input file: Please provide a valid firmware file, supported formats: ";
    for(const auto &ext: validExtensions) {
        std::cout << ext << " ";
    }
    std::cout << std::endl;
    return false;
}

void printDeviceList() {
    std::cout << "Devices found:" << std::endl;
    std::cout << "--------------------------------------------------------------------------------\n";
    for(uint32_t i = 0; i < totalDevices.size(); ++i) {
        std::cout << "[" << i << "] " << "Device: " << totalDevices[i].name;
        std::cout << " | SN: " << totalDevices[i].serialNumber;
        std::cout << " | Firmware version: " << totalDevices[i].device->getDeviceInfo()->getFirmwareVersion() << std::endl;
    }
    std::cout << "---------------------------------------------------------------------------------\n";
}

void updateFirmware(std::shared_ptr<ob::Device> device, const std::string &firmwarePath, bool &updateSucceeded) {
    updateSucceeded = false;
    std::cout << "\nUpgrading device firmware, please wait for a moment...\n" << std::endl;
    try {
        // Reset flags for new firmware update
        needReUpdate = false;
        firstCall    = true;
        updateFailed = false;
        isMismatch   = false;
        // Perform synchronous firmware update
        device->updateFirmware(firmwarePath.c_str(), firmwareUpdateCallback, false);

        // Check if update failed (e.g., firmware mismatch)
        if(updateFailed) {
            std::cerr << "Error: Firmware update failed due to firmware mismatch or invalid firmware file." << std::endl;
            return;
        }

        updateSucceeded = true;
        // Mark first update as done
        for(auto &ctx: totalDevices) {
            if(ctx.device == device) {
                ctx.firstUpdateDone = true;
                break;
            }
        }
    }
    catch(ob::Error &e) {
        // If the update fails, will throw an exception.
        std::cerr << "Error: The upgrade was interrupted! An error occurred! " << std::endl;
        std::cerr << "       Error message: " << e.what() << std::endl;
    }
    catch(const std::runtime_error &e) {
        // Handle callback-reported failures (e.g., ERR_MISMATCH)
        std::cerr << "Error: Firmware update failed! " << std::endl;
        std::cerr << "       Error message: " << e.what() << std::endl;
    }
    std::cout << std::endl;
}