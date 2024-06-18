#include "window.hpp"

#include <iostream>
#include "openobsdk/hpp/Pipeline.hpp"
#include "openobsdk/hpp/Error.hpp"

int main(int argc, char **argv) try {
    // Create a pipeline with default device.
    ob::Pipeline pipe;
    // Get the device.
    std::shared_ptr<ob::Device> device = pipe.getDevice();
    // Get preset list from device.
    std::shared_ptr<ob::DevicePresetList> presetLists = device->getAvailablePresetList();
    std::cout << "Available Presets:" << std::endl;
    for(uint32_t index = 0; index < presetLists->count(); index++) {
        // Print available preset name.
        std::cout << " - " <<index << "." << presetLists->getName(index) << std::endl;
    }

    // Print current preset name.
    std::cout << "Current PresetName: " << device->getCurrentPresetName() << std::endl;

    std::cout << "Enter index of preset to load: ";
    // Select preset to load.
    int loadIndex;
    std::cin >> loadIndex;
    auto presetName = presetLists->getName(loadIndex);
    // Load preset.
    device->loadPreset(presetName);

    std::cout << "Current PresetName: " << device->getCurrentPresetName() << std::endl;

    // Stop Pipeline.
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    exit(EXIT_FAILURE);
}