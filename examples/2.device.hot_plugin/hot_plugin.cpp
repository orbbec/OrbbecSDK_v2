// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <iomanip>
#include <iostream>

struct StreamStateGuard {
    std::ios          &ios;
    std::ios::fmtflags flags;
    char               fill;

    explicit StreamStateGuard(std::ios &s) : ios(s), flags(s.flags()), fill(s.fill()) {}

    ~StreamStateGuard() {
        ios.flags(flags);
        ios.fill(fill);
    }
};

void printDeviceList(const std::string &prompt, std::shared_ptr<ob::DeviceList> deviceList) {
    StreamStateGuard guard(std::cout);
    auto             count = deviceList->getCount();
    if(count == 0) {
        return;
    }
    std::cout << count << " device(s) " << prompt << ": " << std::endl;
    for(uint32_t i = 0; i < count; i++) {
        auto uid          = deviceList->getUid(i);
        auto vid          = deviceList->getVid(i);
        auto pid          = deviceList->getPid(i);
        auto serialNumber = deviceList->getSerialNumber(i);
        auto connection   = deviceList->getConnectionType(i);
        std::cout << " - uid: " << uid << ", vid: 0x" << std::hex << std::setfill('0') << std::setw(4) << vid << ", pid: 0x" << std::setw(4) << pid
                  << ", serial number: " << serialNumber << ", connection: " << connection << std::endl;
    }
    std::cout << std::endl;
}

void rebootDevices(std::shared_ptr<ob::DeviceList> deviceList) {
    for(uint32_t i = 0; i < deviceList->getCount(); i++) {
        // get device from device list
        auto device = deviceList->getDevice(i);

        // reboot device
        device->reboot();
    }
}

class Timer {
public:
    Timer() {
        reset();
    }

    /**
     * @brief Reset the timer to now
     */
    void reset() {
        start_ = last_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Returns milliseconds elapsed since last call and resets timer
     */
    uint64_t touchMs(bool sinceLastTouch = true) {
        auto now  = std::chrono::steady_clock::now();
        auto diff = std::chrono::milliseconds::zero();
        if(sinceLastTouch) {
            diff  = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_);
            last_ = now;
        }
        else {
            diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
        }
        return static_cast<uint64_t>(diff.count());
    }

    /**
     * @brief Returns microseconds elapsed since last call and resets timer
     */
    uint64_t touchUs(bool sinceLastTouch = true) {
        auto now  = std::chrono::steady_clock::now();
        auto diff = std::chrono::microseconds::zero();
        if(sinceLastTouch) {
            diff  = std::chrono::duration_cast<std::chrono::microseconds>(now - last_);
            last_ = now;
        }
        else {
            diff = std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
        }
        return static_cast<uint64_t>(diff.count());
    }

private:
    std::chrono::steady_clock::time_point last_;
    std::chrono::steady_clock::time_point start_;
};

int main(void) try {
    // create context
    Timer       timer;
    ob::Context ctx;
    std::cout << "create context: " << timer.touchMs() << std::endl;

    // register device callback
    timer.reset();
    auto id = ctx.registerDeviceChangedCallback([](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> deviceList) {
        printDeviceList("added", deviceList);
        printDeviceList("removed", removedList);
    });
    std::cout << "registerDeviceChangedCallback: " << timer.touchMs() << std::endl;

    timer.reset();
    ctx.setGvcpPortScheme(OB_GVCP_PORT_SCHEME_B);
    std::cout << "setGvcpPortScheme: " << timer.touchMs() << std::endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // query current device list
    timer.reset();
    auto currentList = ctx.queryDeviceList();
    std::cout << "queryDeviceList: " << timer.touchMs() << std::endl;
    printDeviceList("connected", currentList);

    timer.reset();
    ctx.setGvcpPortScheme(OB_GVCP_PORT_SCHEME_STANDARD);
    std::cout << "setGvcpPortScheme: " << timer.touchMs() << std::endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // query current device list
    timer.reset();
    currentList = ctx.queryDeviceList();
    std::cout << "queryDeviceList: " << timer.touchMs() << std::endl;
    printDeviceList("connected", currentList);

    std::cout << "Press 'r' to reboot the connected devices to trigger the device disconnect and reconnect event, or manually unplug and plugin the device."
              << std::endl;
    std::cout << "Press 'Esc' to exit." << std::endl << std::endl;

    auto scheme = ctx.getGvcpPortScheme();
    // main loop, wait for key press
    while(true) {
        auto key = ob_smpl::waitForKeyPressed(100);
        // Press the esc key to exit
        if(key == 27) {
            break;
        }
        else if(key == 'r' || key == 'R') {
            // update device list
            currentList = ctx.queryDeviceList();

            std::cout << "Rebooting devices..." << std::endl;
            rebootDevices(currentList);
        }
        else if(key == 's' || key == 'S') {
            currentList    = ctx.queryDeviceList();
            auto lastCount = currentList->getCount();
            scheme         = scheme == OB_GVCP_PORT_SCHEME_B ? OB_GVCP_PORT_SCHEME_STANDARD : OB_GVCP_PORT_SCHEME_B;
            timer.reset();
            ctx.setGvcpPortScheme(scheme);
            std::cout << "setGvcpPortScheme to " << scheme << " with time: " << timer.touchMs() << std::endl;
            // update device list
            timer.reset();
            currentList = ctx.queryDeviceList();
            while(lastCount == currentList->getCount()) {
                currentList = ctx.queryDeviceList();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cout << "continue..." << std::endl;
        }
    }

    ctx.unregisterDeviceChangedCallback(id);
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
