// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AlignImpl.hpp"

#include <iostream>
#include <fstream>
#include <regex>

#include <map>
#include <sstream>
#include <string>

// Data structure to hold the sections, keys, and values
using IniSection = std::map<std::string, std::string>;
using IniData    = std::map<std::string, IniSection>;

// Function to trim whitespace from the beginning and end of a string
std::string trim(const std::string &str) {
    const char *whitespace = " \t\n\r";
    size_t      start      = str.find_first_not_of(whitespace);
    size_t      end        = str.find_last_not_of(whitespace);
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Function to parse an INI file
IniData parseIniFile(const std::string &filename) {
    IniData       iniData;
    std::ifstream file(filename);
    std::string   line;
    std::string   currentSection;

    if(!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return iniData;
    }

    while(std::getline(file, line)) {
        line = trim(line);

        // Skip empty lines or comments
        if(line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        // Handle sections [section]
        if(line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            currentSection = trim(currentSection);
        }
        // Handle key-value pairs key=value
        else {
            size_t equalsPos = line.find('=');
            if(equalsPos != std::string::npos) {
                std::string key              = trim(line.substr(0, equalsPos));
                std::string value            = trim(line.substr(equalsPos + 1));
                iniData[currentSection][key] = value;
            }
        }
    }

    file.close();
    return iniData;
}

// Function to display parsed INI data
void displayIniData(const IniData &iniData) {
    for(const auto &section: iniData) {
        std::cout << "[" << section.first << "]" << std::endl;
        for(const auto &pair: section.second) {
            std::cout << pair.first << "=" << pair.second << std::endl;
        }
    }
}

int parse_bolt_data(char *filename, OBCameraIntrinsic &depth_intr, OBCameraIntrinsic &color_intr, OBCameraDistortion &depth_disto, OBCameraDistortion &color_disto,
                    OBTransform &trans) {
    IniData inidata = parseIniFile(filename);
    color_disto.k1  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k1"].c_str()));
    color_disto.k2  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k2"].c_str()));
    color_disto.k3  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k3"].c_str()));
    color_disto.k4  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k4"].c_str()));
    color_disto.k5  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k5"].c_str()));
    color_disto.k6  = static_cast<float>(std::atof(inidata["ColorDistortion"]["k6"].c_str()));
    color_disto.p1  = static_cast<float>(std::atof(inidata["ColorDistortion"]["p1"].c_str()));
    color_disto.p2  = static_cast<float>(std::atof(inidata["ColorDistortion"]["p2"].c_str()));
    color_disto.model   = OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY_K6;
    color_intr.fx   = static_cast<float>(std::atof(inidata["ColorIntrinsic"]["fx"].c_str()));
    color_intr.fy   = static_cast<float>(std::atof(inidata["ColorIntrinsic"]["fy"].c_str()));
    color_intr.cx   = static_cast<float>(std::atof(inidata["ColorIntrinsic"]["cx"].c_str()));
    color_intr.cy   = static_cast<float>(std::atof(inidata["ColorIntrinsic"]["cy"].c_str()));
    color_intr.width   = static_cast<int16_t>(std::atof(inidata["ColorIntrinsic"]["width"].c_str()));
    color_intr.height   = static_cast<int16_t>(std::atof(inidata["ColorIntrinsic"]["height"].c_str()));
    depth_disto.k1  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k1"].c_str()));
    depth_disto.k2  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k2"].c_str()));
    depth_disto.k3  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k3"].c_str()));
    depth_disto.k4  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k4"].c_str()));
    depth_disto.k5  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k5"].c_str()));
    depth_disto.k6  = static_cast<float>(std::atof(inidata["DepthDistortion"]["k6"].c_str()));
    depth_disto.p1  = static_cast<float>(std::atof(inidata["DepthDistortion"]["p1"].c_str()));
    depth_disto.p2  = static_cast<float>(std::atof(inidata["DepthDistortion"]["p2"].c_str()));
    depth_disto.model   = OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY_K6;
    depth_intr.fx       = static_cast<float>(std::atof(inidata["DepthIntrinsic"]["fx"].c_str()));
    depth_intr.fy       = static_cast<float>(std::atof(inidata["DepthIntrinsic"]["fy"].c_str()));
    depth_intr.cx       = static_cast<float>(std::atof(inidata["DepthIntrinsic"]["cx"].c_str()));
    depth_intr.cy       = static_cast<float>(std::atof(inidata["DepthIntrinsic"]["cy"].c_str()));
    depth_intr.width    = static_cast<int16_t>(std::atof(inidata["DepthIntrinsic"]["width"].c_str()));
    depth_intr.height   = static_cast<int16_t>(std::atof(inidata["DepthIntrinsic"]["height"].c_str()));

    for(size_t i = 0; i < 9; i++) {
        std::string key = "rot" + std::to_string(i);
        float       tmp = static_cast<float>(std::atof(inidata["D2CTransformParam"][key].c_str()));
        trans.rot[i]    = tmp;
    }
    for(size_t i = 0; i < 3; i++) {
        std::string key = "trans" + std::to_string(i);
        float       tmp = static_cast<float>(std::atof(inidata["D2CTransformParam"][key].c_str()));
        trans.trans[i]    = tmp;
    }

    return 0;
}

#define RESOLUTION std::pair<int, int>
#define INTRINSIC_MAP std::unordered_map<RESOLUTION, OBCameraIntrinsic, libobsensor::ResHashFunc, libobsensor::ResComp>

int parse_obviewer_data(char *filename, OBCameraIntrinsic &depth_intr, OBCameraIntrinsic &color_intr, OBCameraDistortion &depth_disto,
                        OBCameraDistortion &color_disto, OBTransform &trans) {
    std::ifstream file(filename);
    if(!file.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    std::string line;

    const std::string numberPattern = R"([+-]?\d+\.?\d*[eE]?[+-]?\d*)";

    // Define regular expressions for each type of line
    std::regex extrinsicRotationRegex(
        R"(rotation:(-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+) (-?\d+\.\d+))");
    // Define regular expressions for each type of line using the number pattern
    std::regex intrinsicRegex(R"(fx:()" + numberPattern + R"() fy:()" + numberPattern + R"() cx:()" + numberPattern + R"() cy:()" + numberPattern
                              + R"() width:(\d+) height:(\d+))");

    std::regex distortionRegex(R"(k1:()" + numberPattern + R"() k2:()" + numberPattern + R"() k3:()" + numberPattern + R"() k4:()" + numberPattern + R"() k5:()"
                               + numberPattern + R"() k6:()" + numberPattern + R"() p1:()" + numberPattern + R"() p2:()" + numberPattern + R"())");

    std::regex extrinsicTranslationRegex(R"(translation:()" + numberPattern + R"( )()" + numberPattern + R"( )()" + numberPattern + R"())");

    while(std::getline(file, line)) {
        std::smatch match;

        // Match color intrinsic parameters
        if(std::regex_search(line, match, intrinsicRegex) && line.find("Color Intrinsic") != std::string::npos) {
            color_intr = { std::stof(match[1]), std::stof(match[2]),          std::stof(match[3]),
                           std::stof(match[4]), int16_t(std::stoi(match[5])), int16_t(std::stoi(match[6])) };
        }

        // Match depth intrinsic parameters
        else if(std::regex_search(line, match, intrinsicRegex) && line.find("Depth Intrinsic") != std::string::npos) {
            depth_intr = { std::stof(match[1]), std::stof(match[2]),          std::stof(match[3]),
                           std::stof(match[4]), int16_t(std::stoi(match[5])), int16_t(std::stoi(match[6])) };
        }

        // Match color distortion parameters
        else if(std::regex_search(line, match, distortionRegex) && line.find("Color Distortion") != std::string::npos) {
            color_disto = { std::stof(match[1]), std::stof(match[2]), std::stof(match[3]),
                            std::stof(match[4]), std::stof(match[5]), std::stof(match[6]),
                            std::stof(match[7]), std::stof(match[8]), OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY };
        }

        // Match depth distortion parameters
        else if(std::regex_search(line, match, distortionRegex) && line.find("Depth Distortion") != std::string::npos) {
            depth_disto = { std::stof(match[1]), std::stof(match[2]), std::stof(match[3]),
                            std::stof(match[4]), std::stof(match[5]), std::stof(match[6]),
                            std::stof(match[7]), std::stof(match[8]), OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY };
        }

        // Match depth to color rotation parameters
        else if(std::regex_search(line, match, extrinsicRotationRegex)) {
            for(int i = 0; i < 9; ++i) {
                trans.rot[i] = std::stof(match[1 + i]);
            }
        }

        // Match depth to color translation parameters
        else if(std::regex_search(line, match, extrinsicTranslationRegex)) {
            for(int i = 0; i < 3; ++i) {
                trans.trans[i] = std::stof(match[1 + i]);
            }
        }
    }

    file.close();
    return 0;
}

int parse_oblog_data(char *filename, INTRINSIC_MAP &depth_intr_map, INTRINSIC_MAP &color_intr_map, OBCameraDistortion &depth_disto,
    OBCameraDistortion &color_disto, OBTransform &trans) {

    std::ifstream file(filename);
    if(!file.is_open()) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    std::string line;

    const std::string numberPattern = R"([+-]?\d+\.?\d*[eE]?[+-]?\d*)";

    // Define regular expressions for each type of line
    std::regex intrinsicRegex(R"(cx: ()" + numberPattern + R"(), cy: ()" + numberPattern + R"(), fx: ()" + numberPattern + R"(), fy: ()" + numberPattern
                              + R"(), width: (\d+), height: (\d+))");

    std::regex distortionRegex(R"(k1: ()" + numberPattern + R"(), k2: ()" + numberPattern + R"(), k3: ()" + numberPattern + R"(), k4: ()" + numberPattern + R"(), k5: ()"
                               + numberPattern + R"(), k6: ()" + numberPattern + R"(), p1: ()" + numberPattern + R"(), p2: ()" + numberPattern + R"())");

    std::regex extrinsicRegex(R"(rot: \[()" + numberPattern + R"(), ()" + numberPattern + R"(), ()" + numberPattern + R"(), ()"
        + numberPattern + R"(), ()" + numberPattern + R"(), ()" + numberPattern + R"(), ()"
        + numberPattern + R"(), ()" + numberPattern + R"(), ()" + numberPattern + R"()\], trans: \[()"
        + numberPattern + R"(), ()" + numberPattern + R"(), ()" + numberPattern + R"()\])");

    while(std::getline(file, line)) {
        printf("%s\n", line.c_str());
        std::smatch match;

        // Match color intrinsic parameters
        if(std::regex_search(line, match, intrinsicRegex) && line.find("rgbIntrinsic") != std::string::npos) {
            OBCameraIntrinsic color_intr = { std::stof(match[1]), std::stof(match[2]),          std::stof(match[3]),
                           std::stof(match[4]), int16_t(std::stoi(match[5])), int16_t(std::stoi(match[6])) };
            color_intr_map[RESOLUTION(color_intr.width, color_intr.height)] = color_intr;
        }

        // Match depth intrinsic parameters
        else if(std::regex_search(line, match, intrinsicRegex) && line.find("depthIntrinsic") != std::string::npos) {
            OBCameraIntrinsic depth_intr = { std::stof(match[1]), std::stof(match[2]),          std::stof(match[3]),
                           std::stof(match[4]), int16_t(std::stoi(match[5])), int16_t(std::stoi(match[6])) };
            depth_intr_map[RESOLUTION(depth_intr.width, depth_intr.height)] = depth_intr;
        }

        // Match color distortion parameters
        else if(std::regex_search(line, match, distortionRegex) && line.find("rgbDistortion") != std::string::npos) {
            color_disto = { std::stof(match[1]), std::stof(match[2]), std::stof(match[3]),
                            std::stof(match[4]), std::stof(match[5]), std::stof(match[6]),
                            std::stof(match[7]), std::stof(match[8]), OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY };
        }

        // Match depth distortion parameters
        else if(std::regex_search(line, match, distortionRegex) && line.find("depthDistortion") != std::string::npos) {
            depth_disto = { std::stof(match[1]), std::stof(match[2]), std::stof(match[3]),
                            std::stof(match[4]), std::stof(match[5]), std::stof(match[6]),
                            std::stof(match[7]), std::stof(match[8]), OBCameraDistortionModel::OB_DISTORTION_BROWN_CONRADY };
        }

        // Match depth to color rotation parameters
        else if(std::regex_search(line, match, extrinsicRegex)) {
            for(int i = 0; i < 9; ++i) {
                trans.rot[i] = std::stof(match[1 + i]);
            }
            for(int i = 0; i < 3; ++i) {
                trans.trans[i] = std::stof(match[1 + 9 + i]);
            }
        }
    }

    file.close();
    return 0;

}

int main__(int argc, char *argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: %s <param_file> <depth_image_file> <color_image_file>" << std::endl;
        return -1;
    }

    INTRINSIC_MAP  depth_intr, color_intr;
    OBCameraDistortion depth_disto, color_disto;
    OBTransform        transform;
    if(0 != parse_oblog_data(argv[1], depth_intr, color_intr, depth_disto, color_disto, transform)) {
        std::cerr << "Parse parameter file error." << std::endl;
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: %s <param_file> <depth_image_file> <color_image_file> [swap_endianness=0]" << std::endl;
        return -1;
    }

    OBCameraIntrinsic  depth_intr, color_intr;
    OBCameraDistortion depth_disto, color_disto;
    OBTransform        transform;

    bool bIni = false;
    { 
        std::string tmp = std::string(argv[1]);
        if (tmp.length() >= 4) {
            bIni = tmp.substr(tmp.length() - 4) == ".ini";
        }
    }

    if((bIni && parse_bolt_data(argv[1], depth_intr, color_intr, depth_disto, color_disto, transform))
        || (0 != parse_obviewer_data(argv[1], depth_intr, color_intr, depth_disto, color_disto, transform))) {
        std::cerr << "Parse parameter file error." << std::endl;
        return -1;
    }
    libobsensor::AlignImpl *impl = new libobsensor::AlignImpl();
    impl->initialize(depth_intr, depth_disto, color_intr, color_disto, transform, 1, true, true);

    ob_error *err = nullptr;

    FILE *depth_file = fopen(argv[2], "rb");
    if(!depth_file) {
        *err = { ob_status::OB_STATUS_ERROR, "read file error", "fopen", *argv[2], ob_exception_type::OB_EXCEPTION_TYPE_IO };
        std::cerr << err->message << std::endl;
        return -1;
    }

    uint32_t  depth_size = depth_intr.width * depth_intr.height * sizeof(uint16_t);
    uint16_t *depth_data = (uint16_t *)malloc(depth_size);
    fread(depth_data, depth_size, 1, depth_file);
    bool swap_endianness = false;
    if(argc > 4)
        swap_endianness = bool(std::atoi(argv[4]));
    if(swap_endianness) {
		for(int i = 0; i < depth_intr.width * depth_intr.height; i++) {
			depth_data[i] = _byteswap_ushort(depth_data[i]);
		}
    }

    uint32_t  aligned_depth_size = color_intr.width * color_intr.height * sizeof(uint16_t);
    uint16_t *aligned_depth_data = (uint16_t *)malloc(aligned_depth_size);
    //if(0 == impl->D2C(depth_data, depth_intr.width, depth_intr.height, aligned_depth_data, color_intr.width, color_intr.height, nullptr, true)) {
    if(0 == impl->D2C(depth_data, depth_intr.width, depth_intr.height, aligned_depth_data, color_intr.width, color_intr.height, nullptr, false)) {
        char nname[256];
        sprintf(nname, "%s_filtered_%dx%d.raw", argv[2], color_intr.width, color_intr.height);
        FILE *fp = fopen(nname, "wb");
        if(fp) {
            fwrite(aligned_depth_data, 1, aligned_depth_size, fp);
            fclose(fp);
        }
    }
    free(aligned_depth_data);

    FILE *color_file = fopen(argv[3], "rb");
    if(!color_file) {
        *err = { ob_status::OB_STATUS_ERROR, "read file error", "fopen", *argv[3], ob_exception_type::OB_EXCEPTION_TYPE_IO };
        std::cerr << err->message << std::endl;
        return -1;
    }
    uint32_t color_size = color_intr.width * color_intr.height * sizeof(uint8_t) * 3;
    uint8_t *color_data = (uint8_t *)malloc(color_size);
    fread(color_data, color_size, 1, color_file);
    uint32_t aligned_color_size = depth_intr.width * depth_intr.height * sizeof(uint8_t) * 3;
    uint8_t *aligned_color_data = (uint8_t *)malloc(aligned_color_size);
    if(0 == impl->C2D(depth_data, depth_intr.width, depth_intr.height, color_data, aligned_color_data, color_intr.width, color_intr.height, OB_FORMAT_RGB, false)) {
        char nname[256];
        sprintf(nname, "%s_filtered.raw", argv[3]);
        FILE *fp = fopen(nname, "wb");
        if(fp) {
            fwrite(aligned_color_data, 1, aligned_color_size, fp);
            fclose(fp);
        }
    }

    free(depth_data);
    free(color_data);
    free(aligned_color_data);

    delete impl;
    impl = nullptr;
    return 0;
}

