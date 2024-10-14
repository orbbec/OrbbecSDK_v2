// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IFrame.hpp"
#include "IStreamProfile.hpp"
#include "exception/ObException.hpp"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include <queue>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace libobsensor {

class ObRTPBuffer {
public:
    ObRTPBuffer(uint32_t maxDataSize)
        : bufferSize_(maxDataSize), sequenceNumber_(0), timestamp_(0), staticHeaderSize_(0), recvdDataSize_(0), dynamicHeaderSize_(0) {
        buffer_ = new uint8_t[maxDataSize];
    }

    ~ObRTPBuffer() {
        delete[] buffer_;
    }

    void setSequenceNumber(uint64_t seq) {
        sequenceNumber_ = seq;
    }

    uint64_t getSequenceNumber() const {
        return sequenceNumber_;
    }

    void setTimestamp(uint64_t ts) {
        timestamp_ = ts;
    }

    uint64_t getTimestamp() const {
        return timestamp_;
    }

    void setCodecName(const std::string &name) {
        codecName_ = name;
    }

    const std::string &getCodecName() const {
        return codecName_;
    }

    uint8_t *getBuffer() const {
        return buffer_;
    }

    uint32_t getBufferSize() const {
        return bufferSize_;
    }

    void setHeaderSize(uint32_t size) {
        staticHeaderSize_ = size;
    }

    void setDynamicHeaderSize(uint32_t size) {
        dynamicHeaderSize_ = size;
    }

    void setRecvdDataSize(uint32_t size) {
        recvdDataSize_ = size;
    }

    uint32_t getRecvdDataSize() const {
        return recvdDataSize_;
    }

    template <typename T> T *getStaticHeader() const {
        if(staticHeaderSize_ < sizeof(T)) {
            throw invalid_value_exception("Header size is too small to cast to type T");
        }
        return reinterpret_cast<T *>(buffer_);
    }

    uint32_t getStaticHeaderSize() const {
        return staticHeaderSize_;
    }

    template <typename T> T *getDynamicHeader() const {
        if(dynamicHeaderSize_ < sizeof(T)) {
            throw invalid_value_exception("Custom header size is too small to cast to type T");
        }
        return reinterpret_cast<T *>(buffer_ + staticHeaderSize_);
    }

    uint8_t *getRecvdDataBuffer(uint16_t offset = 0) const {
        return buffer_ + staticHeaderSize_ + offset;
    }

    std::shared_ptr<ObRTPBuffer> cloneAndCopyHeader() {
        auto frame = std::make_shared<ObRTPBuffer>(bufferSize_);
        memcpy(frame->getBuffer(), buffer_, staticHeaderSize_);
        frame->setHeaderSize(staticHeaderSize_);
        return frame;
    }

private:
    uint8_t *buffer_;
    uint32_t bufferSize_;

    uint64_t    sequenceNumber_;
    uint64_t    timestamp_;
    std::string codecName_;
    uint32_t    staticHeaderSize_;
    uint32_t    recvdDataSize_;  // size of data received = dataSize_ + dynamicHeaderSize_
    uint32_t    dynamicHeaderSize_;
};

class ObRTPSink : public MediaSink {
public:
    static ObRTPSink *createNew(std::shared_ptr<const StreamProfile> streamProfile,
                                UsageEnvironment    &env,
                                MediaSubsession     &subsession,  // identifies the kind of data that's being received
                                MutableFrameCallback callback,
                                char const          *streamId = NULL);  // identifies the stream itself (optional)

    virtual ~ObRTPSink() noexcept;

protected:
    ObRTPSink(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession, MutableFrameCallback callback, char const *streamId = NULL);

private:
    static void afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                                  unsigned durationInMicroseconds);
    void        afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds);

private:
    // redefined virtual functions:
    virtual Boolean continuePlaying();
    void            outputFrameFunc();

private:
    MediaSubsession                     &subsession_;
    char                                *streamId_;
    MutableFrameCallback                 frameCallback_;
    std::shared_ptr<const StreamProfile> streamProfile_;
    uint64_t                             frameCount_;
    std::atomic<bool>                    destroy_;

    std::queue<std::shared_ptr<ObRTPBuffer>> reclaimedRTPBufferQueue_;
    std::mutex                               reclaimedRTPBufferMutex_;
    std::queue<std::shared_ptr<ObRTPBuffer>> outputRTPBufferQueue_;
    std::mutex                               outputRTPBufferQueueMutex_;
    std::condition_variable                  frameAvailableCv_;
    std::shared_ptr<ObRTPBuffer>             currentBuffer_;

    const uint8_t  MAX_FRAME_NUM       = 4;
    const uint32_t MAX_FRAME_DATA_SIZE = 3 * 3840 * 2160;

    std::thread outputFrameThread_;
};

}  // namespace libobsensor

