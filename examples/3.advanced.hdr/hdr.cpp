#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

int main(void) try {
    // Create a pipeline with default device
    ob::Pipeline pipe;

    // Get the device from the pipeline
    auto device = pipe.getDevice();

    // Check if the device supports HDR merge
    if(!device->isPropertySupported(OB_STRUCT_DEPTH_HDR_CONFIG, OB_PERMISSION_READ_WRITE)) {
        std::cerr << "Current default device does not support HDR merge" << std::endl;
        std::cout << "Press any key to exit...";
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    // Configure which streams to enable or disable for the Pipeline by creating a Config
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    // Get all stream profiles of the depth camera, including stream resolution, frame rate, and frame format
    auto depthProfiles = pipe.getStreamProfileList(OB_SENSOR_DEPTH);
    auto depthProfile  = depthProfiles->getProfile(OB_PROFILE_DEFAULT);
    config->enableStream(depthProfile);

    // Create HDRMerge post processor to merge depth frames betweens different hdr sequence ids.
    // The HDRMerge also supports processing of infrared frames.
    auto hdrMerge = ob::FilterFactory::createFilter("HDRMerge");

    // configure and enable Hdr stream
    OBHdrConfig obHdrConfig;
    obHdrConfig.enable     = true;  // enable HDR merge
    obHdrConfig.exposure_1 = 7500;
    obHdrConfig.gain_1     = 24;
    obHdrConfig.exposure_2 = 100;
    obHdrConfig.gain_2     = 16;
    device->setStructuredData(OB_STRUCT_DEPTH_HDR_CONFIG, reinterpret_cast<uint8_t *>(&obHdrConfig), sizeof(OBHdrConfig));

    // Start the pipeline with config
    pipe.start(config);

    bool mergeRequired       = true;
    bool alternateShowOrigin = true;

    // Create a window for rendering and set the resolution of the window
    ob_smpl::CVWindow win("HDR-Merge", 1280, 720, ob_smpl::ARRANGE_GRID);
    win.setKeyPrompt("'M': Toggle HDR merge, 'N': Toggle alternate show origin frame");
    win.setKeyPressedCallback([&](int key) {
        if(key == 'M' || key == 'm') {
            mergeRequired = !mergeRequired;
            if(mergeRequired) {
                win.reset();
                win.addLog("HDR merge enabled.");
            }
            else {
                win.reset();
                win.addLog("HDR merge disabled.");
            }
        }
        if(key == 'N' || key == 'n') {
            alternateShowOrigin = !alternateShowOrigin;
            win.reset();
            win.addLog(std::string("Alternate show origin frame: ") + (alternateShowOrigin ? "on" : "off"));
        }
    });

    while(win.run()) {
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        auto depthFrame = frameSet->getFrame(OB_FRAME_DEPTH)->as<ob::DepthFrame>();
        if(depthFrame == nullptr) {
            continue;
        }

        // add original depth frame to render queue
        int groupId = alternateShowOrigin ? 0 : static_cast<int>(depthFrame->getMetadataValue(OB_FRAME_METADATA_TYPE_HDR_SEQUENCE_INDEX));
        win.pushFramesToView(depthFrame, groupId);

        if(mergeRequired) {
            try {
                // Using HDRMerge post processor to merge depth frames
                auto mergedDepthFrame = hdrMerge->process(depthFrame);
                if(mergedDepthFrame == nullptr) {
                    continue;
                }
                // add merged depth frame to render queue
                win.pushFramesToView(mergedDepthFrame, 10);  // set the group id to 10 to avoid same group id with original depth frame
            }
            catch(ob::Error &e) {
                std::cerr << "HDRMerge error: " << e.what() << std::endl;
            }
        }
    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    // close hdr merge
    obHdrConfig.enable = false;
    device->setStructuredData(OB_STRUCT_DEPTH_HDR_CONFIG, reinterpret_cast<uint8_t *>(&obHdrConfig), sizeof(OBHdrConfig));

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit...";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}
