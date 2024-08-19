#include "FilterDecorator.hpp"
#include "frame/FrameFactory.hpp"
#include "exception/ObException.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/StringUtils.hpp"
namespace libobsensor {

const size_t DEFAULT_FRAME_QUEUE_CAPACITY = 10;

FilterExtension::FilterExtension(const std::string &name) : name_(name), enabled_(true), configChanged_(false) {
    srcFrameQueue_ = std::make_shared<FrameQueue<const Frame>>(DEFAULT_FRAME_QUEUE_CAPACITY);  // todo： read from config file to set the size of frame queue
    LOG_DEBUG("Filter {} created with frame queue capacity {}", name_, srcFrameQueue_->capacity());
}

FilterExtension::~FilterExtension() noexcept {
    reset();
}

const std::string &FilterExtension::getName() const {
    return name_;
}

void FilterExtension::pushFrame(std::shared_ptr<const Frame> frame) {
    if(!srcFrameQueue_->isStarted()) {
        srcFrameQueue_->start([&](std::shared_ptr<const Frame> frameToProcess) {
            std::shared_ptr<Frame> rstFrame;
            if(enabled_) {
                checkAndUpdateConfig();
                BEGIN_TRY_EXECUTE({ rstFrame = process(frameToProcess); })
                CATCH_EXCEPTION_AND_EXECUTE({  // catch all exceptions to avoid crashing on the inner thread
                    LOG_WARN("Filter {}: exception caught while processing frame {}#{}, this frame will be dropped", name_, frameToProcess->getType(),
                             frameToProcess->getNumber());
                    return;
                })
            }
            else {
                rstFrame = FrameFactory::createFrameFromOtherFrame(frameToProcess, true);
            }
            std::unique_lock<std::mutex> lock(callbackMutex_);
            if(callback_) {
                callback_(rstFrame);
            }
        });
        LOG_DEBUG("Filter {}: start frame queue", name_);
    }
    srcFrameQueue_->enqueue(frame);
}

void FilterExtension::setCallback(FilterCallback cb) {
    std::unique_lock<std::mutex> lock(callbackMutex_);
    callback_ = cb;
}

void FilterExtension::resizeFrameQueue(size_t size) {
    srcFrameQueue_->resize(size);
}

void FilterExtension::reset() {
    srcFrameQueue_->flush();
    srcFrameQueue_->reset();
}

void FilterExtension::enable(bool en) {
    enabled_ = en;
}

bool FilterExtension::isEnabled() const {
    return enabled_;
}

OBFilterConfigValueType parseFilterConfigValueType(const std::string &typeStr) {
    if(typeStr == "integer" || typeStr == "int" || typeStr == "uint8_t" || typeStr == "uint16_t" || typeStr == "uint32_t" || typeStr == "uint64_t"
       || typeStr == "size_t" || typeStr == "int8_t" || typeStr == "int16_t" || typeStr == "int32_t" || typeStr == "int64_t" || typeStr == "enum") {
        return OB_FILTER_CONFIG_VALUE_TYPE_INT;
    }
    else if(typeStr == "float" || typeStr == "double") {
        return OB_FILTER_CONFIG_VALUE_TYPE_FLOAT;
    }
    else if(typeStr == "bool" || typeStr == "boolean") {
        return OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN;
    }
    else {
        LOG_WARN("Invalid filter config type: {}", typeStr);
        return OB_FILTER_CONFIG_VALUE_TYPE_INVALID;
    }
}

double parseFilterConfigValue(const std::string &valueStr, OBFilterConfigValueType valueType) {
    if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_INT) {
        int value;
        if(!utils::string::cvt2Int(valueStr, value)) {
            LOG_WARN("Invalid filter config value for int type: {}", valueStr);
            return 0.0;
        }
        return static_cast<double>(value);
    }
    else if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_FLOAT) {
        float value;
        if(!utils::string::cvt2Float(valueStr, value)) {
            LOG_WARN("Invalid filter config value for float type: {}", valueStr);
        }
        return static_cast<double>(value);
    }
    else if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN) {
        bool value;
        if(!utils::string::cvt2Boolean(valueStr, value)) {
            LOG_WARN("Invalid filter config value for bool type: {}", valueStr);
            return 0.0;
        }
        return value ? 1.0 : 0.0;
    }

    LOG_WARN("Invalid filter config value type: {}", valueType);
    return 0.0;
}

const std::vector<OBFilterConfigSchemaItem> &FilterExtension::getConfigSchemaVec() {
    if(!configSchemaVec_.empty()) {
        return configSchemaVec_;
    }

    // csv format: name，type， min，max，step，default，description
    auto              schemaCSV = getConfigSchema();
    std::stringstream ss(schemaCSV);
    std::string       itemStr;
    while(std::getline(ss, itemStr)) {
        std::vector<std::string> strVec = utils::string::split(itemStr, ",");
        for(auto &str: strVec) {
            str = utils::string::clearHeadAndTailSpace(str);
        }
        configSchemaStrSplittedVec_.emplace_back(strVec);
    }
    for(auto &strVec: configSchemaStrSplittedVec_) {
        if(strVec.size() < 6) {
            LOG_WARN("Filter {}: invalid config schema item: {}", name_, itemStr);
            continue;
        }
        OBFilterConfigSchemaItem item;
        item.name = strVec[0].c_str();
        item.type = parseFilterConfigValueType(strVec[1]);
        item.min  = parseFilterConfigValue(strVec[2], item.type);
        item.max  = parseFilterConfigValue(strVec[3], item.type);
        item.step = parseFilterConfigValue(strVec[4], item.type);
        item.def  = parseFilterConfigValue(strVec[5], item.type);
        if(strVec.size() > 6) {
            item.desc = strVec[6].c_str();
        }
        configSchemaVec_.push_back(item);
    }

    return configSchemaVec_;
}

void FilterExtension::setConfigValue(const std::string &configName, double value) {
    auto schemaVec = getConfigSchemaVec();
    if(schemaVec.empty()) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config schema is empty, doesn't have any config value");
    }

    if(configMap_.empty()) {
        for(auto &item: schemaVec) {
            configMap_[item.name] = item.def;
        }
    }

    auto it =
        std::find_if(configSchemaVec_.begin(), configSchemaVec_.end(), [&configName](const OBFilterConfigSchemaItem &item) { return item.name == configName; });
    if(it == configSchemaVec_.end()) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config item " << configName << " doesn't exist");
    }
    if(value < it->min || value > it->max) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config item " << configName << " value " << value
                                                                 << " out of range [" << it->min << ", " << it->max << "]");
    }

    std::unique_lock<std::mutex> lock(configMutex_);
    configChanged_         = true;
    configMap_[configName] = value;  // store the value in the map, will be applied in the next process() call
    LOG_DEBUG("Filter {}: config item {} value set to {}", name_, configName, value);
}

void FilterExtension::setConfigValueSync(const std::string &name, double value) {
    setConfigValue(name, value);
    checkAndUpdateConfig();
}

double FilterExtension::getConfigValue(const std::string &configName) {
    auto schemaVec = getConfigSchemaVec();
    if(schemaVec.empty()) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config schema is empty, doesn't have any config value");
    }

    if(configMap_.empty()) {
        for(auto &item: schemaVec) {
            configMap_[item.name] = item.def;
        }
    }

    auto it = configMap_.find(configName);
    if(it == configMap_.end()) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config item " << configName << " doesn't exist");
    }
    return it->second;
}

std::string filterConfigValueToString(double value, OBFilterConfigValueType valueType) {
    if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_INT) {
        return std::to_string(static_cast<int>(value));
    }
    else if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_FLOAT) {
        return std::to_string(value);
    }
    else if(valueType == OB_FILTER_CONFIG_VALUE_TYPE_BOOLEAN) {
        return value ? "1" : "0";
    }
    LOG_WARN("Invalid filter config value type: {}", valueType);
    return "";
}

void FilterExtension::checkAndUpdateConfig() {
    if(configChanged_) {
        std::unique_lock<std::mutex> lock(configMutex_);

        std::vector<std::string> configVec;
        for(auto &item: configSchemaVec_) {
            auto it = configMap_.find(item.name);
            if(it == configMap_.end()) {
                throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config item " << item.name << " doesn't exist");
            }
            auto valueStr = filterConfigValueToString(it->second, item.type);
            configVec.push_back(valueStr);
        }
        updateConfig(configVec);

        configChanged_ = false;
    }
}

void FilterExtension::updateConfigCache(std::vector<std::string> &params) {
    auto schemaVec = getConfigSchemaVec();
    if(schemaVec.empty()) {
        throw invalid_value_exception(utils::string::to_string() << "Filter@" << name_ << ": config schema is empty, doesn't have any config value");
    }

    if(schemaVec.size() == params.size()) {
        for(size_t i = 0; i < schemaVec.size(); i++) {
            auto name        = schemaVec[i].name;
            auto type        = schemaVec[i].type;
            auto value       = parseFilterConfigValue(params[i], type);
            configMap_[name] = value;
        }
    }
}

FilterDecorator::FilterDecorator(const std::string &name, std::shared_ptr<IFilterBase> baseFilter) : FilterExtension(name), baseFilter_(baseFilter) {}

FilterDecorator::~FilterDecorator() noexcept {
    reset();
    LOG_DEBUG("Filter {} destroyed", getName());
}

void FilterDecorator::updateConfig(std::vector<std::string> &params) {
    baseFilter_->updateConfig(params);
    updateConfigCache(params);
}

const std::string &FilterDecorator::getConfigSchema() const {
    return baseFilter_->getConfigSchema();
}

void FilterDecorator::reset() {
    FilterExtension::reset();
    baseFilter_->reset();
    LOG_DEBUG("Filter@{} have been reset", getName());
}

std::shared_ptr<Frame> FilterDecorator::process(std::shared_ptr<const Frame> frame) {
    if(!frame) {
        LOG_WARN("Filter {}: empty frame received, return nullptr", getName());
        return nullptr;
    }

    checkAndUpdateConfig();

    std::unique_lock<std::mutex> lock(processMutex_);
    return baseFilter_->process(frame);
}

std::shared_ptr<IFilterBase> FilterDecorator::getBaseFilter() const {
    return baseFilter_;
}

}  // namespace libobsensor