// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>
#include <libobsensor/hpp/Utils.hpp>

#include "utils.hpp"
#include <iomanip>
#include <vector>
#include <cmath>

// Select streams
void selectStreams(std::shared_ptr<ob::Device> device, std::shared_ptr<ob::Config> config);
// Select sensors.
std::vector<std::shared_ptr<ob::Sensor>> selectSensors(std::shared_ptr<ob::Device> device);
// Print IMU value.
void printImuValue(OBFloat3D obFloat3d, uint64_t index, uint64_t timeStampUs, float temperature, OBFrameType type, const std::string &unitStr);
// Print LiDAR point cloud frame information.
void printLiDARPointCloudInfo(std::shared_ptr<ob::LiDARPointsFrame> frame);

int main(void) try {

    // Create a pipeline.
    ob::Pipeline pipe;
    // Configure which streams to enable or disable for the Pipeline by creating a Config.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->disableAllStream();

    // Get the device from the pipeline
    auto device = pipe.getDevice();

    auto deviceInfo = device->getDeviceInfo();
    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "Current Device: "
              << " name: " << deviceInfo->getName() << ", vid: 0x" << std::hex << deviceInfo->getVid() << ", pid: 0x" << std::setw(4) << std::setfill('0')
              << deviceInfo->getPid() << ", uid: 0x" << deviceInfo->getUid() << std::dec << std::endl;
    // Get serial number
    uint32_t dataSize = 64;
    uint8_t  data[64] = { 0 };
    device->getStructuredData(OB_RAW_DATA_LIDAR_SERIAL_NUMBER, data, &dataSize);
    std::cout << "LiDAR SN: " << std::string((const char *)data, dataSize) << std::endl << std::endl;

    device->setIntProperty(OB_PROP_LIDAR_TAIL_FILTER_LEVEL_INT, 0);

    // Select and enable desired streams
    selectStreams(device, config);

    // Only FrameSet that contains all types of data frames will be output.
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_ALL_TYPE_FRAME_REQUIRE);

    uint32_t frameCount = 0;
    // Start the pipeline with config
    pipe.start(config, [&](std::shared_ptr<ob::FrameSet> frameSet) {
        // Callback function to process the frameSet
        if(!frameSet) {
            return;
        }

        // Process LiDAR point cloud frame and IMU frames
        for(uint32_t i = 0; i < frameSet->getCount(); i++) {
            auto frame = frameSet->getFrame(i);
            if(!frame) {
                continue;
            }

            // Print frame information every 50 frames.
            if(frameCount % 50 == 0) {
                if(frame->getType() == OB_FRAME_LIDAR_POINTS) {
                    auto lidarFrame = frame->as<ob::LiDARPointsFrame>();
                    printLiDARPointCloudInfo(lidarFrame);
                }
                else if(frame->getType() == OB_FRAME_ACCEL) {
                    auto accelFrame = frame->as<ob::AccelFrame>();
                    printImuValue(accelFrame->getValue(), accelFrame->getIndex(), accelFrame->getTimeStampUs(), accelFrame->getTemperature(),
                                  accelFrame->getType(), "m/s^2");
                }
                else if(frame->getType() == OB_FRAME_GYRO) {
                    auto gyroFrame = frame->as<ob::GyroFrame>();
                    printImuValue(gyroFrame->getValue(), gyroFrame->getIndex(), gyroFrame->getTimeStampUs(), gyroFrame->getTemperature(), gyroFrame->getType(),
                                  "rad/s");
                }
            }
        }

        frameCount++;
    });

    while(true) {
        // Wait for user input
        auto key = ob_smpl::waitForKeyPressed();
        if(key == ESC_KEY) {
            break;
        }
    }

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

// Print LiDAR profile information.
void printLiDARProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    auto gyroProfile = profile->as<ob::LiDARStreamProfile>();

    // Get the format.
    auto formatName = profile->getFormat();
    // Get scan rate.
    auto scanRate = gyroProfile->getScanRate();
    std::cout << " - " << index << "."
              << "format: " << ob::TypeHelper::convertOBFormatTypeToString(formatName) << ", "
              << "scan rate: " << ob::TypeHelper::convertOBLiDARScanRateTypeToString(scanRate) << std::endl;
}

// Print accel profile information.
void printAccelProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    // Get the profile of accel.
    auto accProfile = profile->as<ob::AccelStreamProfile>();
    // Get the rate of accel.
    auto accRate = accProfile->getSampleRate();
    std::cout << " - " << index << "."
              << "acc rate: " << ob::TypeHelper::convertOBIMUSampleRateTypeToString(accRate) << std::endl;
}

// Print gyro profile information.
void printGyroProfile(std::shared_ptr<ob::StreamProfile> profile, uint32_t index) {
    // Get the profile of gyro.
    auto gyroProfile = profile->as<ob::GyroStreamProfile>();
    // Get the rate of gyro.
    auto gyroRate = gyroProfile->getSampleRate();
    std::cout << " - " << index << "."
              << "gyro rate: " << ob::TypeHelper::convertOBIMUSampleRateTypeToString(gyroRate) << std::endl;
}

// Select streams
void selectStreams(std::shared_ptr<ob::Device> device, std::shared_ptr<ob::Config> config) {
    // Select sensors.
    auto selectedSensors = selectSensors(device);
    if(selectedSensors.size() == 0) {
        std::cout << "No sensor selected" << std::endl;
        return;
    }
    // Enable the stream of the selected sensor.
    for(auto &sensor: selectedSensors) {
        // Get the list of stream profiles.
        auto streamProfileList = sensor->getStreamProfileList();
        if(streamProfileList->getCount() == 0) {
            std::cout << "No stream profile found for sensor: " << ob::TypeHelper::convertOBSensorTypeToString(sensor->getType()) << std::endl;
            continue;
        }

        std::cout << "Stream profile list for sensor: " << ob::TypeHelper::convertOBSensorTypeToString(sensor->getType()) << std::endl;
        for(uint32_t index = 0; index < streamProfileList->getCount(); index++) {
            // Get the stream profile.
            auto profile = streamProfileList->getProfile(index);
            if(sensor->getType() == OB_SENSOR_ACCEL) {
                printAccelProfile(profile, index);
            }
            else if(sensor->getType() == OB_SENSOR_GYRO) {
                printGyroProfile(profile, index);
            }
            else if(sensor->getType() == OB_SENSOR_LIDAR) {
                printLiDARProfile(profile, index);
            }
            else {
                continue;
            }
        }

        std::cout << "Select a stream profile to enable (input stream profile index): " << std::endl;

        while(true) {
            // Select a stream profile.
            int streamProfileSelected = ob_smpl::getInputOption();
            if(streamProfileSelected >= static_cast<int>(streamProfileList->getCount()) || streamProfileSelected < -1) {
                std::cout << "\nInvalid input, please reselect the stream profile!\n";
                continue;
            }
            if(streamProfileSelected == -1) {
                break;
            }

            // Get the selected stream profile.
            auto selectedStreamProfile = streamProfileList->getProfile(streamProfileSelected);
            // Enable the selected stream profile.
            config->enableStream(selectedStreamProfile);
            break;
        }
    }
}

// Select sensors.
std::vector<std::shared_ptr<ob::Sensor>> selectSensors(std::shared_ptr<ob::Device> device) {
    std::vector<std::shared_ptr<ob::Sensor>> selectedSensors;

    while(true) {
        std::cout << "Sensor list: " << std::endl;
        // Get the list of sensors.
        auto sensorList = device->getSensorList();
        for(uint32_t index = 0; index < sensorList->getCount(); index++) {
            // Get the sensor type.
            auto sensorType = sensorList->getSensorType(index);
            std::cout << " - " << index << "."
                      << "sensor type: " << ob::TypeHelper::convertOBSensorTypeToString(sensorType) << std::endl;
        }

        // Add an option to select all sensors.
        std::cout << " - " << sensorList->getCount() << "."
                  << "all sensors" << std::endl;

        std::cout << "Select a sensor to enable(input sensor index, \'" << sensorList->getCount() << "\' to select all sensors): " << std::endl;

        // Select sensors.
        int sensorSelected = ob_smpl::getInputOption();
        if(sensorSelected > static_cast<int>(sensorList->getCount()) || sensorSelected < 0) {
            if(sensorSelected == -1) {
                break;
            }
            else {
                std::cout << "\nInvalid input, please reselect the sensor!\n";
                continue;
            }
        }

        // Get sensor from sensorList.
        if(sensorSelected == static_cast<int>(sensorList->getCount())) {
            for(uint32_t index = 0; index < sensorList->getCount(); index++) {
                auto sensor = sensorList->getSensor(index);
                selectedSensors.push_back(sensor);
            }
        }
        else {
            auto sensor = sensorList->getSensor(sensorSelected);
            selectedSensors.push_back(sensor);
        }
        break;
    }

    return selectedSensors;
}

// Print IMU value.
void printImuValue(OBFloat3D obFloat3d, uint64_t index, uint64_t timeStampUs, float temperature, OBFrameType type, const std::string &unitStr) {
    std::cout << "frame index: " << index << std::endl;
    auto typeStr = ob::TypeHelper::convertOBFrameTypeToString(type);
    std::cout << typeStr << " Frame: \n\r{\n\r"
              << "  tsp = " << timeStampUs << "\n\r"
              << "  temperature = " << temperature << "\n\r"
              << "  " << typeStr << ".x = " << obFloat3d.x << unitStr << "\n\r"
              << "  " << typeStr << ".y = " << obFloat3d.y << unitStr << "\n\r"
              << "  " << typeStr << ".z = " << obFloat3d.z << unitStr << "\n\r"
              << "}\n\r" << std::endl;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif
// Print LiDAR point cloud frame information.
void printLiDARPointCloudInfo(std::shared_ptr<ob::LiDARPointsFrame> pointCloudFrame) {
    auto pointCloudType = pointCloudFrame->getFormat();
    if(pointCloudType != OB_FORMAT_LIDAR_SPHERE_POINT && pointCloudType != OB_FORMAT_LIDAR_POINT && pointCloudType != OB_FORMAT_LIDAR_SCAN) {
        std::cout << "LiDAR point cloud format invalid" << std::endl;
        return;
    }

    constexpr auto minPointValue   = 1e-6f;
    uint32_t       validPointCount = 0;

    switch(pointCloudType) {
    case OB_FORMAT_LIDAR_SPHERE_POINT: {
        auto     points     = reinterpret_cast<const OBLiDARSpherePoint *>(pointCloudFrame->getData());
        uint32_t pointCount = static_cast<uint32_t>(pointCloudFrame->getDataSize() / sizeof(OBLiDARSpherePoint));

        for(uint32_t i = 0; i < pointCount; ++i) {
            double thetaRad = points->theta * M_PI / 180.0f;  // to unit rad
            double phiRad   = points->phi * M_PI / 180.0f;    // to unit rad
            auto   distance = points->distance;               // mm

            if(distance < minPointValue) {
                ++points;
                continue;  // skip invalid measurements
            }
            auto x = static_cast<float>(distance * cos(thetaRad) * cos(phiRad));
            auto y = static_cast<float>(distance * sin(thetaRad) * cos(phiRad));
            auto z = static_cast<float>(distance * sin(phiRad));
            if(std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
                validPointCount++;
            }

            ++points;
        }
        break;
    }
    case OB_FORMAT_LIDAR_POINT: {
        auto     points     = reinterpret_cast<const OBLiDARPoint *>(pointCloudFrame->getData());
        uint32_t pointCount = static_cast<uint32_t>(pointCloudFrame->getDataSize() / sizeof(OBLiDARPoint));

        for(uint32_t i = 0; i < pointCount; ++i) {
            auto x = points->x;
            auto y = points->y;
            auto z = points->z;

            if(std::fabs(z) >= minPointValue && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
                validPointCount++;
            }
            ++points;
        }
        break;
    }
    case OB_FORMAT_LIDAR_SCAN: {
        auto     points     = reinterpret_cast<const OBLiDARScanPoint *>(pointCloudFrame->getData());
        uint32_t pointCount = static_cast<uint32_t>(pointCloudFrame->getDataSize() / sizeof(OBLiDARScanPoint));

        for(uint32_t i = 0; i < pointCount; ++i) {
            double angleRad = points->angle * M_PI / 180.0f;  // to unit rad
            auto   distance = points->distance;

            if(distance < minPointValue) {
                ++points;
                continue;  // skip invalid measurements
            }
            auto x = static_cast<float>(distance * cos(angleRad));
            auto y = static_cast<float>(distance * sin(angleRad));
            //auto z = 0.0f;
            if(std::isfinite(x) && std::isfinite(y)) {
                validPointCount++;
            }

            ++points;
        }
        break;
    }
    default:
        std::cout << "Invalid LiDAR point cloud format" << std::endl;
        return;
    }

    if(validPointCount == 0) {
        std::cout << "LiDAR point cloud vertices is zero" << std::endl;
        return;
    }

    // Print point cloud information
    std::cout << "frame index: " << pointCloudFrame->getIndex() << std::endl;
    std::cout << "LiDAR PointCloud Frame: \n\r{\n\r"
              << "  tsp = " << pointCloudFrame->getTimeStampUs() << "\n\r"
              << "  format = " << ob::TypeHelper::convertOBFormatTypeToString(pointCloudType) << "\n\r"
              << "  valid point count = " << validPointCount << "\n\r"
              << "}\n\r" << std::endl;
}
