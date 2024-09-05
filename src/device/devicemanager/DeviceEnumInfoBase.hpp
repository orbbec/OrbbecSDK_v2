#pragma once
#include "IDeviceManager.hpp"
#include "context/Context.hpp"

#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

class DeviceEnumInfoBase : public IDeviceEnumInfo {
public:
    DeviceEnumInfoBase(int pid, int vid, const std::string &uid, const std::string &connectionType, const std::string &name, const std::string &deviceSn,
                       const SourcePortInfoList &sourcePortInfoList)
        : pid_(pid),
          vid_(vid),
          uid_(uid),
          connectionType_(connectionType),
          name_(name),
          deviceSn_(deviceSn),
          sourcePortInfoList_(sourcePortInfoList),
          context_(Context::getInstance()) {
        fullName_ = "Orbbec " + name_;
    }

    DeviceEnumInfoBase() : pid_(0), vid_(0), context_(Context::getInstance()) {}

    virtual ~DeviceEnumInfoBase() = default;

    int getPid() const override {
        return pid_;
    }

    int getVid() const override {
        return vid_;
    }

    const std::string &getUid() const override {
        return uid_;
    }

    const std::string &getConnectionType() const override {
        return connectionType_;
    }

    const std::string &getName() const override {
        return name_;
    }

    const std::string &getFullName() const override {
        return fullName_;
    }

    const std::string &getDeviceSn() const override {
        return deviceSn_;
    }

    const SourcePortInfoList &getSourcePortInfoList() const override {
        return sourcePortInfoList_;
    }

    std::shared_ptr<IDeviceManager> getDeviceManager() const override {
        return context_->getDeviceManager();
    }

    bool operator==(const IDeviceEnumInfo &other) const override {
        const auto &otherSourcePortInfoList = other.getSourcePortInfoList();

        bool rst = (other.getUid() == uid_ && otherSourcePortInfoList.size() == sourcePortInfoList_.size());
        if(rst && connectionType_ == "Ethernet") {
            auto netPort      = std::dynamic_pointer_cast<const NetSourcePortInfo>(sourcePortInfoList_.front());
            auto otherNetPort = std::dynamic_pointer_cast<const NetSourcePortInfo>(otherSourcePortInfoList.front());
            rst &= (otherNetPort->address == netPort->address);
        }
        return rst;
    }

protected:
    // device identification info
    int         pid_;
    int         vid_;
    std::string uid_;  // Unique identifier of the port the device is connected to (platform specific)

    std::string connectionType_;  // "Ethernet", "USB2.0", "USB3.0", etc.

    // device info
    std::string name_;
    std::string fullName_;
    std::string deviceSn_;

    // source port info list
    SourcePortInfoList sourcePortInfoList_;

    std::shared_ptr<Context> context_;  // to handle lifespan of the context
};
}  // namespace libobsensor