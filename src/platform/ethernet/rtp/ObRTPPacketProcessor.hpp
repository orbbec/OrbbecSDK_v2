#pragma once

#include <iostream>
#include <mutex>
#include <unordered_set>


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

    bool process(RTPHeader *header, uint8_t *recvData, uint32_t length);


    void startCountDown();

    uint8_t *getData() {
        return rtpBuffer_;
    }

    uint32_t getDataSize() {
        return dataSize_;
    }

    int32_t getNumber() {
        return frameNumber_;
    }

    uint64_t getTimestamp() {
        return timestamp_;
    }

    bool processTimeOut();

    bool processComplete();

    void reset();

    void release();

private:
    void OnStartOfFrame();
    bool founStartPacket();
    void OnEndOfFrame(uint16_t sequenceNumber);
    void countDown(int milliseconds);

private:
    const uint32_t MAX_RTP_FRAME_SIZE = 2 * 1280 * 1280;
    const uint32_t MAX_RTP_FIX_SIZE   = 1472;
    const uint32_t RTP_FIX_SIZE       = 12;

    bool foundStartPacket_;
    bool revDataComplete_;
    bool revDataOutTime_;
    bool countDownStart_;

    int32_t  frameNumber_;
    uint32_t maxPacketCount_;
    uint32_t maxCacheSize_;
    uint32_t maxPacketSize_;
    uint64_t timestamp_;

    uint32_t dataSize_;
    uint8_t *rtpBuffer_;

    std::mutex revStatusMutex_;

    std::unordered_set<uint16_t> sequenceNumberList_;
};

}

