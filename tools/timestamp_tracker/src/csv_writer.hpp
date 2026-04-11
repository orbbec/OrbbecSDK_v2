// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <fstream>
#include <memory>
#include <string>
#include <libobsensor/ObSensor.hpp>

namespace libobsensor {
namespace tools {

class CsvWriter {
public:
    CsvWriter() = default;
    ~CsvWriter() {
        close();
    }

    CsvWriter(const CsvWriter &)            = delete;
    CsvWriter &operator=(const CsvWriter &) = delete;

    bool open(const std::string &serialNumber, const std::string &sensorType);
    void close();
    bool isOpen() const;

    void writeFrame(std::shared_ptr<ob::Frame> frame, uint64_t recvTimeUs);

    const std::vector<std::string> &getOpenedFiles() const {
        return openedFiles_;
    }

private:
    bool openVolume();

    std::ofstream         file_;
    std::string           serialNumber_;
    std::string           sensorType_;
    std::string           baseFilename_;           // filename without ".csv" extension; set on first frame
    bool                  fileReady_     = false;  // true once the file has been opened on first frame
    bool                  headerWritten_ = false;
    uint32_t              writeCount_    = 0;
    uint32_t              rowCount_      = 0;
    uint32_t              volumeIndex_   = 1;
    static const uint32_t FLUSH_INTERVAL = 100;      // Flush every N frames
    static const uint32_t MAX_ROWS       = 1024000;  // Excel row limit 1024576

    std::vector<std::string> openedFiles_;  // filenames opened so far (including rollovers)
};

}  // namespace tools
}  // namespace libobsensor
