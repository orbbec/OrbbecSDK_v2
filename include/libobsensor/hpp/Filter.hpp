/**
 * @file  Filter.hpp
 * @brief This file contains the Filter class, which is the processing unit of the SDK that can perform point cloud generation, format conversion, and other
 * functions.
 */
#pragma once

#include "Types.hpp"
#include "Error.hpp"
#include "Frame.hpp"
#include "libobsensor/h/Filter.h"
#include "libobsensor/h/Frame.h"
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <iostream>
#include <vector>

namespace ob {

/**
 * @brief A callback function that takes a shared pointer to a Frame object as its argument.
 */
typedef std::function<void(std::shared_ptr<Frame>)> FilterCallback;

/**
 * @brief The Filter class is the base class for all filters in the SDK.
 */
class Filter : public std::enable_shared_from_this<Filter> {
protected:
    ob_filter                            *impl_ = nullptr;
    std::string                           name_;
    FilterCallback                        callback_;
    std::vector<OBFilterConfigSchemaItem> configSchemaVec_;

protected:
    /**
     * @brief Default constructor with nullptr impl, used for derived classes only.
     */
    Filter() = default;

    void init(ob_filter *impl) {
        impl_           = impl;
        ob_error *error = nullptr;
        name_           = ob_filter_get_name(impl_, &error);
        Error::handle(&error);

        auto configSchemaList = ob_filter_get_config_schema_list(impl_, &error);
        Error::handle(&error);

        auto count = ob_filter_config_schema_list_get_count(configSchemaList, &error);
        Error::handle(&error);

        for(uint32_t i = 0; i < count; i++) {
            auto item = ob_filter_config_schema_list_get_item(configSchemaList, i, &error);
            Error::handle(&error);
            configSchemaVec_.emplace_back(item);
        }
    }

public:
    explicit Filter(ob_filter *impl) {
        init(impl);
    }

    virtual ~Filter() noexcept {
        if(impl_ != nullptr) {
            ob_error *error = nullptr;
            ob_delete_filter(impl_, &error);
            Error::handle(&error, false);
        }
    }

    /**
     * @brief Get the Impl object of the filter.
     *
     * @return ob_filter* The Impl object of the filter.
     */
    ob_filter *getImpl() const {
        return impl_;
    }

    /**
     * @brief Get the type of filter.
     *
     * @return string The type of filte.
     */
    virtual const std::string &getName() const {
        return name_;
    }

    /**
     * @brief Reset the filter, freeing the internal cache, stopping the processing thread, and clearing the pending buffer frame when asynchronous processing
     * is used.
     */
    virtual void reset() const {
        ob_error *error = nullptr;
        ob_filter_reset(impl_, &error);
        Error::handle(&error);
    }

    /**
     * @brief enable the filter
     */
    virtual void enable(bool enable) const {
        ob_error *error = nullptr;
        ob_filter_enable(impl_, enable, &error);
        Error::handle(&error);
    }

    /**
     * @brief Return Enable State
     */
    virtual bool isEnabled() const {
        ob_error *error  = nullptr;
        bool      enable = ob_filter_is_enabled(impl_, &error);
        Error::handle(&error);
        return enable;
    }

    /**
     * @brief Processes a frame synchronously.
     *
     * @param frame The frame to be processed.
     * @return std::shared_ptr< Frame > The processed frame.
     */
    virtual std::shared_ptr<Frame> process(std::shared_ptr<const Frame> frame) const {
        ob_error *error  = nullptr;
        auto      result = ob_filter_process(impl_, frame->getImpl(), &error);
        Error::handle(&error);
        return std::make_shared<Frame>(result);
    }

    /**
     * @brief Pushes the pending frame into the cache for asynchronous processing.
     *
     * @param frame The pending frame. The processing result is returned by the callback function.
     */
    virtual void pushFrame(std::shared_ptr<Frame> frame) const {
        ob_error *error = nullptr;
        ob_filter_push_frame(impl_, frame->getImpl(), &error);
        Error::handle(&error);
    }

    /**
     * @brief Set the callback function for asynchronous processing.
     *
     * @param callback The processing result callback.
     */
    virtual void setCallBack(FilterCallback callback) {
        callback_       = callback;
        ob_error *error = nullptr;
        ob_filter_set_callback(impl_, &Filter::filterCallback, this, &error);
        Error::handle(&error);
    }

    /**
     * @brief Get config schema of the filter
     * @brief The returned string is a csv format string representing the configuration schema of the filter. The format of the string is:
     *  <parameter_name>, <parameter_type: "int", "float", "bool">, <minimum_value>, <maximum_value>, <value_step>, <default_value>, <parameter_description>
     *
     * @return std::string The config schema of the filter.
     */
    virtual std::string getConfigSchema() const {
        ob_error *error  = nullptr;
        auto      schema = ob_filter_get_config_schema(impl_, &error);
        Error::handle(&error);
        return schema;
    }

    /**
     * @brief Get the Config Schema Vec object
     * @brief The returned vector contains the config schema items. Each item in the vector is an @ref OBFilterConfigSchemaItem object.
     *
     * @return std::vector<OBFilterConfigSchemaItem> The vector of the filter config schema.
     */
    virtual std::vector<OBFilterConfigSchemaItem> getConfigSchemaVec() const {
        return configSchemaVec_;
    }

    /**
     * @brief Set the filter config value by name.
     *
     * @attention The pass into value type is double, witch will be cast to the actual type inside the filter. The actual type can be queried by the filter
     * config schema returned by @ref getConfigSchemaVec.
     *
     * @param configName The name of the config.
     * @param value The value of the config.
     */
    virtual void setConfigValue(const std::string &configName, double value) const {
        ob_error *error = nullptr;
        ob_filter_set_config_value(impl_, configName.c_str(), value, &error);
        Error::handle(&error);
    }

    /**
     * @brief Get the Config Value object by name.
     *
     * @attention The returned value type has been casted to double inside the filter. The actual type can be queried by the filter config schema returned by
     * @ref getConfigSchemaVec.
     *
     * @param configName  The name of the config.
     * @return double The value of the config.
     */
    virtual double getConfigValue(const std::string &configName) const {
        ob_error *error = nullptr;
        double    value = ob_filter_get_config_value(impl_, configName.c_str(), &error);
        Error::handle(&error);
        return value;
    }

private:
    static void filterCallback(ob_frame *frame, void *userData) {
        auto filter = static_cast<Filter *>(userData);
        filter->callback_(std::make_shared<Frame>(frame));
    }

    /**
     * In order to be compatible with the closed source version of orbbecsdk's interface.
     * We recommend using the latest interface names for a better experience.
     */
public:
    virtual const char *type(){
        return getName().c_str();
    }
};

/**
 *  @brief A factory class for creating filters.
 */
class FilterFactory {
public:
    /**
     * @brief Create a filter by name.
     */
    static std::shared_ptr<Filter> createFilter(const std::string &name) {
        ob_error *error = nullptr;
        auto      impl  = ob_create_filter(name.c_str(), &error);
        Error::handle(&error);
        return std::make_shared<Filter>(impl);
    }

    /**
     * @brief Create a private filter by name and activation key.
     * @brief Some private filters require an activation key to be activated, its depends on the vendor of the filter.
     *
     * @param name The name of the filter.
     * @param activation_key The activation key of the filter.
     */
    static std::shared_ptr<Filter> createPrivateFilter(const std::string &name, const std::string &activationKey) {
        ob_error *error = nullptr;
        auto      impl  = ob_create_private_filter(name.c_str(), activationKey.c_str(), &error);
        Error::handle(&error);
        return std::make_shared<Filter>(impl);
    }

    /**
     * @brief Get the vendor specific code of a filter by filter name.
     * @brief A private filter can define its own vendor specific code for specific purposes.
     *
     * @param name The name of the filter.
     * @return std::string The vendor specific code of the filter.
     */
    static std::string getFilterVendorSpecificCode(const std::string &name) {
        ob_error *error = nullptr;
        auto      code  = ob_filter_get_vendor_specific_code(name.c_str(), &error);
        Error::handle(&error);
        return code;
    }
};

class PointCloudFilter : public Filter {
public:
    PointCloudFilter() {
        ob_error *error = nullptr;
        auto      impl  = ob_create_filter("PointCloudFilter", &error);
        Error::handle(&error);
        init(impl);
    }

    virtual ~PointCloudFilter() noexcept = default;

    /**
     * @brief Set the output pointcloud frame format.
     *
     * @param type The point cloud frame format: OB_FORMAT_POINT or OB_FORMAT_RGB_POINT
     */
    void setCreatePointFormat(OBFormat format) {
        setConfigValue("pointFormat", static_cast<double>(format));
    }

    /**
     * @brief Set the point cloud coordinate data zoom factor.
     *
     * @brief Calling this function to set the scale will change the point coordinate scaling factor of the output point cloud frame, The point coordinate
     * scaling factor for the output point cloud frame can be obtained via @ref PointsFrame::getCoordinateValueScale function.
     *
     * @param factor The scale factor.
     */
    void setCoordinateDataScaled(float factor) {
        setConfigValue("coordinateDataScale", factor);
    }

    /**
     * @brief Set point cloud color data normalization.
     * @brief If normalization is required, the output point cloud frame's color data will be normalized to the range [0, 1].
     *
     * @attention This function only works for when create point format is set to OB_FORMAT_RGB_POINT.
     *
     * @param state Whether normalization is required.
     */
    void setColorDataNormalization(bool state) {
        setConfigValue("colorDataNormalization", state);
    }

    /**
     * @brief Set the point cloud coordinate system.
     *
     * @param type The coordinate system type.
     */
    void setCoordinateSystem(OBCoordinateSystemType type) {
        setConfigValue("coordinateSystemType", static_cast<double>(type));
    }

    /**
     * In order to be compatible with the closed source version of orbbecsdk's interface.
     * We recommend using the latest interface names for a better experience.
     */
    OB_DEPRECATED void setPositionDataScaled(float scale){
        setCoordinateDataScaled(scale);
    }
};

class Align : public Filter {
public:
    Align(OBStreamType alignToStreamType) {
        ob_error *error = nullptr;
        auto      impl  = ob_create_filter("Align", &error);
        Error::handle(&error);
        init(impl);

        setConfigValue("AlignType", static_cast<double>(alignToStreamType));
    }

    virtual ~Align() noexcept = default;

    OBStreamType getAlignToStreamType() {
        return static_cast<OBStreamType>(static_cast<int>(getConfigValue("AlignType")));
    }
};

}  // namespace ob
