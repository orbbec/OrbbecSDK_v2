// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RawPhaseStreamer.hpp"
#include "IDevice.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "property/InternalProperty.hpp"
namespace libobsensor {

// Chip defintions
// Since we don't actually have direct access to the chip here, we will need to hardcode a few parameters
const float    CHIP_ADC_UNITS_PER_DEG_C   = 5.45f;  // This is a hardcoded paramter
const uint32_t CHIP_EFUSE_REG_TEMP_CAL_TJ = 20739;  // Read from EFUSE address 0x0B. LSB: Temp Cal Version, MSB: Forced Tj
const uint32_t CHIP_EFUSE_REG_TJ_ADC_VAL  = 1927;   // Read from EFUSE address 0x0C. Temp Sensor ADC value at Tj

#pragma pack(push, 1)
struct InputInfo {
    uint16_t systemId;
    uint16_t nRows;
    uint16_t nCols;
    uint16_t nStreams;
    uint16_t nBitsPerSample;
    uint16_t mode;
};

typedef struct obcTransferHeader {
    uint64_t frameCounter;
    uint16_t extendtionLen;
} ObcTransferHeader;

typedef struct obcMetadataHeader {
    uint64_t timestamp;  // us
    uint16_t width;
    uint16_t height;
    uint32_t size;
} ObcMetadataHeader;

/****aligned 2 bytes****/
typedef struct obcFrameHeader {
    ObcTransferHeader transferHeader;
    ObcMetadataHeader metadataHeader;
} ObcFrameHeader;
#pragma pack(pop)

RawPhaseStreamer::RawPhaseStreamer(IDevice *owner, const std::shared_ptr<IVideoStreamPort> &backend)
    : owner_(owner), backend_(backend), running_(false), depthEngineLoader_(std::make_shared<DepthEngineLoadFactory>()) {
    auto global = depthEngineLoader_->getGlobalContext();
    if(global == nullptr || global->loaded == false) {
        throw io_exception("Failed to load depth engine");
    }
    backendProfileMap_.insert({ { 320, 288 }, { 7680, 434 } });
    backendProfileMap_.insert({ { 640, 576 }, { 7680, 434 } });
    backendProfileMap_.insert({ { 1024, 1024 }, { 8192, 130 } });
    backendProfileMap_.insert({ { 512, 512 }, { 8192, 290 } });

    backendStreamProfileList_ = backend_->getStreamProfileList();
    setStreamProfileList();

    // try to stop stream to avoid that the device is in streaming state due to some reason such as a previous crash
    auto propServer = owner->getPropertyServer();
    if(propServer->isPropertySupported(OB_PROP_STOP_DEPTH_STREAM_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        propServer->setPropertyValueT<bool>(OB_PROP_STOP_DEPTH_STREAM_BOOL, true);
    }
    initNvramData();
    LOG_DEBUG("RawPhaseStreamer created");
}

RawPhaseStreamer::~RawPhaseStreamer() noexcept {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }
    TRY_EXECUTE(stopDepthEngineThread());
    TRY_EXECUTE(stopAllStream());

    if(running_) {
        TRY_EXECUTE(backend_->stopAllStream());
    }
    running_ = false;
}

IDevice *RawPhaseStreamer::getOwner() const {
    return owner_;
}

std::shared_ptr<const SourcePortInfo> RawPhaseStreamer::getSourcePortInfo() const {
    return backend_->getSourcePortInfo();
}

void RawPhaseStreamer::setStreamProfileList() {
    streamProfileList_.clear();
    for(auto &profile: backendStreamProfileList_) {
        auto sp     = profile->as<VideoStreamProfile>();
        auto format = sp->getFormat();
        auto width  = sp->getWidth();
        auto height = sp->getHeight();
        auto fps    = sp->getFps();
        for(auto &item: backendProfileMap_) {
            if(item.second.first != width || item.second.second != height) {
                continue;
            }

            if(!passiveIRModeEnabled_ && item.first.first == 1024 && item.first.second == 1024 && (fps == 30 || fps == 25)) {
                continue;
            }

            auto newSp = StreamProfileFactory::createVideoStreamProfile(OB_STREAM_VIDEO, format, item.first.first, item.first.second, fps);
            streamProfileList_.push_back(newSp);
        }
    }
}

StreamProfileList RawPhaseStreamer::getStreamProfileList() {
    return streamProfileList_;
}

void RawPhaseStreamer::startStream(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) {
    std::unique_lock<std::mutex> streamLock(streamMutex_);

    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        for(auto &cb: callbacks_) {
            auto cbVsp = cb.first->as<VideoStreamProfile>();
            auto spVsp = sp->as<VideoStreamProfile>();
            if(cbVsp->getWidth() != spVsp->getWidth() || cbVsp->getHeight() != spVsp->getHeight() || cbVsp->getFps() != spVsp->getFps()) {
                throw invalid_value_exception("Start stream failed, the stream profile is different from the previous stream profile");
            }
        }
        callbacks_[sp] = callback;
    }

    if(running_) {
        return;
    }

    startDepthEngineThread(sp);

    auto profile    = std::dynamic_pointer_cast<const VideoStreamProfile>(sp);
    auto resolution = std::make_pair(profile->getWidth(), profile->getHeight());
    auto iter =
        std::find_if(backendProfileMap_.begin(), backendProfileMap_.end(),
                     [resolution](const std::pair<std::pair<uint32_t, uint32_t>, std::pair<uint32_t, uint32_t>> &pair) { return pair.first == resolution; });

    if(iter == backendProfileMap_.end()) {
        throw invalid_value_exception("Start stream failed, stream profile not found");
    }
    auto &pair   = iter->second;
    auto  realSp = StreamProfileFactory::createVideoStreamProfile(profile->getType(), profile->getFormat(), pair.first, pair.second, profile->getFps());
    backend_->startStream(realSp, [this](std::shared_ptr<Frame> frame) {
        std::unique_lock<std::mutex> lock(frameQueueMutex_);
        frameQueue_.push(frame);
        frameQueueCV_.notify_one();
    });
    running_ = true;
}

void RawPhaseStreamer::stopStream(std::shared_ptr<const StreamProfile> sp) {
    if(!running_) {
        return;
    }

    std::unique_lock<std::mutex> streamLock(streamMutex_);

    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        auto                        iter = callbacks_.find(sp);
        if(iter == callbacks_.end()) {
            throw invalid_value_exception("Stop stream failed, stream profile not found");
        }

        callbacks_.erase(iter);
        if(!callbacks_.empty()) {
            return;
        }
    }

    backend_->stopAllStream();
    running_ = false;
}

void RawPhaseStreamer::stopAllStream() {
    std::unique_lock<std::mutex> streamLock(streamMutex_);
    if(!running_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }

    backend_->stopAllStream();
    running_ = false;
}

bool RawPhaseStreamer::isRunning() const {
    return running_;
}

void RawPhaseStreamer::parseAndOutputFrame(std::shared_ptr<Frame> frame) {
    InputInfo inputInfo   = { 0 };
    size_t    captureSize = 0, rawFrameSize = 0; /** headerSize = 0*/

    // Parse the metadata from the MIPI header
    YEATS_MIPI_HDR mipiHdr      = { 0 };
    int            mipiHeadSize = sizeof(YEATS_MIPI_HDR) * 2;
    // passive ir
    if(frame->getDataSize() == 4096 * 1154 * 2) {
        mipiHeadSize = sizeof(YEATS_MIPI_HDR);
    }

    auto data = frame->getData();
    // Try reading chip ID to determine if there is padding in header or not
    uint16_t chipId = *(uint16_t *)data;
    if(chipId == 0x5931) {
        // For mode 5, we can read directly into the mipi header
        mipiHdr = *(YEATS_MIPI_HDR *)data;
    }
    else {
        //  int size = sizeof(YEATS_MIPI_HDR) * 2;
        // For mode 4/7, we need to skip every other byte because of padding from Jetson Nano
        std::vector<uint8_t> headerData(sizeof(YEATS_MIPI_HDR) * 2);
        // inFile.read(reinterpret_cast<char *>(headerData.data()), sizeof(YEATS_MIPI_HDR) * 2);
        memcpy(headerData.data(), data, sizeof(YEATS_MIPI_HDR) * 2);
        for(size_t i = 0; i < sizeof(YEATS_MIPI_HDR); ++i) {
            uint8_t *mipiHdrArray = reinterpret_cast<uint8_t *>(&mipiHdr);
            mipiHdrArray[i]       = headerData[i * 2];
        }
    }

    // Note: if you want to use NFOV_2x2BINNED, capture data using NFOV_UNBINNED and then set the inputInfo.mode = 2
    // Read the relevant info from the mipi header
    inputInfo.mode           = mipiHdr.mode_info.bits.mode;
    inputInfo.nRows          = mipiHdr.frameHeight;
    inputInfo.nCols          = mipiHdr.frameWidth;
    inputInfo.nStreams       = mipiHdr.capture_info.bits.nCaptures + 1;  // zero indexed
    inputInfo.nBitsPerSample = (inputInfo.mode == 5) ? 8 : 16;           // mode 5 uses 8 bit compressed, others use 16 bit float

    captureSize = inputInfo.nRows * inputInfo.nCols * inputInfo.nBitsPerSample / 8;

    rawFrameSize = captureSize * inputInfo.nStreams;

    uint8_t *dstData = (uint8_t *)data + rawFrameSize;
    uint8_t *srcData = (uint8_t *)data + rawFrameSize - (inputInfo.nRows * 2);
    memcpy(dstData, srcData, mipiHeadSize);

    auto   global      = depthEngineLoader_->getGlobalContext();
    size_t outputBytes = global->plugin.depth_engine_get_output_frame_size(depthEngineContext_);

    std::vector<uint8_t> outputBuffer(outputBytes, 0);

    // For every frame, read the RAW data and process it.
    k4a_depth_engine_input_frame_info_t  inputFrameInfo  = { 0 };
    k4a_depth_engine_output_frame_info_t outputFrameInfo = { 0 };

    {

        // Need to add in the laser and sensor temperature info.
        // Read this from the chip EFUSE in combination with header.
        uint32_t Tj               = (CHIP_EFUSE_REG_TEMP_CAL_TJ >> 8);
        float    sensorTempOffset = CHIP_EFUSE_REG_TJ_ADC_VAL - Tj * CHIP_ADC_UNITS_PER_DEG_C;

        {

            int32_t resvValue      = ((int32_t)mipiHdr.resv[0]) | ((int32_t)mipiHdr.resv[1] << 16);
            int16_t integerPart    = static_cast<uint16_t>(resvValue / 1000);
            int16_t fractionalPart = static_cast<uint16_t>(resvValue % 1000);

            float tmpValue               = static_cast<float>(integerPart) + static_cast<float>(fractionalPart) / 1000.0f;
            inputFrameInfo.laser_temp[0] = tmpValue;
            // LOG_INFO("get temp from ebd:{}", tmpValue);
            // LOG_INFO("origin data:{:x},{:x},{:x},{:x}", mipiHdr.resv[0], mipiHdr.resv[1], mipiHdr.resv[2], mipiHdr.resv[3]);
        }

        inputFrameInfo.laser_temp[1]               = 0;  // This can just be populated with 0
        inputFrameInfo.sensor_temp                 = (((float)mipiHdr.tempSensorADC.bits.adcVal) - sensorTempOffset) / CHIP_ADC_UNITS_PER_DEG_C;
        inputFrameInfo.center_of_exposure_in_ticks = 0;  // This isn't used within the depth engine

        k4a_depth_engine_output_type_t outputType = k4a_depth_engine_output_type_t::K4A_DEPTH_ENGINE_OUTPUT_TYPE_Z_DEPTH;
        if(passiveIRModeEnabled_) {
            outputType = k4a_depth_engine_output_type_t::K4A_DEPTH_ENGINE_OUTPUT_TYPE_PCM;
        }

        auto retCode = global->plugin.depth_engine_process_frame(depthEngineContext_, (uint8_t *)data + mipiHeadSize, rawFrameSize, outputType,
                                                                 outputBuffer.data(), outputBytes, &outputFrameInfo, &inputFrameInfo);

        if(K4A_DEPTH_ENGINE_RESULT_SUCCEEDED != retCode) {
            LOG_ERROR("Process frame failed! Error code:{}", retCode);
            return;
        }

        // Now we have the output data. They are uint16_t type and AB frame immediately follows Z frame.
        size_t    nPixels     = static_cast<size_t>(outputFrameInfo.output_height) * static_cast<size_t>(outputFrameInfo.output_width);
        uint16_t *outputFrame = reinterpret_cast<uint16_t *>(outputBuffer.data());
        uint16_t *zFrame      = outputFrame;
        uint16_t *abFrame     = outputFrame + nPixels;

        if(outputType == k4a_depth_engine_output_type_t::K4A_DEPTH_ENGINE_OUTPUT_TYPE_PCM) {
            abFrame = zFrame;
        }

        std::lock_guard<std::mutex> lock(cbMtx_);
        for(const auto &iter: callbacks_) {
            if(iter.first->getType() == OB_STREAM_DEPTH && outputType != k4a_depth_engine_output_type_t::K4A_DEPTH_ENGINE_OUTPUT_TYPE_PCM) {
                auto depthStreamProfile = iter.first->as<libobsensor::VideoStreamProfile>();
                auto depthFrame         = FrameFactory::createFrameFromStreamProfile(depthStreamProfile);
                depthFrame->updateData((const uint8_t *)zFrame, nPixels * 2);
                depthFrame->setDataSize(nPixels * 2);
                depthFrame->copyInfoFromOther(frame);
                iter.second(depthFrame);
            }
            else if(iter.first->getType() == OB_STREAM_IR) {
                auto irStreamProfile = iter.first->as<libobsensor::VideoStreamProfile>();
                auto irFrame         = FrameFactory::createFrameFromStreamProfile(irStreamProfile);
                irFrame->updateData((const uint8_t *)abFrame, nPixels * 2);
                irFrame->setDataSize(nPixels * 2);
                irFrame->copyInfoFromOther(frame);
                iter.second(irFrame);
            }
        }
    }
}

k4a_depth_engine_mode_t RawPhaseStreamer::getDepthEngineMode(std::shared_ptr<const StreamProfile> profile) {
    auto                    videoProfile = std::dynamic_pointer_cast<const VideoStreamProfile>(profile);
    k4a_depth_engine_mode_t mode         = K4A_DEPTH_ENGINE_MODE_UNKNOWN;
    if((videoProfile->getWidth() == 1024 && videoProfile->getHeight() == 1024) || ((videoProfile->getWidth() == 8192) && videoProfile->getHeight() == 130)
       || (videoProfile->getWidth() == 4096 && videoProfile->getHeight() == 1154)) {
        if(passiveIRModeEnabled_) {
            mode = K4A_DEPTH_ENGINE_MODE_PCM;
        }
        else {
            mode = K4A_DEPTH_ENGINE_MODE_MEGA_PIXEL;
        }
    }
    else if((videoProfile->getWidth() == 512 && videoProfile->getHeight() == 512) || (videoProfile->getWidth() == 8192 && videoProfile->getHeight() == 290)) {
        mode = K4A_DEPTH_ENGINE_MODE_QUARTER_MEGA_PIXEL;
    }
    else if((videoProfile->getWidth() == 640 && videoProfile->getHeight() == 576) || (videoProfile->getWidth() == 7680 && videoProfile->getHeight() == 434)) {
        mode = K4A_DEPTH_ENGINE_MODE_LT_NATIVE;
    }
    else if((videoProfile->getWidth() == 320 && videoProfile->getHeight() == 288) || (videoProfile->getWidth() == 7680 && videoProfile->getHeight() == 434)) {
        mode = K4A_DEPTH_ENGINE_MODE_LT_SW_BINNING;
    }

    return mode;
}

void RawPhaseStreamer::stopDepthEngineThread() {
    if(!depthEngineThread_.joinable()) {
        return;
    }
    depthEngineThreadExit_ = true;
    {
        std::unique_lock<std::mutex> lock(frameQueueMutex_);
        frameQueueCV_.notify_one();
    }
    depthEngineThread_.join();
}

void RawPhaseStreamer::startDepthEngineThread(std::shared_ptr<const StreamProfile> profile) {
    if(depthEngineThread_.joinable()) {
        auto lastVsp = lastStreamProfile_->as<VideoStreamProfile>();
        auto vsp     = profile->as<VideoStreamProfile>();
        if(lastVsp->getWidth() == vsp->getWidth() && lastVsp->getHeight() == vsp->getHeight()) {
            return;
        }
        stopDepthEngineThread();
    }

    lastStreamProfile_     = profile;
    depthEngineThreadExit_ = false;
    depthEngineThread_     = std::thread([profile, this] {
        // depth engine must be initialize, using and deinitialize in the same thread
        // todo: catch exceptions
        initDepthEngine(profile);  // init depth engine
        std::shared_ptr<Frame> frame;
        while(!depthEngineThreadExit_) {
            {
                std::unique_lock<std::mutex> lock(frameQueueMutex_);
                frameQueueCV_.wait(lock, [&]() { return !frameQueue_.empty() || depthEngineThreadExit_; });
                if(depthEngineThreadExit_) {
                    break;
                }

                frame = frameQueue_.front();
                frameQueue_.pop();
            }
            parseAndOutputFrame(frame);
        }
        deinitDepthEngine();
    });
}

void RawPhaseStreamer::initDepthEngine(std::shared_ptr<const StreamProfile> profile) {
    LOG_DEBUG("Depth engine create and initialize...");

    waitNvramDataReady();

    k4a_depth_engine_input_type_t deInputType = k4a_depth_engine_input_type_t::K4A_DEPTH_ENGINE_INPUT_TYPE_UNKNOWN;
    k4a_depth_engine_mode_t       mode        = K4A_DEPTH_ENGINE_MODE_UNKNOWN;

    auto videoProfile = std::dynamic_pointer_cast<const VideoStreamProfile>(profile);
    mode              = getDepthEngineMode(videoProfile);

    deInputType = (mode == K4A_DEPTH_ENGINE_MODE_MEGA_PIXEL) ? k4a_depth_engine_input_type_t::K4A_DEPTH_ENGINE_INPUT_TYPE_8BIT_COMPRESSED_RAW
                                                             : k4a_depth_engine_input_type_t::K4A_DEPTH_ENGINE_INPUT_TYPE_16BIT_RAW;

    curDepthEngineMode_                    = mode;
    auto                           global  = depthEngineLoader_->getGlobalContext();
    k4a_depth_engine_result_code_t retCode = global->plugin.depth_engine_create_and_initialize(&depthEngineContext_, nvramData_.size(), nvramData_.data(), mode,
                                                                                               deInputType, nullptr, nullptr, nullptr);

    if(K4A_DEPTH_ENGINE_RESULT_SUCCEEDED != retCode) {
        throw io_exception(utils::string::to_string() << "Depth engine create and initialize failed,retCode" << retCode);
    }

    LOG_DEBUG("Depth engine init succeed!");
}

void RawPhaseStreamer::deinitDepthEngine() {
    auto global = depthEngineLoader_->getGlobalContext();
    global->plugin.depth_engine_destroy(&depthEngineContext_);
    depthEngineContext_ = nullptr;
}

void RawPhaseStreamer::initNvramData() {
    auto global = depthEngineLoader_->getGlobalContext();
    if(!global->loaded) {
        throw io_exception("Failed to load depth engine plugin");
    }
    LOG_INFO("Succeed to load depth engine plugin");

    std::shared_ptr<const StreamProfile> firmwareDataProfile;
    for(auto &sp: backendStreamProfileList_) {
        auto profile = sp->as<VideoStreamProfile>();
        if(profile->getWidth() == 1024 && profile->getHeight() == 512 && profile->getFps() == 5) {
            firmwareDataProfile = sp;
            break;
        }
    }
    if(!firmwareDataProfile) {
        throw io_exception("Can not find firmware data profile.");
    }

    {
        std::unique_lock<std::mutex> streamLock(streamMutex_);

        backend_->startStream(firmwareDataProfile, [&](std::shared_ptr<const Frame> frame) {
            ObcFrameHeader *frameHeader = (ObcFrameHeader *)frame->getData();
            nvramData_.resize(frameHeader->metadataHeader.size);
            {
                std::unique_lock<std::mutex> lk(nvramMutex_);
                memcpy(nvramData_.data(), (uint8_t *)frame->getData() + sizeof(ObcFrameHeader), frameHeader->metadataHeader.size);
            }
            nvramCV_.notify_all();
        });
    }

    auto wait_thread = std::thread([this]() {
        std::unique_lock<std::mutex> streamLock(streamMutex_);
        waitNvramDataReady();
        backend_->stopAllStream();
    });
    wait_thread.detach();
}

void RawPhaseStreamer::waitNvramDataReady() {
    std::unique_lock<std::mutex> lk(nvramMutex_);
    auto                         state = nvramCV_.wait_for(lk, std::chrono::seconds(10), [this] { return !nvramData_.empty(); });
    if(!state) {
        throw io_exception("Failed to get NVRAM data, timeout!");
    }
    LOG_DEBUG("NVRAM data is ready!");
}

void RawPhaseStreamer::enablePassiveIRMode(bool enable) {
    passiveIRModeEnabled_ = enable;

    for(auto &item: backendProfileMap_) {
        if(item.first.first != 1024 || item.first.second != 1024) {
            continue;
        }
        if(passiveIRModeEnabled_) {
            item.second = { 8192, 130 };
        }
        else {
            item.second = { 4096, 1154 };
        }
        break;
    }

    setStreamProfileList();
}

}  // namespace libobsensor
