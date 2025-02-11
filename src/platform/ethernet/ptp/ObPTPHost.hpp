#pragma once

#include "ObPTPPacketCreator.hpp"
#include <string>
#include <thread>
#include <functional>

namespace libobsensor {

typedef std::function<void()> PTPTimeSyncCallback;

class ObPTPHost {
public:
    ObPTPHost(std::string localMac, std::string localIP, std::string address, uint16_t port, std::string mac)
        : localMac_(localMac), localIp_(localIP), serverIp_(address), serverPort_(port), serverMac_(mac){};

    virtual ~ObPTPHost() = default;

    virtual void timeSync() = 0;
    virtual void destroy()  = 0;
    virtual void setPTPTimeSyncCallback(PTPTimeSyncCallback callback) = 0;
    
protected:
    ObPTPPacketCreator ptpPacketCreator_;

    std::string localMac_;
    std::string localIp_;
    std::string serverIp_;
    uint16_t    serverPort_;
    std::string serverMac_;
};

}  // namespace libobsensor
