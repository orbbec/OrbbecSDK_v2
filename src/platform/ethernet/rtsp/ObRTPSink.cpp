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

ObRTPSink *ObRTPSink::createNew(std::shared_ptr<const StreamProfile> streamProfile, UsageEnvironment &env, MediaSubsession &subsession, MutableFrameCallback callback, char const *streamId) {
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

ObRTPSink::~ObRTPSink() noexcept{
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
    currentBuffer_->setRecvdDataSize(frameSize);
    currentBuffer_->setSequenceNumber(frameCount_);
    currentBuffer_->setCodecName(subsession_.codecName());
    auto tsp = subsession_.getNormalPlayTime(presentationTime);
    currentBuffer_->setTimestamp(static_cast<uint64_t>(tsp));

    {
        std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
        outputRTPBufferQueue_.push(currentBuffer_);
        currentBuffer_.reset();
    }
    frameAvailableCv_.notify_all();
    frameCount_++;

    // Then continue, to request the next frame of data:
    continuePlaying();

    // uint64_t start  = utils::getNowTimesUs();
    // uint64_t curTsp = utils::getNowTimesUs();

    // static uint64_t dataLenCnt_ = 0;
    // static uint64_t frameCnt_   = 0;
    // static uint64_t lastTsp     = 0;
    // static float    fps;
    // static float    dataRate_;

    // dataLenCnt_ += frameSize;
    // frameCnt_ += 1;
    // if(lastTsp != 0 && curTsp -lastTsp >= 1000) {
    // auto timeDiff = curTsp -lastTsp;
    // fps = (double)frameCnt_/timeDiff *1000;
    // frameCnt_ = 0;
    // dataRate_ = (double)dataLenCnt_ *8 /1000 /1000 /timeDiff *1000;
    // dataLenCnt_ = 0;
    // lastTsp = curTsp;
    // }
    // if(lastTsp == 0) {
    // lastTsp = curTsp;
    // }
    // char uSecsStr[7] = { 0 };
    // sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);

    // std::string logStr = utils::string::to_string() << "New frame received: type=" << subsession_.codecName() << ", tsp=" << (int)presentationTime.tv_sec
    // <<
    //"."
    //                                           << uSecsStr << ", framesize=" << frameSize << ", index=" << *(uint32_t *)frame->frameObj->frameData;
    // if(fps != 0) {
    //     logStr = logStr + ", avrFps=" + std::to_string(fps) + ", avrBitRate=" + std::to_string(dataRate_) + "Mbps";
    // }
    // envir() << logStr.c_str();

    // do {
    //     if(strcmp(subsession_.codecName(), "OB_FMT_Y16") == 0) {
    //         currentBuffer_->setHeaderSize(sizeof(OBNetworkFrameHeader));
    //         auto header          = currentBuffer_->getDynamicHeader<OBNetworkFrameHeader>();
    //         frameObj->index      = header->frameCounter;
    //         frameObj->systemTime = curTsp;
    //         frameObj->deviceTime = header->timestamp;
    //         // LOG_INFO("TimeStamp: {0}(0x{0:8x}), SysTimeStamp: {1}", frameObj->deviceTime, frameObj->systemTime);
    //         frameObj->format       = OB_FORMAT_Y16;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);  // Y16 format needs to remove the network header
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     // todo-lingyi adds Y8, yuyv format, RVL format and Y10 format
    //     else if(strcmp(subsession_.codecName(), "OB_FMT_Y8") == 0) {
    //         auto header            = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index        = header->frameCounter;
    //         frameObj->systemTime   = curTsp;
    //         frameObj->deviceTime   = header->timestamp;
    //         frameObj->format       = OB_FORMAT_Y8;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);  // Y8 format needs to remove the network header
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else if(strcmp(subsession_.codecName(), "OB_FMT_Y10") == 0) {
    //         auto header            = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index        = header->frameCounter;
    //         frameObj->systemTime   = curTsp;
    //         frameObj->deviceTime   = header->timestamp;
    //         frameObj->format       = OB_FORMAT_Y10;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);  // Y10 format needs to remove the network header
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else if(strcmp(subsession_.codecName(), "OB_FMT_RVL") == 0) {
    //         auto header            = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index        = header->frameCounter;
    //         frameObj->systemTime   = curTsp;
    //         frameObj->deviceTime   = header->timestamp;
    //         frameObj->format       = OB_FORMAT_RVL;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);  // The RVL format needs to remove the network header
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else if(strcmp(subsession_.codecName(), "YUYV") == 0) {
    //         auto header          = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index      = header->frameCounter;
    //         frameObj->systemTime = curTsp;
    //         frameObj->deviceTime = header->timestamp;
    //         // LOG_INFO("TimeStamp: {0}(0x{0:8x}), SysTimeStamp: {1}", frameObj->deviceTime, frameObj->systemTime);
    //         frameObj->format    = OB_FORMAT_YUYV;
    //         frameObj->frameSize = frameSize - sizeof(OBNetworkFrameHeader) - header->extentionLen;
    //         frameObj->frameData =
    //             currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader) + header->extentionLen;  // YUYV format needs to remove the network header
    //         frameObj->metadataSize = header->extentionLen;
    //         frameObj->metadata     = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);
    //     }
    //     else if(strcmp(subsession_.codecName(), "H264") == 0) {
    //         // frame->frameHeaderSize is actually the starting address of frame->frameObj->frameData valid data
    //         if((currentBuffer_->frameDataBuffer[currentBuffer_->frameHeaderSize] & 0x1f) == H264_NAL_SPS
    //            || (currentBuffer_->frameDataBuffer[currentBuffer_->frameHeaderSize] & 0x1f) == H264_NAL_PPS) {  // SPS(7) or PPS(8)
    //             break;
    //         }
    //         else {
    //             frameObj->index      = frameCount_;
    //             frameObj->systemTime = curTsp;
    //             // frameObj->deviceTime   = (uint64_t)presentationTime.tv_sec *1000000 + (int)presentationTime.tv_usec;
    //             // frameObj->deviceTime   = ((RTPSource *)fSource)->curPacketRTPTimestamp();
    //             frameObj->deviceTime   = subsession_.getNormalPlayTime(presentationTime);
    //             frameObj->format       = OB_FORMAT_H264;
    //             frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize;
    //             frameObj->frameData    = currentBuffer_->frameDataBuffer;
    //             frameObj->metadataSize = 0;
    //             frameObj->metadata     = nullptr;
    //         }
    //     }
    //     else if(strcmp(subsession_.codecName(), "H265") == 0) {
    //         // frame->frameHeaderSize is actually the starting address of frame->frameObj->frameData valid data
    //         if(((currentBuffer_->frameDataBuffer[currentBuffer_->frameHeaderSize] & 0x7f) >> 1) == H265_NAL_VPS
    //            || ((currentBuffer_->frameDataBuffer[currentBuffer_->frameHeaderSize] & 0x7f) >> 1) == H265_NAL_SPS
    //            || ((currentBuffer_->frameDataBuffer[currentBuffer_->frameHeaderSize] & 0x7f) >> 1) == H265_NAL_PPS) {
    //             break;
    //         }
    //         else {
    //             frameObj->index      = frameCount_;
    //             frameObj->systemTime = curTsp;
    //             // frameObj->deviceTime   = (uint64_t)presentationTime.tv_sec *1000000 + (int)presentationTime.tv_usec;
    //             // frameObj->deviceTime   = ((RTPSource *)fSource)->curPacketRTPTimestamp();
    //             frameObj->deviceTime   = subsession_.getNormalPlayTime(presentationTime);
    //             frameObj->format       = OB_FORMAT_H265;
    //             frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize;
    //             frameObj->frameData    = currentBuffer_->frameDataBuffer;
    //             frameObj->metadataSize = 0;
    //             frameObj->metadata     = nullptr;
    //         }
    //     }
    //     else if(strcmp(subsession_.codecName(), "JPEG") == 0) {
    //         frameObj->index      = frameCount_;
    //         frameObj->systemTime = curTsp;
    //         // frameObj->deviceTime   = (uint64_t)presentationTime.tv_sec *1000000 + (int)presentationTime.tv_usec;
    //         // frameObj->deviceTime   = ((RTPSource *)fSource)->curPacketRTPTimestamp();
    //         frameObj->deviceTime = subsession_.getNormalPlayTime(presentationTime);
    //         // LOG_INFO("TimeStamp: {0}(0x{0:8x}), SysTimeStamp: {1}", frameObj->deviceTime, frameObj->systemTime);
    //         frameObj->format    = OB_FORMAT_MJPG;
    //         frameObj->frameSize = frameSize + currentBuffer_->frameHeaderSize;
    //         // Single machine can trigger color arbitrarily. There are non-frame packets with a length of 556. Call the frame callback @LingYi
    //         // if(frameObj->frameSize == 556) skip secondary packets
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer;
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else if(strcmp(subsession_.codecName(), "OB_FMT_MJPEG") == 0) {
    //         auto header            = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index        = frameCount_;
    //         frameObj->systemTime   = curTsp;
    //         frameObj->deviceTime   = header->timestamp;
    //         frameObj->format       = OB_FORMAT_MJPG;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else if(strcmp(subsession_.codecName(), "OB_FMT_Y8") == 0) {
    //         auto header            = (OBNetworkFrameHeader *)currentBuffer_->frameDataBuffer;
    //         frameObj->index        = header->frameCounter;
    //         frameObj->systemTime   = curTsp;
    //         frameObj->deviceTime   = header->timestamp;
    //         frameObj->format       = OB_FORMAT_Y8;
    //         frameObj->frameSize    = frameSize + currentBuffer_->frameHeaderSize - sizeof(OBNetworkFrameHeader);
    //         frameObj->frameData    = currentBuffer_->frameDataBuffer + sizeof(OBNetworkFrameHeader);
    //         frameObj->metadataSize = 0;
    //         frameObj->metadata     = nullptr;
    //     }
    //     else {
    //         // VideoFrameObject fo = { frameSize, 0,      frame->frameObj->frameData,
    //         //                    nullptr,   curTsp, (uint64_t)presentationTime.tv_sec *1000 + (int)presentationTime.tv_usec /1000 };
    //         break;
    //     }

    //     {
    //         std::unique_lock<std::mutex> lk(outputRTPBufferQueueMutex_);
    //         outputRTPBufferQueue_.push(currentBuffer_);
    //         currentBuffer_.reset();
    //     }
    //     frameAvailableCv_.notify_all();
    //     frameCount_++;
    // } while(0);

    // // Then continue, to request the next frame of data:
    // continuePlaying();

    // auto dur = utils::getNowTimesUs() -start;
    // if(dur > 1000) {
    //     LOG_ERROR("Live555 afterGettingFrame dur=" << dur << "usec > 1000 usec";
    // }
    // else if(dur > 100) {
    //     LOG_WARN("Live555 afterGettingFrame dur=" << dur << "usec > 100 usec";
    // }
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
            LOG_WARN("Drop output-frame to receive new frame due to reclaimed-frame queue is empty: frameIndex={0}, devTsp={1}",
                     currentBuffer_->getSequenceNumber(), currentBuffer_->getTimestamp());
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
        if(format != streamProfile_->getFormat()) {
            // LOG_ERROR_INTVA
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
        frame->updateData(output->getRecvdDataBuffer() + frameOffset, output->getRecvdDataSize() - frameOffset);

        frameCallback_(frame);
        {
            std::unique_lock<std::mutex> lk(reclaimedRTPBufferMutex_);
            reclaimedRTPBufferQueue_.push(output);
        }
    }
}

}  // namespace libobsensor
