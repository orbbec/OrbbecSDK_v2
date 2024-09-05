#include "Config.hpp"
#include "logger/Logger.hpp"
#include "stream/StreamProfileFactory.hpp"
#include <algorithm>

namespace libobsensor {

void Config::enableStream(std::shared_ptr<const StreamProfile> prf) {
    auto iter = std::find_if(enabledStreamProfileList_.begin(), enabledStreamProfileList_.end(),
                             [prf](const std::shared_ptr<const StreamProfile> &p) { return p->getType() == prf->getType(); });
    if(iter != enabledStreamProfileList_.end()) {
        enabledStreamProfileList_.erase(iter);
    }
    enabledStreamProfileList_.push_back(prf);
}

void Config::enableStream(OBStreamType type) {
    auto prf = StreamProfileFactory::createStreamProfile(type);
    enableStream(prf);
}

void Config::enableVideoStream(OBStreamType type, uint32_t width, uint32_t height, uint32_t fps, OBFormat format) {
    auto prf = StreamProfileFactory::createVideoStreamProfile(type, format, width, height, fps);
    enableStream(prf);
}
void Config::enableAccelStream(OBAccelFullScaleRange fullScaleRange, ob_accel_sample_rate sampleRate) {
    auto prf = StreamProfileFactory::createAccelStreamProfile(fullScaleRange, sampleRate);
    enableStream(prf);
}

void Config::enableGyroStream(OBGyroFullScaleRange fullScaleRange, ob_gyro_sample_rate sampleRate) {
    auto prf = StreamProfileFactory::createGyroStreamProfile(fullScaleRange, sampleRate);
    enableStream(prf);
}

void Config::disableStream(OBStreamType type) {
    auto prfIter = enabledStreamProfileList_.begin();
    while(prfIter != enabledStreamProfileList_.end()) {
        if((*prfIter)->getType() == type) {
            enabledStreamProfileList_.erase(prfIter);
            break;
        }
        else {
            prfIter++;
        }
    }
}

StreamProfileList Config::getEnabledStreamProfileList() const {
    return enabledStreamProfileList_;
}

std::shared_ptr<const StreamProfile> Config::getEnabledStreamProfile(OBStreamType type) const {
    for(auto prf: enabledStreamProfileList_) {
        if(prf->getType() == type) {
            return prf;
        }
    }
    return nullptr;
}

void Config::setAlignMode(OBAlignMode mode) {
    alignMode_ = mode;
}

OBAlignMode Config::getAlignMode() const {
    return alignMode_;
}

void Config::setDepthScaleAfterAlignRequire(bool enable) {
    depthScaleRequire_ = enable;
}

bool Config::getDepthScaleAfterAlignRequire() const {
    return depthScaleRequire_;
}

void Config::disableAllStream() {
    enabledStreamProfileList_.clear();
}

void Config::setFrameAggregateOutputMode(OBFrameAggregateOutputMode mode) {
    frameAggregateOutputMode_ = mode;
}

OBFrameAggregateOutputMode Config::getFrameAggregateOutputMode() const {
    return frameAggregateOutputMode_;
}

bool Config::operator==(const Config &cmp) const {
    if(cmp.alignMode_ != alignMode_ || cmp.depthScaleRequire_ != depthScaleRequire_
       || cmp.enabledStreamProfileList_.size() != enabledStreamProfileList_.size()) {
        return false;
    }

    for(const auto &sp: enabledStreamProfileList_) {
        bool found = false;
        for(const auto &cmpSp: cmp.enabledStreamProfileList_) {
            if(sp.get() == cmpSp.get()) {
                found = true;
                break;
            }
        }

        if(!found) {
            return false;
        }
    }

    return true;
}

bool Config::operator!=(const Config &cmp) const {
    return !operator==(cmp);
}

std::shared_ptr<Config> Config::clone() const {
    auto config                       = std::make_shared<Config>();
    config->alignMode_                = alignMode_;
    config->depthScaleRequire_        = depthScaleRequire_;
    config->enabledStreamProfileList_ = enabledStreamProfileList_;
    config->frameAggregateOutputMode_ = frameAggregateOutputMode_;
    return config;
}

bool Config::isStreamEnabled(OBStreamType type) const {
    for(auto sp: enabledStreamProfileList_) {
        if(sp->getType() == type) {
            return true;
        }
    }
    return false;
}

}  // namespace libobsensor