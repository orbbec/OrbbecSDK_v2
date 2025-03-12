#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include "NpCapLoader.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"
#include "environment/EnvConfig.hpp"

namespace libobsensor {

NpCapLoader::NpCapLoader() : dylib_(nullptr), wpcapDllLoaded_(false) {}

NpCapLoader::~NpCapLoader() {
    unload();
}

NpCapLoader &NpCapLoader::getInstance() {
    static NpCapLoader instance;
    return instance;
}

bool NpCapLoader::init() {
    std::lock_guard<std::mutex> lock(loadMtx_);
    {
        if(wpcapDllLoaded_) {
            LOG_DEBUG("wpcap.dll is already loaded.");
            return true;
        }

        std::string wpcapLoadPath = EnvConfig::getExtensionsDirectory() + "/wpcap/";

        dylib_ = std::make_shared<dylib>(wpcapLoadPath, "wpcap");
        if(!dylib_) {
            LOG_ERROR("Failed to load wpcap.dll!");
            return false;
        }

        void *pcapFindalldevs = dylib_->get_symbol("pcap_findalldevs");
        if(pcapFindalldevs == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_findalldevs!");
            return false;
        }
        pcap_findalldevs_ = reinterpret_cast<pcap_findalldevs_t>(pcapFindalldevs);

        void *pcapOpenLive = dylib_->get_symbol("pcap_open_live");
        if(pcapOpenLive == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_open_live!");
            return false;
        }
        pcap_open_live_ = reinterpret_cast<pcap_open_live_t>(pcapOpenLive);

        void *pcapDatalink = dylib_->get_symbol("pcap_datalink");
        if(pcapDatalink == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_datalink!");
            return false;
        }
        pcap_datalink_ = reinterpret_cast<pcap_datalink_t>(pcapDatalink);

        void *pcapCompile = dylib_->get_symbol("pcap_compile");
        if(pcapCompile == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_compile!");
            return false;
        }
        pcap_compile_ = reinterpret_cast<pcap_compile_t>(pcapCompile);

        void *pcapSetfilter = dylib_->get_symbol("pcap_setfilter");
        if(pcapSetfilter == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_setfilter!");
            return false;
        }
        pcap_setfilter_ = reinterpret_cast<pcap_setfilter_t>(pcapSetfilter);

        void *pcapFreecode = dylib_->get_symbol("pcap_freecode");
        if(pcapFreecode == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_freecode!");
            return false;
        }
        pcap_freecode_ = reinterpret_cast<pcap_freecode_t>(pcapFreecode);

        void *pcapClose = dylib_->get_symbol("pcap_close");
        if(pcapClose == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_close!");
            return false;
        }
        pcap_close_ = reinterpret_cast<pcap_close_t>(pcapClose);

        void *pcapFreealldevs = dylib_->get_symbol("pcap_freealldevs");
        if(pcapFreealldevs == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_freealldevs!");
            return false;
        }
        pcap_freealldevs_ = reinterpret_cast<pcap_freealldevs_t>(pcapFreealldevs);

        void *pcapNextEx = dylib_->get_symbol("pcap_next_ex");
        if(pcapNextEx == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_next_ex!");
            return false;
        }
        pcap_next_ex_ = reinterpret_cast<pcap_next_ex_t>(pcapNextEx);

        void *pcapGeterr = dylib_->get_symbol("pcap_geterr");
        if(pcapGeterr == nullptr) {
            LOG_ERROR("Failed to get function pointer for pcap_geterr!");
            return false;
        }
        pcap_geterr_ = reinterpret_cast<pcap_geterr_t>(pcapGeterr);

        wpcapDllLoaded_ = true;
        LOG_DEBUG("wpcap.dll loaded successfully.");
        return true;
    }
}

void NpCapLoader::unload() {
    if(wpcapDllLoaded_ && dylib_ != nullptr) {
        wpcapDllLoaded_ = false;
        std::cout << "wpcap.dll unloaded." << std::endl;
    }
}


}  // namespace libobsensor

#endif