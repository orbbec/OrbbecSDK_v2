#include "ObRTPSink.hpp"

#include "H264VideoRTPSource.hh"

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"

#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace libobsensor {

#define H264_NAL_SPS 7  // Sequence parameter set
#define H264_NAL_PPS 8  // Picture parameter set

#define H265_NAL_VPS 32  // Video Parameter Set
#define H265_NAL_SPS 33  // Sequence parameter set
#define H265_NAL_PPS 34  // Picture parameter set

#pragma pack(2)
typedef struct {
    uint64_t frameCounter;  // Frame number
    uint16_t extentionLen;  // Extended data length, default is 0
    uint64_t timestamp;     // timestamp
    uint32_t width;         // Frame pixel width
    uint32_t height;        // Frame pixel height
} OBNetworkFrameHeader;
#pragma pack()

const std::vector<std::string> OBVendorCodecs = {
    "OB_FMT_Y16", "OB_FMT_Y8", "OB_FMT_Y10", "OB_FMT_RVL", "OB_FMT_MJPEG", "YUYV",
};

bool isOBVendorCodec(const std::string &codec) {
    return std::find(OBVendorCodecs.begin(), OBVendorCodecs.end(), codec) != OBVendorCodecs.end();
}

const std::map<std::string, OBFormat> CodecToFormatMap = {
    { "H264", OB_FORMAT_H264 },    { "H265", OB_FORMAT_H265 },      { "JPEG", OB_FORMAT_MJPG },      { "OB_FMT_Y16", OB_FORMAT_Y16 },
    { "OB_FMT_Y8", OB_FORMAT_Y8 }, { "OB_FMT_Y10", OB_FORMAT_Y10 }, { "OB_FMT_RVL", OB_FORMAT_RVL }, { "OB_FMT_MJPEG", OB_FORMAT_MJPG },
};

OBFormat codecToOBFormat(const std::string &codec) {
    auto it = CodecToFormatMap.find(codec);
    if(it != CodecToFormatMap.end()) {
        return it->second;
    }
    return OB_FORMAT_UNKNOWN;
}

ObRTPSink *ObRTPSink::createNew(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession,
                                MutableFrameCallback callback, char const *streamId) {
    return new ObRTPSink(streamProfile, env, subsession, callback, streamId);
}

ObRTPSink::ObRTPSink(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession, MutableFrameCallback callback,
                     char const *streamId)
    : MediaSink(env),
      subsession_(subsession),
      frameCallback_(callback),
      streamProfile_(streamProfile),
      frameCount_(0),
      destroy_(false),
      currentBuffer_(nullptr) {
    streamId_ = strDup(streamId);
    envir() << "ObRTPSink created! streamId = " << streamId_ << "\n";

    auto     frame       = std::make_shared<ObRTPBuffer>(MAX_FRAME_DATA_SIZE);
    auto     frameBuffer = frame->getBuffer();
    uint32_t headerSize  = 0;

    // Load vps/pps/sps into the frame header
    if(strcmp(subsession_.codecName(), "H264") == 0) {
        unsigned int  nalUnitNum;
        SPropRecord  *pSPropRecord;
        unsigned char nalUnitStartCode[4] = { 0, 0, 0, 1 };

        pSPropRecord = parseSPropParameterSets(subsession_.fmtp_spropparametersets(), nalUnitNum);
        for(unsigned int i = 0; i < nalUnitNum; i++) {
            memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
            headerSize += 4;
            memcpy(frameBuffer + headerSize, pSPropRecord[i].sPropBytes, pSPropRecord[i].sPropLength);
            headerSize += pSPropRecord[i].sPropLength;
        }

        memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
        headerSize += 4;
        frame->setHeaderSize(headerSize);
        delete[] pSPropRecord;
    }
    else if(strcmp(subsession_.codecName(), "H265") == 0) {
        SPropRecord *sPropRecords[3];
        unsigned     numSPropRecords[3];
        sPropRecords[0] = parseSPropParameterSets(subsession_.fmtp_spropvps(), numSPropRecords[0]);
        sPropRecords[1] = parseSPropParameterSets(subsession_.fmtp_spropsps(), numSPropRecords[1]);
        sPropRecords[2] = parseSPropParameterSets(subsession_.fmtp_sproppps(), numSPropRecords[2]);

        unsigned char nalUnitStartCode[4] = { 0, 0, 0, 1 };

        for(uint8_t i = 0; i < 3; i++) {
            for(uint32_t j = 0; j < numSPropRecords[i]; j++) {
                memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
                headerSize += 4;
                memcpy(frameBuffer + headerSize, sPropRecords[i][j].sPropBytes, sPropRecords[i][j].sPropLength);
                headerSize += sPropRecords[i][j].sPropLength;
            }
            delete[] sPropRecords[i];
        }
        memcpy(frameBuffer + headerSize, &nalUnitStartCode[0], 4);
        headerSize += 4;
        frame->setHeaderSize(headerSize);
    }
    // else: header size is 0

    std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
    for(uint8_t i = 0; i < MAX_FRAME_NUM - 1; i++) {
        auto newFrame = frame->cloneAndCopyHeader();
        reclaimedRTPBufferQueue_.push(newFrame);
    }
    reclaimedRTPBufferQueue_.push(frame);

    outputFrameThread_ = std::thread(&ObRTPSink::outputFrameFunc, this);
}

ObRTPSink::~ObRTPSink() noexcept {
    envir() << "ObRTPSink destructor! streamId = " << streamId_ << "\n";
    frameCallback_ = nullptr;
    destroy_       = true;
    frameAvailableCv_.notify_all();
    delete[] streamId_;
    if(outputFrameThread_.joinable()) {
        outputFrameThread_.join();
    }
}

void ObRTPSink::afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
                                  unsigned durationInMicroseconds) {
    ObRTPSink *sink = (ObRTPSink *)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ObRTPSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned /*duration in microseconds*/) {
    utils::unusedVar(numTruncatedBytes);

    do {
        if(strcmp(subsession_.codecName(), "H264") == 0) {
            auto recv = currentBuffer_->getRecvdDataBuffer();
            if((recv[0] & 0x1f) == H264_NAL_SPS || (recv[0] & 0x1f) == H264_NAL_PPS) {  // SPS(7) or PPS(8)
                break;
            }
        }
        else if(strcmp(subsession_.codecName(), "H265") == 0) {
            auto recv = currentBuffer_->getRecvdDataBuffer();
            if(((recv[0] & 0x7f) >> 1) == H265_NAL_VPS || ((recv[0] & 0x7f) >> 1) == H265_NAL_SPS || ((recv[0] & 0x7f) >> 1) == H265_NAL_PPS) {
                break;
            }
        }

        currentBuffer_->setRecvdDataSize(frameSize);
        currentBuffer_->setCodecName(subsession_.codecName());
        currentBuffer_->setSequenceNumber(frameCount_);
        auto tsp = subsession_.getNormalPlayTime(presentationTime);
        currentBuffer_->setTimestamp(static_cast<uint64_t>(tsp));

        {
            std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
            outputRTPBufferQueue_.push(currentBuffer_);
            currentBuffer_.reset();
        }
        frameAvailableCv_.notify_all();
        frameCount_++;
    } while(0);

    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean ObRTPSink::continuePlaying() {
    if(fSource == NULL || frameCallback_ == NULL) {
        return False;
    }

    if(!currentBuffer_) {
        std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
        if(!reclaimedRTPBufferQueue_.empty()) {
            currentBuffer_ = reclaimedRTPBufferQueue_.front();
            reclaimedRTPBufferQueue_.pop();
        }
    }

    if(!currentBuffer_) {
        // Actively drop frames
        std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
        if(!outputRTPBufferQueue_.empty()) {
            currentBuffer_ = outputRTPBufferQueue_.front();
            outputRTPBufferQueue_.pop();
            LOG_WARN("Drop output-frame to receive new frame due to reclaimed-frame queue is empty: devTsp={}", currentBuffer_->getTimestamp());
        }
    }

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(currentBuffer_->getRecvdDataBuffer(), MAX_FRAME_DATA_SIZE - currentBuffer_->getStaticHeaderSize(), afterGettingFrame, this,
                          onSourceClosure, this);
    return true;
}

void ObRTPSink::outputFrameFunc() {
    while(!destroy_) {
        std::shared_ptr<ObRTPBuffer> output;
        {
            std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
            frameAvailableCv_.wait(lk, [&]() { return !outputRTPBufferQueue_.empty() || destroy_; });
            if(destroy_) {
                break;
            }
            output = outputRTPBufferQueue_.front();
            outputRTPBufferQueue_.pop();
        }

        if(!output) {
            continue;
        }

        auto codecName = output->getCodecName();
        auto format    = codecToOBFormat(codecName);

        do {
            if(format != streamProfile_->getFormat()) {
                LOG_ERROR("Received frame format is not same as required");
                break;
            }

            auto frame = FrameFactory::createFrameFromStreamProfile(streamProfile_);

            uint32_t frameOffset = 0;
            if(isOBVendorCodec(codecName)) {
                frameOffset = sizeof(OBNetworkFrameHeader);
                output->setDynamicHeaderSize(sizeof(OBNetworkFrameHeader));
                auto header = output->getDynamicHeader<OBNetworkFrameHeader>();
                frame->setTimeStampUsec(header->timestamp);
                frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                frame->setNumber(header->frameCounter);
            }
            else {
                frame->setTimeStampUsec(output->getTimestamp());
                frame->setSystemTimeStampUsec(utils::getNowTimesUs());
                frame->setNumber(output->getSequenceNumber());
            }

            if(output->getStaticHeaderSize() > 0) {
                frame->updateData(output->getBuffer(), output->getRecvdDataSize() + output->getStaticHeaderSize());
            }
            else {
                frame->updateData(output->getRecvdDataBuffer() + frameOffset, output->getRecvdDataSize() - frameOffset);
            }

            frameCallback_(frame);
        } while(0);

        {
            std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
            reclaimedRTPBufferQueue_.push(output);
        }
    }
}

}  // namespace libobsensor
