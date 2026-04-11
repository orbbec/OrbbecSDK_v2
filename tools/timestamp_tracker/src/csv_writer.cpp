// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "csv_writer.hpp"
#include <libobsensor/hpp/TypeHelper.hpp>
#include <iostream>
#include <algorithm>
#include <iosfwd>
#include <sstream>

namespace libobsensor {
namespace tools {

bool CsvWriter::open(const std::string &serialNumber, const std::string &sensorType) {
    close();
    serialNumber_ = serialNumber;
    sensorType_   = sensorType;
    // File creation is deferred to the first frame, when the actual profile is known.
    return true;
}

bool CsvWriter::openVolume() {
    if(file_.is_open()) {
        file_.flush();
        file_.close();
    }

    std::string filename = baseFilename_;
    if(volumeIndex_ > 1) {
        filename += "-" + std::to_string(volumeIndex_);
    }
    filename += ".csv";

    file_.open(filename);
    if(!file_.is_open()) {
        std::cerr << "Failed to open CSV file: " << filename << std::endl;
        return false;
    }

    // Simplified header: only timestamp-related columns
    file_ << "FrameIndex,FrameNumber,RecvTS(us),SysTS(us),GlobalTS(us),DevTS(us),Diff_SG(us),Diff_SD(us)\n";
    file_.flush();
    headerWritten_ = true;
    rowCount_      = 0;

    openedFiles_.push_back(filename);
    return true;
}

void CsvWriter::close() {
    if(file_.is_open()) {
        file_.flush();  // Ensure all data is written before closing
        file_.close();
    }
    fileReady_     = false;
    headerWritten_ = false;
    writeCount_    = 0;
    rowCount_      = 0;
    volumeIndex_   = 1;
}

bool CsvWriter::isOpen() const {
    return file_.is_open() || !serialNumber_.empty();
}

void CsvWriter::writeFrame(std::shared_ptr<ob::Frame> frame, uint64_t recvTimeUs) {
    if(!frame) {
        return;
    }

    // On the first frame, resolve the actual profile and open the file.
    if(!fileReady_) {
        auto     profile = frame->getStreamProfile()->as<ob::VideoStreamProfile>();
        uint32_t width   = profile ? profile->getWidth() : 0;
        uint32_t height  = profile ? profile->getHeight() : 0;
        uint32_t fps     = profile ? profile->getFps() : 0;
        std::string fmt  = profile ? ob::TypeHelper::convertOBFormatTypeToString(profile->getFormat()) : "";

        // Build base filename: Timestamps_{SerialNumber}_{SensorType}_{Width}x{Height}_{FPS}_{Format}
        baseFilename_ = "Timestamps_" + serialNumber_ + "_" + sensorType_ + "_"
                        + std::to_string(width) + "x" + std::to_string(height) + "_"
                        + std::to_string(fps) + "_" + fmt;
        volumeIndex_  = 1;
        rowCount_     = 0;
        writeCount_   = 0;
        if(!openVolume()) {
            return;
        }
        fileReady_ = true;
    }

    std::ostringstream oss;

    // frame index (from sdk)
    oss << frame->index();
    // frame number(from device)
    if(frame->hasMetadata(OB_FRAME_METADATA_TYPE_FRAME_NUMBER)) {
        oss << "," << frame->getMetadataValue(OB_FRAME_METADATA_TYPE_FRAME_NUMBER);
    }
    else {
        oss << ",n/a";
    }

    // app receive timestamp (system_clock, captured at pipeline callback entry)
    oss << "," << recvTimeUs;

    // timestamps: system timestamp, global timestamp, device timestamp
    auto sysTs    = frame->systemTimeStampUs();
    auto globalTs = frame->globalTimeStampUs();
    auto deviceTs = frame->timeStampUs();
    oss << "," << sysTs << "," << globalTs << "," << deviceTs;

    // diff_system_global
    if(globalTs > 0) {
        oss << "," << static_cast<int64_t>(sysTs - globalTs);
    }
    else {
        oss << ",n/a";
    }

    // diff_system_device
    if(deviceTs > 0) {
        oss << "," << static_cast<int64_t>(sysTs - deviceTs);
    }
    else {
        oss << ",n/a";
    }
    file_ << oss.str() << "\n";
    ++rowCount_;

    // Flush periodically to reduce I/O overhead while ensuring data persistence
    if(++writeCount_ % FLUSH_INTERVAL == 0) {
        file_.flush();
    }

    // Roll over to a new file when the Excel row limit is reached
    if(rowCount_ >= MAX_ROWS) {
        ++volumeIndex_;
        openVolume();
    }
}

}  // namespace tools
}  // namespace libobsensor
