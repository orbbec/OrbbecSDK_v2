#pragma once

#include <iostream>
#include <mutex>
#include <unordered_set>
#include <vector>


namespace libobsensor {

struct RTPHeader {
    uint8_t  csrcCount : 4;
    uint8_t  extension : 1;
    uint8_t  padding : 1;
    uint8_t  version : 2;
    uint8_t  payloadType : 7;
    uint8_t  marker : 1;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
};

struct OBRTPPacket {
    uint8_t              marker;
    uint8_t              payloadType;
    uint16_t             sequenceNumber;
    std::vector<uint8_t> payload;

    OBRTPPacket(uint8_t marker, uint8_t payloadType, uint16_t seqNum, const std::vector<uint8_t> &data)
        : marker(marker), payloadType(payloadType), sequenceNumber(seqNum), payload(data) {}
};

class ObRTPPacketProcessor {

public:
    ObRTPPacketProcessor();

    ~ObRTPPacketProcessor() noexcept;

    bool process(RTPHeader *header, uint8_t *recvData, uint32_t length, uint32_t type);

    uint8_t *getFrameData() {
        return rtpBuffer_ + (RTP_FIX_METADATA_OFFSET + RTP_FIX_METADATA_SIZE);
    }

    uint32_t getFrameDataSize() {
        return dataSize_ - RTP_FIX_METADATA_SIZE;
    }

    uint8_t *getMetaData();

    uint32_t getMetaDataSize() {
        return (RTP_FIX_METADATA_OFFSET + RTP_FIX_METADATA_SIZE);
    }

    uint64_t getNumber() {
        return frameNumber_;
    }

    uint64_t getTimestamp() {
        return timestamp_;
    }

    bool processError();

    bool processComplete();

    void reset();

    void release();

private:
    void OnStartOfFrame();
    bool foundStartPacket();
    void OnEndOfFrame(uint16_t sequenceNumber);
    void convertBigEndianToLittleEndian(uint8_t *recvData, uint32_t size);

private:
    const uint32_t MAX_RTP_FRAME_SIZE      = 4 * 1920 * 1080;
    const uint32_t MAX_RTP_FIX_SIZE        = 1472;
    const uint32_t RTP_FIX_SIZE            = 12;
    const uint32_t RTP_FIX_METADATA_OFFSET = 12;
    const uint32_t RTP_FIX_METADATA_SIZE   = 96;

    bool foundStartPacket_;
    bool revDataComplete_;
    bool revDataError_;

    uint64_t frameNumber_;
    uint32_t maxPacketCount_;
    uint32_t maxCacheSize_;
    uint32_t maxPacketSize_;
    uint64_t timestamp_;

    //metadataSize(96) + framedataSize
    uint32_t dataSize_;
    // metadata(96) + framedata
    uint8_t *rtpBuffer_;

    uint32_t rtpType_;

    uint32_t fameSequenceNumberCount_;
};

}

