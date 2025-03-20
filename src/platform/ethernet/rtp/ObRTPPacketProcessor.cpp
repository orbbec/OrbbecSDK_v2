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
    : foundStartPacket_(false), revDataComplete_(false), revDataError_(false), frameNumber_(0), dataSize_(0), rtpBuffer_(nullptr), fameSequenceNumberCount_(0) {

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
    foundStartPacket_        = true;
    dataSize_                = 0;
    fameSequenceNumberCount_ = 0;
    memset(rtpBuffer_, 0, maxCacheSize_);
}

bool ObRTPPacketProcessor::foundStartPacket() {
    return foundStartPacket_;
}

bool ObRTPPacketProcessor::process(RTPHeader *header, uint8_t *recvData, uint32_t length, uint32_t type) {
    if(fameSequenceNumberCount_ >= maxPacketCount_) {
        LOG_WARN("RTP data buffer overflow!");
        reset();
        return false;
    }

    uint8_t  marker         = header->marker;
    uint16_t sequenceNumber = ntohs(header->sequenceNumber);
    rtpType_                = type;
    // LOG_DEBUG("marker-{}, sequenceNumber-{},length-{}, type-{}", marker, sequenceNumber, length, type);
    if(sequenceNumber == START_RTP_TAG) {
        timestamp_ = header->timestamp;
        OnStartOfFrame();
    }

    if(!foundStartPacket()) {
        return false;
    }

    fameSequenceNumberCount_++;

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
    if(fameSequenceNumberCount_ == (uint32_t)(sequenceNumber + 1)) {
        if(rtpType_ == OB_STREAM_DEPTH) {
            convertBigEndianToLittleEndian(getFrameData(), getFrameDataSize());
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
    uint16_t *data      = reinterpret_cast<uint16_t *>(recvData);
    uint32_t  numPixels = size / 2;
    for(uint32_t i = 0; i < numPixels; ++i) {
        data[i] = (data[i] >> 8) | (data[i] << 8);
    }
}

uint8_t *ObRTPPacketProcessor::getMetaData() {
    uint32_t *data      = reinterpret_cast<uint32_t *>(rtpBuffer_ + RTP_FIX_METADATA_OFFSET);
    uint32_t  numPixels = RTP_FIX_METADATA_SIZE / 4;
    for(uint32_t i = 0; i < numPixels; ++i) {
        data[i] = ((data[i] >> 24) & 0x000000FF) | ((data[i] >> 8) & 0x0000FF00) | ((data[i] << 8) & 0x00FF0000) | ((data[i] << 24) & 0xFF000000);
    }
    return rtpBuffer_;
}

bool ObRTPPacketProcessor::processError() {
    return revDataError_;
}

bool ObRTPPacketProcessor::processComplete() {
    return revDataComplete_;
}

void ObRTPPacketProcessor::reset() {
    foundStartPacket_        = false;
    revDataComplete_         = false;
    revDataError_            = false;
    dataSize_                = 0;
    fameSequenceNumberCount_ = 0;
    memset(rtpBuffer_, 0, maxCacheSize_);
}

void ObRTPPacketProcessor::release() {
    if(rtpBuffer_ != nullptr) {
        delete rtpBuffer_;
        rtpBuffer_ = nullptr;
    }
}

}