#include "ObRTPPacketProcessor.hpp"
#include "logger/Logger.hpp"
#include "ethernet/socket/SocketTypes.hpp"
#include <algorithm>
#include <limits>

#define START_RTP_TAG 0x0
#define END_RTP_TAG 0x1
#define OB_RTP_PACKET_REV_TIMEOUT 10

namespace libobsensor {


ObRTPPacketProcessor::ObRTPPacketProcessor()
    : foundStartPacket_(false), revDataComplete_(false), revDataError_(false), countDownStart_(false), frameNumber_(0), dataSize_(0) ,rtpBuffer_(nullptr) {

    maxPacketSize_  = MAX_RTP_FIX_SIZE - RTP_FIX_SIZE;
    maxPacketCount_ = MAX_RTP_FRAME_SIZE / maxPacketSize_ + 1;
    maxCacheSize_   = maxPacketCount_ * maxPacketSize_;
    rtpBuffer_      = new uint8_t[maxCacheSize_];
    memset(rtpBuffer_, 0, maxCacheSize_);
}

ObRTPPacketProcessor::~ObRTPPacketProcessor() noexcept {
    release();
}

void ObRTPPacketProcessor::OnStartOfFrame() {
    foundStartPacket_ = true;
    dataSize_         = 0;
    memset(rtpBuffer_, 0, maxCacheSize_);
    sequenceNumberList_.clear();
}

bool ObRTPPacketProcessor::founStartPacket() {
    return foundStartPacket_;
}

bool ObRTPPacketProcessor::process(RTPHeader *header, uint8_t *recvData, uint32_t length, uint32_t type) {
    if(sequenceNumberList_.size() >= maxPacketCount_) {
        LOG_WARN("RTP data buffer overflow!");
        return false;
    }

    uint8_t  marker         = header->marker;
    uint16_t sequenceNumber = ntohs(header->sequenceNumber);
    rtpType_                = type;
    //LOG_DEBUG("marker-{}, sequenceNumber-{},length-{}, type-{}", marker, sequenceNumber, length, type);
    if(sequenceNumber == START_RTP_TAG) {
        timestamp_ = header->timestamp;
        OnStartOfFrame();
    }

    if(!founStartPacket()) {
        return false;
    }

    if(sequenceNumberList_.find(sequenceNumber) != sequenceNumberList_.end()) {
        return true;
    }
    
    sequenceNumberList_.insert(sequenceNumber);

    uint32_t offset  = RTP_FIX_METADATA_OFFSET + sequenceNumber * maxPacketSize_;
    uint32_t dataLen = length - RTP_FIX_SIZE;
    if(rtpBuffer_ != nullptr && dataLen > 0) {
        memcpy(rtpBuffer_ + offset, recvData + RTP_FIX_SIZE, dataLen);
        dataSize_ += dataLen;
    }

    if(marker == END_RTP_TAG) {
        OnEndOfFrame(sequenceNumber);
    }

    return true;
}

void ObRTPPacketProcessor::OnEndOfFrame(uint16_t sequenceNumber) {
    // If the number of received data packets equals the end frame's SN (sequence number),
    // it indicates that all RTP packets for the frame have been successfully received. Otherwise,
    // it means that some RTP packets for the frame are still missing, and you will need to wait
    // for 10ms to receive additional data before proceeding.
    if(sequenceNumberList_.size() == (uint32_t)(sequenceNumber + 1)) {
        std::unique_lock<std::mutex> lk(revStatusMutex_);
        if(rtpType_ == OB_STREAM_DEPTH) {
            convertBigEndianToLittleEndian(rtpBuffer_, dataSize_);
        }
        revDataComplete_ = true;
        revDataError_    = false;
        ++frameNumber_;
    }
    else {
        revDataComplete_ = false;
        revDataError_    = true;
        LOG_WARN("Received rtp packet count does not match sequenceNumber!");
    }
}

void ObRTPPacketProcessor::convertBigEndianToLittleEndian(uint8_t *recvData, uint32_t size) {
    uint16_t *data      = reinterpret_cast<uint16_t *>(recvData + RTP_FIX_METADATA_OFFSET);
    uint32_t  numPixels = size / 2;
    for(uint32_t i = 0; i < numPixels; ++i) {
        data[i] = (data[i] >> 8) | (data[i] << 8);
    }
}

void ObRTPPacketProcessor::startCountDown() {
    if(countDownStart_) {
        return;
    }

    countDownStart_ = true;
    std::thread timerThread(&ObRTPPacketProcessor::countDown, this, OB_RTP_PACKET_REV_TIMEOUT);
}

void ObRTPPacketProcessor::countDown(int milliseconds) {
    LOG_DEBUG("Start the rtp packet countdown timer...");
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    { 
        std::unique_lock<std::mutex> lk(revStatusMutex_);
        if(!revDataComplete_) {
            revDataError_ = true;
        }
    }
    LOG_DEBUG("Exit the rtp packet countdown timer...");
}

bool ObRTPPacketProcessor::processError() {
    bool timeOutStatus = false;
    {
        std::unique_lock<std::mutex> lk(revStatusMutex_);
        timeOutStatus = revDataError_;
    }
    return timeOutStatus;
}

bool ObRTPPacketProcessor::processComplete() {
    bool revStatus = false;
    {
        std::unique_lock<std::mutex> lk(revStatusMutex_);
        revStatus = revDataComplete_;
    }

    return revStatus;
}

void ObRTPPacketProcessor::reset() {
    foundStartPacket_ = false;
    countDownStart_   = false;
    revDataComplete_  = false;
    revDataError_     = false;
}

void ObRTPPacketProcessor::release() {
    if(rtpBuffer_ != nullptr) {
        delete rtpBuffer_;
        rtpBuffer_ = nullptr;
    }
}

}