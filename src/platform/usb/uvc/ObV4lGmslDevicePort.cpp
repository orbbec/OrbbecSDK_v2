#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <list>
#include <regex>

#include <linux/media.h>
#include <linux/videodev2.h>

#include "ObV4lUvcDevicePort.hpp"
#include "ObV4lGmslDevicePort.hpp"
#include "ObV4lGmslHostProtocolTypes.hpp"
#include "libobsensor/h/Property.h"
#include "utils/Utils.hpp"
#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "frame/FrameFactory.hpp"

#include <iostream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <string>

#if 0
//for mutil-device-sync
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

namespace libobsensor {

#define USE_MEMORY_MMAP true
std::mutex mMultiThreadI2CMutex;

static std::map<uint32_t, uint32_t> v4lFourccMapGmsl = {
    { 0x47524559, 0x59382020 }, /* 'GREY' to 'Y8  ' */
    { 0x48455643, 0x48323635 }, /* 'HEVC' to 'H265' */
};

int xioctlGmsl(int fh, unsigned long request, void *arg) {
    int ret   = 0;
    int retry = 5;
    do {
        ret = ioctl(fh, request, arg);
    } while(ret < 0 && (errno == EINTR || errno == EAGAIN) && retry--);
    return ret;
}

int         devPid    = 0;
std::string devSn     = "";
std::string devAsicSn = "";

int getPidSn(const std::string &dev_name, void *data) {
    int ret = 0, fd = -1;
    LOG_DEBUG("-Entry get_pid_sn dev_name:{}", dev_name);

    VALIDATE_NOT_NULL(data);
    fd = open(dev_name.c_str(), O_RDWR);
    if(fd < 0) {
        LOG_DEBUG("GMSL Mipi device get_pid_sn could not be open");
        return fd;
    }
    else {
        LOG_DEBUG("Entry get_pid_sn dev_name  fd:{}", fd);
    }

    v4l2_ext_controls ctrls;
    v4l2_ext_control  ctrl;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ctrls.count      = 1;
    ctrls.controls   = &ctrl;

    ctrl.id   = G2R_CAMERA_CID_GET_PID_SN;
    ctrl.size = 34;
    ctrl.p_u8 = reinterpret_cast<uint8_t *>(data);

    ret = xioctlGmsl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    if(ret < 0) {
        LOG_ERROR("%s:%d ioctl failed on getdata erron:{} strerror:{} \n ", __FILE__, __LINE__, errno, strerror(errno));  // printf err message
        return -1;
    }
    return 0;
}

int getDeviceInfoFromFW(const std::string &dev_name, void *data) {
    int ret = 0, fd = -1;
    LOG_DEBUG("-Entry get_pid_sn dev_name:{}", dev_name);

    VALIDATE_NOT_NULL(data);
    fd = open(dev_name.c_str(), O_RDWR);
    if(fd < 0) {
        LOG_DEBUG("GMSL Mipi device get_pid_sn could not be open");
        return fd;
    }

    v4l2_ext_controls ctrls;
    v4l2_ext_control  ctrl;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    ctrls.count      = 1;
    ctrls.controls   = &ctrl;

    ctrl.id   = G2R_CAMERA_CID_GET_PID_SN;
    ctrl.size = sizeof(orbbec_device_info);  // 34;
    ctrl.p_u8 = reinterpret_cast<uint8_t *>(data);

    ret = xioctlGmsl(fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    if(ret < 0) {
        LOG_DEBUG("ioctl failed on getDeviceInfoFromFW-pid-vid from videox strerror:{}", strerror(errno));  // printf err message
        ret = -1;
    }
    close(fd);
    return ret;
}

enum class Platform { Unknown, Orin, Xavier };

Platform DetectPlatform() {
    std::ifstream file("/proc/version");
    std::string   line;
    if(!file.is_open()) {
        std::cerr << "Failed to open /proc/version" << std::endl;
        return Platform::Unknown;
    }
    if(std::getline(file, line)) {
        // 假设版本字符串以"Linux version "开头，后面跟着版本号
        // 检查是否包含"-tegra"后缀以及版本号中可能包含的其他标识符
        if(line.find("5.15.136-tegra") != std::string::npos) {
            return Platform::Orin;
        }
        else if(line.find("5.10.104-tegra") != std::string::npos) {
            return Platform::Xavier;
        }
        // 你可以根据需要添加更多的条件来检查其他版本或平台
    }
    return Platform::Unknown;
}

//--------------------------------------------------------------------------------------------
// GMSL MIPI & firmware I2C protocol end
static const uint8_t INTERFACE_DEPTH    = 0;
static const uint8_t INTERFACE_COLOR    = 4;
static const uint8_t INTERFACE_IR       = 2;
static const uint8_t INTERFACE_IR_LEFT  = 2;
static const uint8_t INTERFACE_IR_RIGHT = 3;
static const uint8_t INTERFACE_IMU      = 5;

//--------------------------------------------------------------------------------------------
v4l2_capability getV4l2DeviceCapabilitiesGmsl(const std::string &dev_name) {
    // RAII to handle exceptions
    v4l2_capability cap = {};
    memset(&cap, 0, sizeof(cap));
    std::unique_ptr<int, std::function<void(int *)>> fd(new int(open(dev_name.c_str(), O_RDWR | O_NONBLOCK, 0)), [](int *d) {
        if(d && (*d)) {
            ::close(*d);
        }
        delete d;
    });

    if(*fd < 0) {
        LOG_ERROR("getV4l2DeviceCapabilitiesGmsl: Cannot open {}", dev_name);
        return cap;
    }

    if(xioctlGmsl(*fd, VIDIOC_QUERYCAP, &cap) < 0) {
        if(errno == EINVAL) {
            LOG_ERROR("getV4l2DeviceCapabilitiesGmsl {} is no V4L2 device", dev_name);
        }
        else {
            LOG_ERROR("getV4l2DeviceCapabilitiesGmsl  xioctlGmsl(VIDIOC_QUERYCAP) failed!");
        }
    }
    return cap;
}

int checkVideoIndex(const std::string &dev_name) {
    static std::regex video_dev_index("\\d+$");
    std::smatch       match;
    uint8_t           video_index{};
    if(std::regex_search(dev_name, match, video_dev_index)) {
        video_index = static_cast<uint8_t>(std::stoi(match[0]));
    }
    else {
        LOG_ERROR("Unresolved Video4Linux device pattern:  name device is not support!  dev_name:{}", dev_name);
        return -1;
    }
    return video_index;
}

std::vector<std::shared_ptr<V4lDeviceInfoGmsl>> ObV4lGmslDevicePort::queryRelatedDevices(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    std::vector<std::shared_ptr<V4lDeviceInfoGmsl>> devs;
    DIR                                            *dir = opendir("/sys/class/video4linux");
    if(!dir) {
        LOG_DEBUG("Failed to open /sys/class/video4linux, possibly no device connected");
        return devs;
    }

    int min_node = 0, max_node = 0;

    LOG_DEBUG("Entry queryRelatedDevices infName:{}, portInfo->uid:{} ", portInfo->infName, portInfo->uid);
    std::string portInfo_streamType = portInfo->uid.substr(portInfo->uid.find_last_of('-') + 1);
    LOG_DEBUG("-portInfo_streamType:{} ", portInfo_streamType);
    int portInfo_streamType_int = std::stoi(portInfo_streamType);

    int portinfo_videoIndex = checkVideoIndex(portInfo->infName);
    LOG_DEBUG("portinfo_videoIndex: {}", portinfo_videoIndex);

    if(portInfo_streamType_int == ORB_MUX_PAD_DEPTH) {
        min_node = portinfo_videoIndex - 0;
        max_node = portinfo_videoIndex + 7;
    }
    else if(portInfo_streamType_int == ORB_MUX_PAD_RGB) {
        min_node = portinfo_videoIndex - 2;
        max_node = portinfo_videoIndex + 5;
    }
    else if(portInfo_streamType_int == ORB_MUX_PAD_IR_L) {
        min_node = portinfo_videoIndex - 4;
        max_node = portinfo_videoIndex + 3;
    }
    else if(portInfo_streamType_int == ORB_MUX_PAD_IR_R) {
        min_node = portinfo_videoIndex - 6;
        max_node = portinfo_videoIndex + 1;
    }

    struct dirent *entry;
    while((entry = readdir(dir))) {
        std::string name = entry->d_name;
        if(name == "." || name == ".." || name.find("video") == std::string::npos) {
            continue;
        }
        LOG_DEBUG("Found video4linux: {}", name);
        int tmp_videoIndex = checkVideoIndex(name);
        LOG_DEBUG("Found video4linux-tmp_videoIndex: {}", tmp_videoIndex);
        // if(tmp_videoIndex<portinfo_videoIndex)
        //     continue;

        if((tmp_videoIndex < min_node) || (tmp_videoIndex > max_node))
            continue;

        std::string path = "/sys/class/video4linux/" + name;
        std::string realPath{};
        char        buf[PATH_MAX] = { 0 };
        if(realpath(path.c_str(), buf)) {
            realPath = buf;
            if(realPath.find("virtual") != std::string::npos) {
                continue;
            }
        }
        try {
            std::string busNum = "1", devNum = "0", devPath = "/dev/video";
            std::string devname = "/dev/" + name;
            struct stat st      = {};
            if(stat(devname.c_str(), &st) < 0) {
                LOG_DEBUG("Cannot identify {}", devname);
                continue;
            }

            if(!S_ISCHR(st.st_mode)) {
                LOG_DEBUG("{} is no device", devname);
                continue;
            }

            // Search directory and up to three parent directories to find busnum/devnum
            std::ostringstream ss;
            ss << "/sys/dev/char/" << major(st.st_rdev) << ":" << minor(st.st_rdev) << "/device/";
            auto             searchPath         = ss.str();
            auto             validPath          = false;
            static const int MAX_DEV_PARENT_DIR = 10;
            for(int i = 0; i < MAX_DEV_PARENT_DIR; i++) {
                std::ifstream busnum(searchPath + "busnum");
                std::ifstream devnum(searchPath + "devnum");
                std::ifstream devpath(searchPath + "devpath");
                if(busnum.good() && devnum.good() && devpath.good()) {
                    busnum >> busNum;
                    devnum >> devNum;
                    devpath >> devPath;
                    validPath = true;
                }
                busnum.close();
                devnum.close();
                devpath.close();
                if(validPath) {
                    break;
                }
                searchPath += "../";
            }
#if 0
            if(!validPath) {
                LOG_DEBUG("Cannot find busnum/devnum for {}", devname);
                continue;
            }
            std::string modalias;
            if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/modalias") >> modalias)) {
                LOG_DEBUG("Failed to read modalias");
                continue;
            }
            if(modalias.size() < 14 || modalias.substr(0, 5) != "usb:v" || modalias[9] != 'p') {
                LOG_DEBUG("Not a usb format modalias");
                continue;
            }
            if(!(std::istringstream(modalias.substr(5, 4)) >> std::hex >> vid)) {
                LOG_DEBUG("Failed to read vendor ID");
                continue;
            }
            if(!(std::istringstream(modalias.substr(10, 4)) >> std::hex >> pid)) {
                LOG_DEBUG("Failed to read product ID");
                continue;
            }
            if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/bInterfaceNumber") >> std::hex >> mi)) {
                LOG_DEBUG("Failed to read interface number");
                continue;
            }
#endif

            // auto url = busNum + "-" + devPath + "-" + devNum;
            // auto url = "usb://06d0:2bc5/1/0";
            // LOG_DEBUG("----------------------vid: {0}, pid: {1}, portInfo->vid: {2}, portInfo->pid: {3}", vid, pid, portInfo->vid, portInfo->pid);
            //  if(portInfo->vid == vid && portInfo->pid == pid) && portInfo->infIndex == mi && portInfo->url == url)
            {

                // Find the USB specification (USB2/3) type from the underlying device
                // Use device mapping obtained in previous step to traverse node tree
                // and extract the required descriptors
                // Traverse from
                // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/3-6:1.0/video4linux/video0
                // to
                // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/version
                auto info  = std::make_shared<V4lDeviceInfoGmsl>();
                info->name = devname;
                info->cap  = getV4l2DeviceCapabilitiesGmsl(devname);
                devs.push_back(info);
            }
        }
        catch(std::exception &e) {
            LOG_DEBUG("Failed to parse v4l device info: {}", e.what());
        }
    }
    closedir(dir);
    if(devs.empty()) {
        LOG_DEBUG("No v4l device found for port: {}", portInfo->infUrl);
    }

    // sort by video#num, because the metadata device is always found after the video device for the same uvc device
    std::sort(devs.begin(), devs.end(), [](const std::shared_ptr<V4lDeviceInfoGmsl> &a, const std::shared_ptr<V4lDeviceInfoGmsl> &b) {
        int numA = std::stoi(a->name.substr(10));
        int numB = std::stoi(b->name.substr(10));
        return numA < numB;
    });
    return devs;
}

bool ObV4lGmslDevicePort::isContainedMetadataDevice(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    auto devs = queryRelatedDevices(portInfo);
    for(auto &dev: devs) {
        if(dev->cap.device_caps & V4L2_CAP_META_CAPTURE) {
            return true;
        }
    }
    return true;  // for test
}

void foreachProfileGmsl(std::vector<std::shared_ptr<V4lDeviceHandleGmsl>>                                              deviceHandles,
                        std::function<bool(std::shared_ptr<V4lDeviceHandleGmsl>, std::shared_ptr<VideoStreamProfile>)> func) {
    bool quit        = false;
    int  fdPnum      = 0;
    int  mStreamType = OB_STREAM_VIDEO;
    for(auto &devHandle: deviceHandles) {
        if(quit) {
            break;
        }
        // enum format
        v4l2_fmtdesc pixel_format = {};
        pixel_format.type         = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while(!quit && xioctlGmsl(devHandle->fd, VIDIOC_ENUM_FMT, &pixel_format) == 0) {

            v4l2_frmsizeenum frame_size = {};
            frame_size.pixel_format     = pixel_format.pixelformat;
            uint32_t fourcc             = (const utils::big_endian<int> &)pixel_format.pixelformat;

            LOG_DEBUG("-Recognized pixel-format {}, pixel_format.pixelformat: 0x{:0x}, fourcc:0x{:0x} ", (char *)pixel_format.description,
                      pixel_format.pixelformat, fourcc);

            if(v4lFourccMapGmsl.count(fourcc)) {
                fourcc = v4lFourccMapGmsl.at(fourcc);
            }

            LOG_DEBUG("---fourcc-222-:0x{:0x} ", fourcc);
            if(pixel_format.pixelformat == 0) {
                // unsupported format fourcc maybe in pixel_format.description
                std::string description = std::string((const char *)pixel_format.description);
                std::size_t index       = description.find('-');
                if(index != std::string::npos) {
                    description = description.substr(0, index);
                }
                // hex string to dec
                int descriptionFourcc = std::stoi(description, 0, 16);
                fourcc                = (const utils::big_endian<int> &)descriptionFourcc;
            }
            else {
                LOG_DEBUG("Recognized pixel-format {0}, pixel_format.pixelformat: {1}, fourcc:{2} ", (char *)pixel_format.description, pixel_format.pixelformat,
                          fourcc);
            }
            // enum format params
            while(!quit && xioctlGmsl(devHandle->fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0) {
                v4l2_frmivalenum frame_interval = {};
                frame_interval.pixel_format     = pixel_format.pixelformat;
                frame_interval.width            = frame_size.discrete.width;
                frame_interval.height           = frame_size.discrete.height;

#if 0  // filter remove 1920/960/320 resolution
                if(frame_interval.width==1920 || frame_interval.width==960 || frame_interval.width==320 ){
                    //LOG_DEBUG("--->fileter resolution 1920x1080, 1920x*...");
                    continue;
                }
#endif
                while(!quit && xioctlGmsl(devHandle->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0) {
                    if(frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if(frame_interval.discrete.numerator != 0) {
                            auto format = utils::uvcFourccToOBFormat(fourcc);
                            if(format != OB_FORMAT_UNKNOWN) {
                                auto width  = frame_size.discrete.width;
                                auto height = frame_size.discrete.height;
                                auto fps    = static_cast<float>(frame_interval.discrete.denominator) / static_cast<float>(frame_interval.discrete.numerator);
#if 1
                                if(fdPnum == 0) {
                                    mStreamType = OB_STREAM_DEPTH;
                                }
                                else if(fdPnum == 1) {
                                    mStreamType = OB_STREAM_COLOR;
                                }
                                else if(fdPnum == 2) {
                                    mStreamType = OB_STREAM_IR_LEFT;
                                }
                                else if(fdPnum == 3) {
                                    mStreamType = OB_STREAM_IR_RIGHT;
                                }
                                else {
                                    mStreamType = OB_STREAM_VIDEO;
                                }
#endif
                                // LOG_DEBUG("-devHandle->fd:{0}, -format:{1}, width:{2}, heigh:{3}, fps:{4}, mStreamType:{5}, fdPnum:{6}", devHandle->fd,
                                // format, width, height, fps, mStreamType, fdPnum );
                                // auto profile = std::make_shared<VideoStreamProfile>(OB_STREAM_VIDEO, format, width, height, fps);
                                auto profile = std::make_shared<VideoStreamProfile>(std::shared_ptr<LazySensor>(), mStreamType, format, width, height, fps);
                                if(fourcc != 0) {
                                    quit = func(devHandle, profile);
                                }
                            }
                        }
                    }
                    ++frame_interval.index;
                }
                ++frame_size.index;
            }
            ++pixel_format.index;
        }
        fdPnum++;
    }
}

uint32_t CIDFromOBPropertyIDGmsl(uint32_t id) {
    switch(id) {
    case OB_PROP_COLOR_BACKLIGHT_COMPENSATION_INT:
        return V4L2_CID_BACKLIGHT_COMPENSATION;
    case OB_PROP_COLOR_BRIGHTNESS_INT:
        return V4L2_CID_BRIGHTNESS;
    case OB_PROP_COLOR_CONTRAST_INT:
        return V4L2_CID_CONTRAST;
    case OB_PROP_COLOR_EXPOSURE_INT:
        return V4L2_CID_EXPOSURE_ABSOLUTE;
    case OB_PROP_COLOR_GAIN_INT:
        return V4L2_CID_GAIN;
    case OB_PROP_COLOR_GAMMA_INT:
        return V4L2_CID_GAMMA;
    case OB_PROP_COLOR_HUE_INT:
        return V4L2_CID_HUE;
    case OB_PROP_COLOR_SATURATION_INT:
        return V4L2_CID_SATURATION;
    case OB_PROP_COLOR_SHARPNESS_INT:
        return V4L2_CID_SHARPNESS;
    case OB_PROP_COLOR_WHITE_BALANCE_INT:
        return V4L2_CID_WHITE_BALANCE_TEMPERATURE;
    case OB_PROP_COLOR_AUTO_EXPOSURE_BOOL:
        return V4L2_CID_EXPOSURE_AUTO;  // Automatic gain/exposure control
    case OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL:
        return V4L2_CID_AUTO_WHITE_BALANCE;
    case OB_PROP_COLOR_POWER_LINE_FREQUENCY_INT:
        return V4L2_CID_POWER_LINE_FREQUENCY;
    case OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT:
        return V4L2_CID_EXPOSURE_AUTO_PRIORITY;
    case OB_PROP_COLOR_ROLL_INT:
        return V4L2_CID_ROTATE;
    case OB_PROP_COLOR_FOCUS_INT:
        return V4L2_CID_FOCUS_ABSOLUTE;
    default:
        std::stringstream ss;
        ss << "no v4l2 cid for option option" << id;
        LOG_ERROR(ss.str());
        return 0;
    }
}

std::string fourccToStringGmsl(uint32_t id) {
    uint32_t device_fourcc = id;
    char     fourcc_buff[sizeof(device_fourcc) + 1];
    std::memcpy(fourcc_buff, &device_fourcc, sizeof(device_fourcc));
    fourcc_buff[sizeof(device_fourcc)] = 0;
    return fourcc_buff;
}

ObV4lGmslDevicePort::ObV4lGmslDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo) : portInfo_(portInfo) {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort-");

    auto devs = queryRelatedDevices(portInfo_);
    if(devs.empty()) {
        throw camera_disconnected_exception("No v4l device found for port: " + portInfo_->infUrl);
    }

    std::shared_ptr<V4lDeviceHandleGmsl> devHandle = nullptr;
    auto                                 iter      = devs.begin();
    while(iter != devs.end()) {
        if(((*iter)->cap.device_caps & V4L2_CAP_META_CAPTURE) && devHandle != nullptr) {
            // LOG_DEBUG("-V4L2_CAP_META_CAPTURE-1-");
            devHandle->metadataInfo = *iter;
            devHandle->metadataFd   = -1;
            auto fd                 = open(devHandle->metadataInfo->name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(fd < 0) {
                LOG_ERROR("Failed to open metadata dev: {}", devHandle->metadataInfo->name);
                continue;
            }
            LOG_DEBUG("Opened metadata dev:{}, fd:{}", devHandle->metadataInfo->name, fd);
            devHandle->metadataFd = fd;
        }
        else {
            // LOG_DEBUG("-V4L2_CAP_CAPTURE-2-");
            devHandle       = std::make_shared<V4lDeviceHandleGmsl>();
            devHandle->info = *iter;
            devHandle->fd   = -1;
            // devHandle->info->name = "/dev/video1";  // for test use.
            int fd = open(devHandle->info->name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(fd < 0) {
                throw io_exception("Failed to open: " + devHandle->info->name);
            }
            devHandle->fd = fd;
            LOG_DEBUG("Opened video device:{}, fd:{}", devHandle->info->name, fd);
            deviceHandles_.push_back(devHandle);
        }
        iter++;
    }
    if(deviceHandles_.empty()) {
        throw camera_disconnected_exception("No v4l device found for port: " + portInfo_->infUrl);
    }

    LOG_DEBUG("V4L device port created for {} with {} v4l2 device", portInfo_->infUrl, deviceHandles_.size());
}

ObV4lGmslDevicePort::~ObV4lGmslDevicePort() noexcept {
    LOG_DEBUG("Entry ~ObV4lGmslDevicePort");
    try {
        stopAllStream();
    }
    catch(const std::exception &ex) {
        LOG_ERROR(ex.what());
    }
    LOG_DEBUG("~ObV4lGmslDevicePort() deviceHandles_.size:{}", deviceHandles_.size());
    for(auto &devHandle: deviceHandles_) {
        if(devHandle->fd >= 0) {
            close(devHandle->fd);
        }
        if(devHandle->metadataFd >= 0) {
            close(devHandle->metadataFd);
        }
        if(devHandle->stopPipeFd[0] >= 0) {
            close(devHandle->stopPipeFd[0]);
        }
        if(devHandle->stopPipeFd[1] >= 0) {
            close(devHandle->stopPipeFd[1]);
        }
        // other resources are created on startStream and should be cleaned up on stopStream/stopAllStream
    }
    LOG_DEBUG("Leave ~ObV4lGmslDevicePort");
}

//---------------------------------------------------------------------------------------------------------------------------------
// 裁剪深度图函数
void cropDepthImage(const uint8_t *src, uint8_t *dst, int srcWidth, int srcHeight, int cropRitght, int cropBottom) {
    assert(src != nullptr && dst != nullptr);
    int cropWidth  = srcWidth - cropRitght;
    int cropHeight = srcHeight - cropBottom;
    assert(srcWidth >= cropWidth && srcHeight >= cropHeight);

    // LOG_DEBUG("--->cropWidth:{}, cropHeight:{}, cropRitght:{}, cropBottom:{} \n", cropWidth, cropHeight, cropRitght, cropBottom);

    // 计算源图像和目标图像的每行像素数（以字节为单位）
    // const size_t srcBytesPerRow = srcWidth * sizeof(uint8_t);
    const size_t dstBytesPerRow = cropWidth * sizeof(uint8_t);

    // 遍历图像的每一行，并复制像素到目标缓冲区
    for(int y = 0; y < cropHeight; y++) {
        // 计算当前行的源和目标指针
        const uint8_t *srcRow = src + y * srcWidth;
        uint8_t       *dstRow = dst + y * cropWidth;

        // 复制当前行的像素（除了最右边的32列）
        // memcpy(dstRow, srcRow, dstBytesPerRow);
        std::memcpy(dstRow, srcRow, dstBytesPerRow);
    }
}

// 裁剪深度图函数
void cropDepthImage16(const uint16_t *src, uint16_t *dst, int srcWidth, int srcHeight, int cropRitght, int cropBottom) {
    assert(src != nullptr && dst != nullptr);
    int cropWidth  = srcWidth - cropRitght;
    int cropHeight = srcHeight - cropBottom;
    assert(srcWidth >= cropWidth && srcHeight >= cropHeight);

    // LOG_DEBUG("--->>>cropWidth:%d, cropHeight:%d, cropRitght:%d, cropBottom:%d\n", cropWidth, cropHeight, cropRitght, cropBottom);

    // 计算源图像和目标图像的每行像素数（以字节为单位）
    // const size_t srcBytesPerRow = srcWidth * sizeof(uint16_t);
    const size_t dstBytesPerRow = cropWidth * sizeof(uint16_t);

    // 遍历图像的每一行，并复制像素到目标缓冲区
    for(int y = 0; y < cropHeight; y++) {
        // 计算当前行的源和目标指针
        const uint16_t *srcRow = src + y * srcWidth;
        uint16_t       *dstRow = dst + y * cropWidth;

        // 复制当前行的像素（除了最右边的32列）
        // memcpy(dstRow, srcRow, dstBytesPerRow);
        std::memcpy(dstRow, srcRow, dstBytesPerRow);
    }
}

// 假设buf是一个指向数据的指针，size是数据的大小（以字节为单位）
void writeBufferToFile(const char *buf, std::size_t size, const std::string &filename) {
    std::ofstream file(filename, std::ios::out | std::ios::binary);  // 以二进制模式打开文件

    if(!file.is_open()) {
        // 如果文件打开失败，则抛出异常或处理错误
        throw std::runtime_error("无法打开文件: " + filename);
    }

    // 将buf中的数据写入文件
    file.write(buf, size);

    // 关闭文件
    file.close();
}
//---------------------------------------------------------------------------------------------------------------------------------

void ObV4lGmslDevicePort::captureLoop(std::shared_ptr<V4lDeviceHandleGmsl> devHandle) {
    int metadataBufferIndex = -1;
    int colorFrameNum       = 0;  // color drop 1~3 frame -> fix color green screen issue.
    try {
        LOG_DEBUG("-Entry-ObV4lGmslDevicePort::captureLoop");
        int max_fd = std::max({ devHandle->fd, devHandle->metadataFd, devHandle->stopPipeFd[0], devHandle->stopPipeFd[1] });

        if(devHandle->metadataFd >= 0) {
            v4l2_buffer buf = {};
            memset(&buf, 0, sizeof(buf));
            buf.type   = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
            buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            xioctlGmsl(devHandle->metadataFd, VIDIOC_QBUF, &buf);
        }

        if(devHandle->fd >= 0) {
            v4l2_buffer buf = {};
            memset(&buf, 0, sizeof(buf));
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            xioctlGmsl(devHandle->fd, VIDIOC_QBUF, &buf);
        }

        while(devHandle->isCapturing) {
            // LOG_DEBUG("-ObV4lGmslDevicePort::captureLoop-devHandle->fd:{} ", devHandle->fd );
            struct timeval remaining = { 0, 500000 };  // 500ms
            fd_set         fds{};
            FD_ZERO(&fds);
            if(devHandle->fd >= 0) {
                FD_SET(devHandle->fd, &fds);
            }
            if(devHandle->metadataFd >= 0) {
                FD_SET(devHandle->metadataFd, &fds);
            }
            if(devHandle->stopPipeFd[0] >= 0) {
                FD_SET(devHandle->stopPipeFd[0], &fds);
            }
            if(devHandle->stopPipeFd[1] >= 0) {
                FD_SET(devHandle->stopPipeFd[1], &fds);
            }
            int val = select(max_fd + 1, &fds, nullptr, nullptr, &remaining);
            if(val < 0) {
                if(errno == EINTR) {
                    LOG_DEBUG("select interrupted: {}", strerror(errno));
                }
                else {
                    LOG_DEBUG("select failed: {}", strerror(errno));
                }
                continue;
            }
            else if(val == 0) {
                // 处理超时情况
                continue;
            }

            // LOG_DEBUG("-Entry -ObV4lGmslDevicePort::captureLoop-");
            if(FD_ISSET(devHandle->stopPipeFd[0], &fds) || FD_ISSET(devHandle->stopPipeFd[1], &fds)) {
                if(!devHandle->isCapturing) {
                    LOG_DEBUG("V4L stream is closed: {}", devHandle->info->name);
                }
                else {
                    LOG_ERROR("Stop pipe was signalled during streaming: {}", devHandle->info->name);
                }
                break;
            }

            // LOG_DEBUG("-metadata-ObV4lGmslDevicePort::captureLoop-");
            if(devHandle->metadataFd >= 0 && FD_ISSET(devHandle->metadataFd, &fds)) {
                FD_CLR(devHandle->metadataFd, &fds);
                v4l2_buffer buf = { 0 };
                memset(&buf, 0, sizeof(buf));
                buf.type   = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
                buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                // LOG_DEBUG("-metadata-ObV4lGmslDevicePort::captureLoop-");
                if(xioctlGmsl(devHandle->metadataFd, VIDIOC_DQBUF, &buf) < 0) {
                    LOG_DEBUG("VIDIOC_DQBUF failed, {}, {}", strerror(errno), devHandle->metadataInfo->name);
                }
                // LOG_DEBUG("---ObV4lGmslDevicePort::captureLoop-metadata--buf.bytesused:{}, buf.index:{}, buf.length:{} ", buf.bytesused, buf.index,
                // buf.length);

#if 0
                for(int i=0; i< buf.length; i++){
                    printf("[%s][%d]:rgb-md frame.data[%d]:%x  \n", __FUNCTION__, __LINE__, i, devHandle->metadataBuffers[buf.index].ptr[i]  );
                }
#endif

                // if( (buf.bytesused) && (buf.flags&V4L2_BUF_FLAG_DONE) )
                if((buf.bytesused) && (!(buf.flags & V4L2_BUF_FLAG_ERROR)))
                // if(buf.bytesused)
                {
                    devHandle->metadataBuffers[buf.index].actual_length = buf.bytesused;
                    devHandle->metadataBuffers[buf.index].sequence      = buf.sequence;
                    metadataBufferIndex                                 = buf.index;
                    // LOG_DEBUG("captureLoop-metadata devname:{}, buf.sequence:{}, buf.bytesused:{}, buf.index:{}", devHandle->info->name, buf.sequence,
                    // buf.bytesused, buf.index);
                }

                // LOG_DEBUG("-metadata-ObV4lGmslDevicePort::captureLoop-");
                if(devHandle->isCapturing) {
                    // LOG_DEBUG("-metadata-ObV4lGmslDevicePort::captureLoop-");
                    if(xioctlGmsl(devHandle->metadataFd, VIDIOC_QBUF, &buf) < 0) {
                        LOG_ERROR("devHandle->metadataFd VIDIOC_QBUF, errno: {0}, {1}, {3}", strerror(errno), errno, __LINE__);
                    }
                }
            }

            // LOG_DEBUG("-ObV4lGmslDevicePort::captureLoop-");
            if(FD_ISSET(devHandle->fd, &fds)) {
                FD_CLR(devHandle->fd, &fds);
                v4l2_buffer buf = {};
                memset(&buf, 0, sizeof(buf));
                buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                // reader buffer
                if(xioctlGmsl(devHandle->fd, VIDIOC_DQBUF, &buf) < 0) {
                    LOG_DEBUG("VIDIOC_DQBUF failed, {}, {}", strerror(errno), devHandle->metadataInfo->name);
                }

                // LOG_DEBUG("captureLoop-buf.index:{}, buf.sequence:{}, buf.length:{}, buf.flags:{} ", buf.index, buf.sequence, buf.length, buf.flags);
                // if( (buf.bytesused) && (buf.flags&V4L2_BUF_FLAG_DONE) )
                if((buf.bytesused) && (!(buf.flags & V4L2_BUF_FLAG_ERROR))) {
                    auto rawframe   = FrameFactory::createFrameFromStreamProfile(devHandle->profile);
                    auto videoFrame = rawframe->as<VideoFrame>();
                    if((DetectPlatform() == Platform::Xavier) || (DetectPlatform() == Platform::Orin)) {
                        handleSpecialResolution(devHandle, devHandle->buffers[buf.index].ptr, buf.bytesused, videoFrame);
                    }
                    else {
                        videoFrame->updateData(devHandle->buffers[buf.index].ptr, buf.bytesused);
                    }

                    if(metadataBufferIndex >= 0 && devHandle->metadataBuffers[metadataBufferIndex].sequence == buf.sequence) {
                        auto &metaBuf = devHandle->metadataBuffers[metadataBufferIndex];
                        // LOG_DEBUG("---ObV4lGmslDevicePort::captureLoop-metadata--buf.index:{}, buf.length:{} ", metaBuf.sequence, metaBuf.actual_length);
                        auto uvc_payload_header     = metaBuf.ptr;
                        auto uvc_payload_header_len = metaBuf.actual_length;
                        videoFrame->updateMetadata(static_cast<const uint8_t *>(uvc_payload_header), 12);
                        videoFrame->appendMetadata(static_cast<const uint8_t *>(uvc_payload_header), uvc_payload_header_len);

                        if(uvc_payload_header_len >= sizeof(StandardUvcFramePayloadHeader)) {
                            auto payloadHeader = (StandardUvcFramePayloadHeader *)uvc_payload_header;
                            videoFrame->setTimeStampUsec(payloadHeader->dwPresentationTime);
                        }
                    }

                    auto realtime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    videoFrame->setSystemTimeStampUsec(realtime);
                    videoFrame->setNumber(buf.sequence);

                    if(devHandle->profile->getType() == OB_STREAM_COLOR) {
                        if(colorFrameNum >= 3) {
                            // LOG_DEBUG("captureLoop-videoFrame.frameSize:{}", videoFrame->getDataSize());
                            devHandle->frameCallback(videoFrame);
                        }
                        else {
                            colorFrameNum++;
                            LOG_DEBUG("captureLoop colorFrameNum<3 drop. colorFrameNum:{}", colorFrameNum);
                        }
                    }
                    else {
                        // LOG_DEBUG("captureLoop-videoFrame.frameSize:{}", videoFrame->getDataSize());
                        devHandle->frameCallback(videoFrame);
                    }
                }

                if(devHandle->isCapturing) {
                    // LOG_DEBUG("-Entry -ObV4lGmslDevicePort::captureLoop-");
                    // xioctlGmsl(devHandle->fd, VIDIOC_QBUF, &buf);
                    if(xioctlGmsl(devHandle->fd, VIDIOC_QBUF, &buf) < 0) {
                        LOG_ERROR(" VIDIOC_QBUF, strerrno:{}, errno:{}", strerror(errno), errno);
                    }
                }
            }
        }
    }
    catch(const std::exception &ex) {
        LOG_ERROR(ex.what());
    }
}

void ObV4lGmslDevicePort::handleSpecialResolution(std::shared_ptr<V4lDeviceHandleGmsl> devHandle, const uint8_t *srcData, uint32_t srcSize,
                                                  std::shared_ptr<VideoFrame> videoFrame) {
    // LOG_DEBUG("-Entry handleSpecialResolution");
    VALIDATE_NOT_NULL(devHandle);
    //---------------------------------------------------------------------------------------------------------------------------------
    // LOG_DEBUG("-SpecialResolution profile->width:{}, profile->height:{}, profile->streamType:{}, videoFrame.frameSize:{}", devHandle->profile->width ,
    // devHandle->profile->height, devHandle->profile->streamType, videoFrame.frameSize);

#if 0
    //save raw video to file for test
    try {
        std::string filename="output/saveraw_"+ std::to_string(buf.sequence) +".raw";
        writeBufferToFile((char*)videoFrame.frameData, videoFrame.frameSize, filename);
        std::cout << "数据已成功写入文件output.raw" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
    }
#endif

    // auto start = std::chrono::high_resolution_clock::now();
    // handle 848x480; 848x100; 424x240, 480x270 resolution
    auto width      = devHandle->profile->getWidth();
    auto height     = devHandle->profile->getHeight();
    auto streamType = devHandle->profile->getType();
    if(((width % 424) == 0) || ((width == 480) && (height == 270))) {
        int originalWidth  = width;
        int originalHeight = height;
        int paddedWidth    = 0;
        int paddedHeight   = 0;
        int trimRight      = 0;
        int trimBottom     = 0;

        if((streamType == OB_STREAM_IR_LEFT) || (streamType == OB_STREAM_IR_RIGHT) || (streamType == OB_STREAM_IR))  // IR
        {
            if(originalWidth == 848) {
                paddedWidth = (originalWidth + 48);
            }
            else if(originalWidth == 424) {
                paddedWidth = (originalWidth + 24);
            }
            else if((originalWidth == 480) && (originalHeight == 270)) {
                paddedWidth = (originalWidth + 32);
            }

            paddedHeight = (originalHeight + 0);

            // 计算填充的像素数量
            trimRight  = paddedWidth - originalWidth;
            trimBottom = paddedHeight - originalHeight;
            // LOG_DEBUG("-SpecialResolution paddedWidth:{}, paddedHeight:{}, trimRight:{}, trimBottom:{}", paddedWidth, paddedHeight, trimRight, trimBottom);

            // std::vector<uint8_t> croppedImage(originalWidth*originalHeight);
            try {
                // cropDepthImage((uint8_t *)frameInfo.data, &croppedImage[0], paddedWidth, paddedHeight, trimRight, trimBottom);
                cropDepthImage(srcData, videoFrame->getDataMutable(), paddedWidth, paddedHeight, trimRight, trimBottom);
                videoFrame->setDataSize(originalWidth * originalHeight);
            }
            catch(const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
        else if((streamType == OB_STREAM_DEPTH || streamType == OB_STREAM_COLOR) && height != 270)  // Depth OR Color
        {
            if(originalWidth == 848) {
                paddedWidth = (originalWidth + 16);
            }
            else if(originalWidth == 424) {
                paddedWidth = (originalWidth + 24);
            }
            paddedHeight = (originalHeight + 0);

            // 计算填充的像素数量
            trimRight  = paddedWidth - originalWidth;
            trimBottom = paddedHeight - originalHeight;
            // LOG_DEBUG("-SpecialResolution paddedWidth:{}, paddedHeight:{}, trimRight:{}, trimBottom:{}", paddedWidth, paddedHeight, trimRight, trimBottom);

#if 1
            // std::vector<uint8_t> croppedImage(originalWidth*originalHeight);
            try {
                // cropDepthImage((uint8_t *)frameInfo.data, &croppedImage[0], paddedWidth, paddedHeight, trimRight, trimBottom);
                cropDepthImage16(reinterpret_cast<const uint16_t *>(srcData), reinterpret_cast<uint16_t *>(videoFrame->getDataMutable()), paddedWidth,
                                 paddedHeight, trimRight, trimBottom);
                videoFrame->setDataSize(originalWidth * originalHeight * 2);
            }
            catch(const std::exception &e) {
                // 处理异常
                std::cerr << "Error: " << e.what() << std::endl;
            }
#endif
        }
    }
    else {
        // LOG_DEBUG("-SpecialResolution profile->width:{}, profile->height:{}, profile->streamType:{}", devHandle->profile->getWidth(),
        //           devHandle->profile->getHeight(), devHandle->profile->getType());
        videoFrame->updateData(srcData, srcSize);
    }

    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    // LOG_DEBUG("--->Fucntion took {} millisecounds to execute. ", duration.count() );

#if 0
    //save process video to file
    try {
        std::string filename="output/saveraw_"+ std::to_string(buf.sequence) +".raw";
        writeBufferToFile((char*)videoFrame.frameData, videoFrame.frameSize, filename);
        std::cout << "数据已成功写入文件output.raw" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
    }
#endif
    //---------------------------------------------------------------------------------------------------------------------------------
}

StreamProfileList ObV4lGmslDevicePort::getStreamProfileList() {
    StreamProfileList profileList;
    foreachProfileGmsl(deviceHandles_, [&profileList](std::shared_ptr<V4lDeviceHandleGmsl> devHandle, std::shared_ptr<VideoStreamProfile> profile) {
        utils::unusedVar(devHandle);
        profileList.push_back(profile);
        return false;  // false means continue
    });
    return profileList;
}

uint32_t phaseProfileFormatToFourccGmsl(std::shared_ptr<const VideoStreamProfile> profile) {
    int      formatFourcc    = 0;
    OBFormat format          = profile->getFormat();
    auto     foundFormatIter = std::find_if(v4lFourccMapGmsl.begin(), v4lFourccMapGmsl.end(),
                                            [&](const std::pair<uint32_t, uint32_t> &item) { return item.second == utils::obFormatToUvcFourcc(format); });
    if(foundFormatIter != v4lFourccMapGmsl.end()) {
        return (const utils::big_endian<int> &)(foundFormatIter->first);
    }

    formatFourcc = utils::obFormatToUvcFourcc(format);
    if(formatFourcc == 0) {
        LOG_ERROR("unsupported format {}", profile->getFormat());
        return 0;
    }

    return (const utils::big_endian<int> &)(formatFourcc);
}

void ObV4lGmslDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    std::shared_ptr<V4lDeviceHandleGmsl> devHandle    = nullptr;
    auto                                 videoProfile = profile->as<VideoStreamProfile>();
    foreachProfileGmsl(deviceHandles_, [&](std::shared_ptr<V4lDeviceHandleGmsl> handle, std::shared_ptr<VideoStreamProfile> prof) {
        if(prof->getType() == videoProfile->getType() && prof->getWidth() == videoProfile->getWidth() && prof->getHeight() == videoProfile->getHeight()
           && prof->getFps() == videoProfile->getFps() && prof->getFormat() == videoProfile->getFormat()) {
            devHandle = handle;
            return true;
        }
        return false;
    });
    if(!devHandle) {
        throw pal_exception("No v4l device found for profile: width=" + std::to_string(videoProfile->getWidth())
                            + ", height=" + std::to_string(videoProfile->getHeight()) + ", fps=" + std::to_string(videoProfile->getFps())
                            + ", format=" + std::to_string(videoProfile->getFormat()));
    }
    if(devHandle->isCapturing) {
        pal_exception("V4l device is already capturing");
    }

    if(devHandle->metadataFd >= 0) {
        v4l2_format fmt = {};
        fmt.type        = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
        if(xioctlGmsl(devHandle->metadataFd, VIDIOC_G_FMT, &fmt) < 0) {
            throw io_exception("Failed to get metadata format! " + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
        if(fmt.type != LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL) {
            throw io_exception("Invalid metadata type!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }

        const std::vector<uint32_t> requires_formats   = { LOCAL_V4L2_META_FMT_D4XX_GMSL, V4L2_META_FMT_UVC };
        bool                        set_format_success = false;

#if 0
        for(auto required_format: requires_formats) {
            memcpy(fmt.fmt.raw_data, &required_format, sizeof(required_format));
            if(xioctlGmsl(devHandle->metadataFd, VIDIOC_S_FMT, &fmt) >= 0) {
                LOG_DEBUG("Set metadata format to {}", fourccToStringGmsl(required_format));
                set_format_success = true;
                break;
            }
        }
#endif

#if 1
        fmt.fmt.pix.width       = 1482175047;
        fmt.fmt.pix.height      = 96;
        fmt.fmt.pix.pixelformat = 0;

        if(xioctlGmsl(devHandle->metadataFd, VIDIOC_S_FMT, &fmt) >= 0) {
            LOG_DEBUG("-metadata-Set metadata format to {}", fmt.fmt.pix.pixelformat);
            set_format_success = true;
        }
#endif

        if(!set_format_success) {
            throw io_exception("Failed to set metadata format!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }

        struct v4l2_requestbuffers req = { 0 };
        req.count                      = MAX_BUFFER_COUNT_GMSL;
        req.type                       = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
        req.memory                     = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
        if(xioctlGmsl(devHandle->metadataFd, VIDIOC_REQBUFS, &req) < 0) {
            throw io_exception("Failed to request metadata buffers!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
        for(uint32_t i = 0; i < req.count && i < MAX_BUFFER_COUNT_GMSL; i++) {
            struct v4l2_buffer buf = {};
            memset(&buf, 0, sizeof(buf));
            buf.type   = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
            buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            buf.index  = i;  // only one buffer
            if(xioctlGmsl(devHandle->metadataFd, VIDIOC_QUERYBUF, &buf) < 0) {
                throw io_exception("Failed to query metadata buffer!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }

            if(USE_MEMORY_MMAP) {
                devHandle->metadataBuffers[i].ptr = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, devHandle->metadataFd, buf.m.offset);
                devHandle->metadataBuffers[i].length = buf.length;
                if(devHandle->metadataBuffers[i].ptr == MAP_FAILED) {
                    LOG_ERROR(" mmap, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
                }
            }
            else {
                uint8_t  md_extra         = (LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL == buf.type) ? MAX_META_DATA_SIZE : 0;
                uint32_t _length          = buf.length + md_extra;
                devHandle->buffers[i].ptr = static_cast<uint8_t *>(malloc(_length));
                ;
                devHandle->buffers[i].length = _length;
                if(!devHandle->buffers[i].ptr) {
                    LOG_ERROR(" User_p allocation failed!, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
                }
                memset(devHandle->buffers[i].ptr, 0, _length);
            }

            if(xioctlGmsl(devHandle->metadataFd, VIDIOC_QBUF, &buf) < 0) {
                LOG_ERROR(" VIDIOC_QBUF, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
            }
        }

        LOG_DEBUG("-metadata-VIDIOC_STREAMON---");
        v4l2_buf_type bufType = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
        if(xioctlGmsl(devHandle->metadataFd, VIDIOC_STREAMON, &bufType) < 0) {
            throw io_exception("Failed to stream on metadata!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
    }

    if(devHandle->fd >= 0) {
        v4l2_format fmt    = {};
        fmt.type           = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width  = videoProfile->getWidth();
        fmt.fmt.pix.height = videoProfile->getHeight();
        // fmt.fmt.pix.field       = V4L2_FIELD_NONE;
        fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
        fmt.fmt.pix.pixelformat = phaseProfileFormatToFourccGmsl(videoProfile);

        if(xioctlGmsl(devHandle->fd, VIDIOC_S_FMT, &fmt) < 0) {
            throw io_exception("Failed to set format!" + devHandle->info->name + ", " + strerror(errno));
        }
        if(xioctlGmsl(devHandle->fd, VIDIOC_G_FMT, &fmt) < 0) {
            throw io_exception("Failed to get format!" + devHandle->info->name + ", " + strerror(errno));
        }
        LOG_DEBUG("Video node was successfully configured to {0} format, fd {1}, name: {2}", fourccToStringGmsl(fmt.fmt.pix.pixelformat), devHandle->fd,
                  devHandle->info->name);

#if 0
        struct v4l2_streamparm streamparm={};
        memset(&streamparm, 0x00, sizeof(struct v4l2_streamparm));
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctlGmsl(devHandle->fd, VIDIOC_G_PARM, &streamparm) < 0) {
            LOG_ERROR(" VIDIOC_G_PARM, errno:{}({}), line:{}", strerror(errno), errno, __LINE__);
        }

        LOG_DEBUG("Camera ouput format: ( {} x {} )  stride: {}, imagesize: {}, frate: {} / {}", fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.bytesperline,
                fmt.fmt.pix.sizeimage, streamparm.parm.capture.timeperframe.denominator, streamparm.parm.capture.timeperframe.numerator);
#endif

#if 1
        v4l2_streamparm streamparm                       = {};
        streamparm.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        streamparm.parm.capture.timeperframe.numerator   = 1;
        streamparm.parm.capture.timeperframe.denominator = videoProfile->getFps();
        if(xioctlGmsl(devHandle->fd, VIDIOC_S_PARM, &streamparm) < 0) {
            throw io_exception("Failed to set streamparm!" + devHandle->info->name + ", " + strerror(errno));
        }
        if(xioctlGmsl(devHandle->fd, VIDIOC_G_PARM, &streamparm) < 0) {
            throw io_exception("Failed to get streamparm!" + devHandle->info->name + ", " + strerror(errno));
        }
#endif

        struct v4l2_requestbuffers req = {};
        req.count                      = MAX_BUFFER_COUNT_GMSL;
        req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory                     = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
        if(xioctlGmsl(devHandle->fd, VIDIOC_REQBUFS, &req) < 0) {
            throw io_exception("Failed to request buffers!" + devHandle->info->name + ", " + strerror(errno));
        }
        for(uint32_t i = 0; i < req.count && i < MAX_BUFFER_COUNT_GMSL; i++) {
            struct v4l2_buffer buf = {};
            memset(&buf, 0, sizeof(buf));
            buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            buf.index  = i;
            if(xioctlGmsl(devHandle->fd, VIDIOC_QUERYBUF, &buf) < 0) {
                throw io_exception("Failed to query buffer!" + devHandle->info->name + ", " + strerror(errno));
            }

            if(USE_MEMORY_MMAP) {
                devHandle->buffers[i].ptr    = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, devHandle->fd, buf.m.offset);
                devHandle->buffers[i].length = buf.length;
                if(devHandle->buffers[i].ptr == MAP_FAILED) {
                    LOG_ERROR(" mmap, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
                }
            }
            else {
                uint8_t  md_extra         = (V4L2_BUF_TYPE_VIDEO_CAPTURE == buf.type) ? MAX_META_DATA_SIZE : 0;
                uint32_t _length          = buf.length + md_extra;
                devHandle->buffers[i].ptr = static_cast<uint8_t *>(malloc(_length));
                ;
                devHandle->buffers[i].length = _length;

                if(!devHandle->buffers[i].ptr) {
                    LOG_ERROR(" User_p allocation failed!, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
                }
                memset(devHandle->buffers[i].ptr, 0, _length);

                buf.m.userptr = reinterpret_cast<unsigned long>(devHandle->buffers[i].ptr);
            }

            if(xioctlGmsl(devHandle->fd, VIDIOC_QBUF, &buf) < 0) {
                LOG_ERROR(" VIDIOC_QBUF, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
            }
        }

        devHandle->profile = videoProfile;  // fix stoptream no streamoff issue. when streamon exception.

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        v4l2_buf_type                bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        if(xioctlGmsl(devHandle->fd, VIDIOC_STREAMON, &bufType) < 0) {
            throw io_exception("Failed to stream on!" + devHandle->info->name + ", " + strerror(errno));
        }
        lk.unlock();
        LOG_DEBUG("-VIDIOC_STREAMON success-");
    }

    if(pipe(devHandle->stopPipeFd) < 0) {
        throw io_exception("Failed to create stop pipe!" + devHandle->info->name + ", " + strerror(errno));
    }

    devHandle->isCapturing   = true;
    devHandle->profile       = videoProfile;
    devHandle->frameCallback = callback;
    devHandle->captureThread = std::make_shared<std::thread>([this, devHandle]() { captureLoop(devHandle); });

    LOG_DEBUG("-Leave startStream-");
}

void ObV4lGmslDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    LOG_INFO("-Entry stopStream-");
    if(deviceHandles_.empty()) {
        LOG_DEBUG("-deviceHandles_.empty-");
        return;
    }

    auto clearUp = [](std::shared_ptr<V4lDeviceHandleGmsl> devHandle) {
        // cleanup
        for(uint32_t i = 0; i < MAX_BUFFER_COUNT_GMSL; i++) {
            if(devHandle->buffers[i].ptr) {
                if(USE_MEMORY_MMAP) {
                    munmap(devHandle->buffers[i].ptr, devHandle->buffers[i].length);
                    devHandle->buffers[i].ptr    = nullptr;
                    devHandle->buffers[i].length = 0;
                    // LOG_DEBUG("-munmap-video devHandle->buffers[{}]", i);
                }
                else {
                    free(devHandle->buffers[i].ptr);
                    devHandle->buffers[i].ptr    = nullptr;
                    devHandle->buffers[i].length = 0;
                }
            }
            if(devHandle->metadataBuffers[i].ptr) {
                if(USE_MEMORY_MMAP) {
                    munmap(devHandle->metadataBuffers[i].ptr, devHandle->metadataBuffers[i].length);
                    devHandle->metadataBuffers[i].ptr    = nullptr;
                    devHandle->metadataBuffers[i].length = 0;
                    // LOG_DEBUG("-munmap-metadata devHandle->metadataBuffers[{}]", i);
                }
                else {
                    free(devHandle->metadataBuffers[i].ptr);
                    devHandle->metadataBuffers[i].ptr    = nullptr;
                    devHandle->metadataBuffers[i].length = 0;
                }
            }
        }
        if(devHandle->stopPipeFd[0] >= 0) {
            close(devHandle->stopPipeFd[0]);
            devHandle->stopPipeFd[0] = -1;
        }

        if(devHandle->stopPipeFd[1] >= 0) {
            close(devHandle->stopPipeFd[1]);
            devHandle->stopPipeFd[1] = -1;
        }
    };

    auto videoProfile = profile->as<VideoStreamProfile>();
    for(auto &devHandle: deviceHandles_) {

#if 1
        if(!devHandle->profile || !(*devHandle->profile == *videoProfile)) {
            LOG_DEBUG("-deviceHandles devHandle->profile:{}, profile:{}, continue...", devHandle->profile, profile);
            continue;
        }
        LOG_DEBUG("-deviceHandles- devHandle->isCapturing:{}, devHandle->profile:{}, profile:{}", devHandle->isCapturing, devHandle->profile, profile);
#endif

#if 0
        if(!devHandle->profile || !(*devHandle->profile == *profile) || !devHandle->isCapturing) {
            LOG_DEBUG("-deviceHandles- devHandle->isCapturing:{}, devHandle->profile:{}, profile:{}",devHandle->isCapturing, devHandle->profile, profile);
            continue;
        }
#endif

        devHandle->isCapturing = false;
        // signal the capture loop to stop
        if(devHandle->stopPipeFd[1] >= 0) {
            char buff[1] = { 0 };
            auto ret     = write(devHandle->stopPipeFd[1], buff, 1);
            if(ret < 0) {
                LOG_ERROR("Failed to write to stop pipe, errnoStr:{}, errno:{}, line:{} ", strerror(errno), errno, __LINE__);
            }
        }

        // wait for the capture loop to stop
        if(devHandle->captureThread && devHandle->captureThread->joinable()) {
            devHandle->captureThread->join();
        }
        if(devHandle->captureThread)
            devHandle->captureThread.reset();

        v4l2_buf_type                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        if(xioctlGmsl(devHandle->fd, VIDIOC_STREAMOFF, &type) < 0) {
            throw io_exception("Failed to stream off!" + devHandle->info->name + ", " + strerror(errno));
        }
        lk.unlock();
        LOG_DEBUG("-VIDIOC_STREAMOFF success-");

        clearUp(devHandle);

        struct v4l2_requestbuffers req = {};
        req.count                      = 0;
        req.type                       = type;
        req.memory                     = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
        if(xioctlGmsl(devHandle->fd, VIDIOC_REQBUFS, &req) < 0) {
            throw io_exception("Failed to request buffers!" + devHandle->info->name + ", " + strerror(errno));
        }

        if(devHandle->metadataFd >= 0) {
            type = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
            if(xioctlGmsl(devHandle->metadataFd, VIDIOC_STREAMOFF, &type) < 0) {
                throw io_exception("Failed to stream off metadata!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }
            LOG_DEBUG("-VIDIOC_STREAMOFF metadata success-");

            req.count  = 0;
            req.type   = type;
            req.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
            if(xioctlGmsl(devHandle->metadataFd, VIDIOC_REQBUFS, &req) < 0) {
                throw io_exception("Failed to request metadata buffers!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }
        }
    }
    LOG_INFO("-Leave stopStream-");
}

void ObV4lGmslDevicePort::stopAllStream() {
    LOG_DEBUG("-Entry stopAllStream-");
    for(auto &devHandle: deviceHandles_) {
        if(devHandle->isCapturing) {
            stopStream(devHandle->profile);
        }
    }
    LOG_DEBUG("-Leave stopAllStream-");
}

std::shared_ptr<const SourcePortInfo> ObV4lGmslDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

//--------------------------------------------------------------------------------------
#if 0
bool ObV4lGmslDevicePort::sendData(const uint8_t *data, const uint32_t dataLen) {

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::sendData-ctrl:{} ", dataLen);

    uint16_t opcode, nId, halfWordSize, magic;
    uint32_t propertyId = 0;
    uint8_t  mData0 = 0, mData1 = 0, mData2 = 0, mData3 = 0;
    uint8_t  mHeaderLen   = dataLen - 4;
    uint32_t alignDataLen = 0, alignI2CDataLen = 0;
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::mHeaderLen:{} ", mHeaderLen);

#if 0
    uint32_t ctrl         = OB_VENDOR_XU_CTRL_ID_64;
    auto    alignDataLen = dataLen;
    if(alignDataLen <= 64) {
        ctrl         = OB_VENDOR_XU_CTRL_ID_64;
        alignDataLen = 64;
    }
    else if(alignDataLen > 512) {
        ctrl         = OB_VENDOR_XU_CTRL_ID_1024;
        alignDataLen = 1024;
    }
    else {
        ctrl         = OB_VENDOR_XU_CTRL_ID_512;
        alignDataLen = 512;
    }
#endif

    if(data != NULL) {
        opcode       = ((ProtocolHeader *)(data))->opcode;
        nId           = ((ProtocolHeader *)(data))->nId;
        halfWordSize = ((ProtocolHeader *)(data))->halfWordSize;
        magic        = ((ProtocolHeader *)(data))->magic;
        LOG_DEBUG("--->>> opcode:{}, nId:{}, halfWordSize:{}, magic:0x{:0x} ", opcode, nId, halfWordSize, magic);

        uint8_t *pDataBuf = ((uint8_t *)(data)) + HP_HEADER_SIZE;
        propertyId       = *(uint32_t *)pDataBuf;
        LOG_DEBUG("--->>> propertyId: {} ", propertyId);

        mData0 = propertyId & 0x000000FF;
        mData1 = (propertyId >> 8) & 0x000000FF;
        mData2 = (propertyId >> 16) & 0x000000FF;
        mData3 = (propertyId >> 24) & 0x000000FF;
        LOG_DEBUG("--->>> mData0:0x{:0x}, mData1:0x{:0x}, mData2:{:0x}, mData3:{:0x} ", mData0, mData1, mData2, mData3);

        // if(propertyId==1000)
        {
            LOG_DEBUG("-ObV4lGmslDevicePort-mHeaderLen:{}", mHeaderLen);
            i2c_msg_t get_version_cmd;
            memset( &get_version_cmd, 0, sizeof(i2c_msg_t) );
            get_version_cmd.header.len   = mHeaderLen;  // G2R_GET_VERSION_CMD_LEN;
            get_version_cmd.header.code  = opcode;     // G2R_GET_VERSION_CMD_CODE;
            get_version_cmd.header.index = nId;         // inde++;
            get_version_cmd._data[0]     = mData0;      // 0xe8;
            get_version_cmd._data[1]     = mData1;      // 0x03;
            get_version_cmd._data[2]     = mData2;
            get_version_cmd._data[3]     = mData3;

            alignDataLen = mHeaderLen; //OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;  // 252; //172;  GMSL I2C read 252 bytes/per
            return setXuExt(G2R_CAMERA_CID_SET_DATA, (uint8_t *)(&get_version_cmd), alignDataLen);
        }
    }

    LOG_ERROR("-Entry ObV4lGmslDevicePort::data is nullprt!!! ");
    return false;  // setXuExt(ctrl, data, alignDataLen);
}
#endif

uint32_t ObV4lGmslDevicePort::sendAndReceive(const uint8_t *send, uint32_t sendLen, uint8_t *recv, uint32_t exceptedRecvLen) {
    std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
    uint16_t                     opcode = ((ProtocolHeader *)(send))->opcode;
    // uint16_t       reqId      = ((ProtocolHeader *)(send))->nId;
    const uint8_t *dataBuf    = send + sizeof(ProtocolHeader);
    uint32_t       propertyId = *reinterpret_cast<const uint32_t *>(dataBuf);

    if(!sendData(send, sendLen)) {
        return -1;
    }
    utils::sleepUs(300);
    // accroding to the opcode and propertyId to add more wait time
    if((propertyId == OB_PROP_LDP_BOOL) || (propertyId == OB_PROP_DISPARITY_TO_DEPTH_BOOL) || (propertyId == OB_PROP_DISP_SEARCH_RANGE_MODE_INT)) {
        auto delayTime = 10;  // 10ms
        utils::sleepMs(delayTime);
    }
    if(propertyId == OB_PROP_LDP_MEASURE_DISTANCE_INT) {  // 100
        auto delayTime = 10;                              // 10ms
        utils::sleepMs(delayTime);
    }
    if((propertyId >= 1000) && (propertyId <= 1999)) {
        auto delayTime = 10;  // 10ms
        utils::sleepMs(delayTime);
    }
    if((propertyId >= 4000) && (propertyId <= 4999)) {
        auto delayTime = 10;  // 10ms
        utils::sleepMs(delayTime);
    }
    if((propertyId >= 4500) && (propertyId <= 4599)) {
        auto delayTime = 10;  // 10ms
        utils::sleepMs(delayTime);
    }
    if((propertyId == 1043) || (propertyId == 1038)) {
        auto delayTime = 20;  // 10ms
        utils::sleepMs(delayTime);
    }

    const int updateDelayMinTime  = 150;
    int       updateDelayBaseTime = updateDelayMinTime * 4;

    int delayTime = updateDelayMinTime;
    if((opcode == 0x0D) || (opcode == 0x0E) || (opcode == 0x0F)) {
        if((opcode == 0x0D) && (propertyId == 0x10000))  // cfg.bin & cfg2.bin & cfg_f.bin & cfg2_f.bin
        {
            delayTime = updateDelayBaseTime;
        }
        else if((opcode == 0x0D) && (propertyId == 0x20000))  // 131072 Gemini330_app_1.3.29.bin
        {
            delayTime = updateDelayBaseTime * 2;
        }
        else if((opcode == 0x0D) && (propertyId == 0x60000))  // 393216 //Gemini330_app_1.3.29.bin
        {
            delayTime = updateDelayBaseTime * 2 * 3;
        }
        else if((opcode == 0x0D) && (propertyId == 0x500000))  // ORBBEC_ISP_20240730.bin
        {
            delayTime = updateDelayBaseTime * 2 * 3;
        }
        else if((opcode == 0x0D) && (propertyId == 0xA4000))  // TMF8801_FW_3.0.22.bin
        {
            delayTime = updateDelayBaseTime * 2;
        }
        else if((opcode == 0x0D))  // flash erase delay
        {
#if 1                                 // for debug test use. remove it when release
                                      // add delay  64K-200ms, 128K-400ms, 256K-800ms
            int uintDelayTime = 200;  // 200ms
            int delayNum      = (propertyId / 65536 /*64K*/) + ((propertyId % 65536) ? 1 : 0);
            int delayTime     = delayNum * uintDelayTime;
            LOG_DEBUG("eraseFlashFunc2 delayTime {}. after, I2C update write flash", delayTime);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayTime));
#endif
        }
        else if((opcode == 0x0E))  // writeFlash
        {
            delayTime = updateDelayMinTime - 100;
        }
        else if((opcode == 0x0F)) {
            delayTime = updateDelayMinTime * 2;
        }
        else {
            delayTime = 10;
        }
        utils::sleepMs(delayTime);
    }
    // opcode 25 handle update read vertyfy
    if(opcode == 25) {
        auto delayTime = 10;
        utils::sleepMs(delayTime);
    }

    if(!recvData(recv, &exceptedRecvLen)) {

        return -1;
    }
    return exceptedRecvLen;
}

/**
 * I2C  opcode=30
 * data: propid<4bytes>+offset<4bytes>+read_len<4bytes>
 *
 */
bool ObV4lGmslDevicePort::sendData(const uint8_t *data, const uint32_t dataLen) {
    uint16_t opcode, nId = 0, halfWordSize = 0, magic = 0;
    uint32_t propertyId = 0, alignDataLen = 0, alignI2CDataLen = 0, ctrl = 0;
    uint8_t  mI2cPackDataLen = 0, mI2cPackLen = 0;
    bool     ret = false;

    LOG_DEBUG("-Entry sendData-dataLen:{} ", dataLen);
    if(alignDataLen >= OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX) {
        alignDataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else {
        alignDataLen = dataLen;
    }

    VALIDATE_NOT_NULL(data);
    {
        opcode       = ((ProtocolHeader *)(data))->opcode;
        nId          = ((ProtocolHeader *)(data))->nId;
        halfWordSize = ((ProtocolHeader *)(data))->halfWordSize;
        magic        = ((ProtocolHeader *)(data))->magic;
        LOG_DEBUG("------------------------------------------------------------------------");

        uint8_t *pDataBuf = ((uint8_t *)(data)) + sizeof(ProtocolHeader);
        propertyId        = *(uint32_t *)pDataBuf;
        LOG_DEBUG("sendData opcode:{}, nId:{}, halfWordSize:{}, magic:0x{:0x}, PropertyId:{}", opcode, nId, halfWordSize, magic, propertyId);

#if 0
        if(opcode==13 || opcode==14 ||opcode==18 ||opcode==25 ||opcode==30) //with offset
        {
            uint32_t mOffset = *(uint32_t *)(pDataBuf + 4);
            LOG_DEBUG("sendData mOffset:{} ", mOffset);

            uint32_t mSize = *(uint32_t *)(pDataBuf + 8);
            LOG_DEBUG("sendData mSize:{} ", mSize);
        }
#endif

        mI2cPackDataLen = dataLen - sizeof(ProtocolHeader);  //- sizeof(uint16_t);
        mI2cPackLen     = mI2cPackDataLen + sizeof(i2c_msg_header_t);
        LOG_DEBUG("sendData mI2cPackDataLen:{}, alignDataLen:{}, mI2cPackLen:{} ", mI2cPackDataLen, alignDataLen, mI2cPackLen);

#if 0
        mData0 = propertyId & 0x000000FF;
        mData1 = (propertyId >> 8) & 0x000000FF;
        mData2 = (propertyId >> 16) & 0x000000FF;
        mData3 = (propertyId >> 24) & 0x000000FF;
        LOG_DEBUG("sendData 1data-PropertyId mData0:{}, mData1:{}, mData2:{}, mData3:{} ", mData0, mData1, mData2, mData3);
        LOG_DEBUG("sendData 2data-PropertyId data0:{}, data1:{}, data2:{}, data3:{} ", pDataBuf[0], pDataBuf[1], pDataBuf[2], pDataBuf[3]);
#endif

        // LOG_DEBUG("--------------------------------------------------------------------------");
        {
            alignI2CDataLen = alignDataLen - 2;  // cal i2c_msg_t len
            LOG_DEBUG("sendData alignI2CDataLen:{} ", alignI2CDataLen);
            i2c_msg_t send_i2c_pack_msg;
            memset(&send_i2c_pack_msg, 0, sizeof(i2c_msg_t));
            send_i2c_pack_msg.header.len   = alignI2CDataLen;  // G2R_GET_VERSION_CMD_LEN;
            send_i2c_pack_msg.header.code  = opcode;           // G2R_GET_VERSION_CMD_CODE;
            send_i2c_pack_msg.header.index = nId;              // inde++;
            // memcpy(send_i2c_pack_msg._data, pDataBuf, mI2cPackDataLen);
            std::memcpy(send_i2c_pack_msg._data, pDataBuf, mI2cPackDataLen);

            if((mI2cPackDataLen <= 2) && (mI2cPackDataLen > 0)) {
                propertyId = *(uint16_t *)send_i2c_pack_msg._data;
                LOG_DEBUG("sendData PropertyId:{} ", propertyId);
                // LOG_DEBUG("sendData 04data-PropertyId dat0:{}, data1:{} ", send_i2c_pack_msg._data[0], send_i2c_pack_msg._data[1]);
            }
            else if(mI2cPackDataLen >= 4) {
                propertyId = *(uint32_t *)send_i2c_pack_msg._data;
                LOG_DEBUG("sendData PropertyId:{} ", propertyId);
                // LOG_DEBUG("sendData 4data-PropertyId dat0:{}, data1:{}, data2:{}, data3:{} ", send_i2c_pack_msg._data[0], send_i2c_pack_msg._data[1],
                // send_i2c_pack_msg._data[2], send_i2c_pack_msg._data[3]);
            }

            if(propertyId == 202) {  // OB_PROP_DEVICE_REPOWER_BOOL
                int value = *(uint8_t *)(send_i2c_pack_msg._data + 4);
                LOG_DEBUG("sendData value:{} ", value);
                if(value == 1) {
                    LOG_DEBUG("sendData PropertyId:{} -not need read i2c response status. handle resetGmslDriver.", propertyId);
                    resetGmslDriver();
                    return true;
                }
                else if(value == 0) {
                    LOG_DEBUG("sendData PropertyId:{} -do not anything! -value:{} ", propertyId, value);
                    return true;
                }
                else {
                    LOG_ERROR("NOT support param!");
                }
            }
            LOG_DEBUG("--------------------------------------------------------------------------");

            // alignDataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;  // 252; //172;  GMSL I2C read 252 bytes/per
            ctrl = G2R_CAMERA_CID_SET_DATA;
            ret  = setXuExt(ctrl, (uint8_t *)(&send_i2c_pack_msg), alignI2CDataLen);
            // LOG_DEBUG("-Leave ObV4lGmslDevicePort::sendData ret:{} ", ret);
            return ret;
            // return setXuExt(ctrl, data, alignDataLen);
        }
    }

    LOG_DEBUG("-Leave ObV4lGmslDevicePort::sendData ret:{} ", ret);
    return ret;
}

bool ObV4lGmslDevicePort::recvData(uint8_t *data, uint32_t *dataLen) {

    LOG_DEBUG("-Entry recvData-dataLen:{0}", *dataLen);
    uint32_t ctrl = 0;  // OB_VENDOR_XU_CTRL_ID_512;

    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(dataLen);

    if(*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_16) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_16;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_16) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_32)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_32;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_32) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_64)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_64;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_64) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_128)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_128;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_128) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_192)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_192;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_192) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_256)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_256) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_512)) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_64;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else if(*dataLen > OB_GMSL_FW_I2C_DATA_LEN_512) {
        // ctrl = OB_VENDOR_XU_CTRL_ID_1024;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else {
        // ctrl = OB_VENDOR_XU_CTRL_ID_512;
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }

    LOG_DEBUG("-PuRaw send read data *dataLen:{}", *dataLen);
    // note fw read data len
    {
        //*dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
        // setPuRaw(G2R_CAMERA_CID_SET_DATA_LEN, G2R_GET_VERSION_DATA_LEN);
        *dataLen += 10;  // *dataLen(real data len) + packhead(8) +propid(2)
        LOG_DEBUG("-PuRaw send read data cal(*dataLen+10) real *dataLen:{}", *dataLen);
        setPuRaw(G2R_CAMERA_CID_SET_DATA_LEN, *dataLen);
        ctrl = G2R_CAMERA_CID_GET_DATA;
    }

    return getXuExt(ctrl, data, dataLen);
}

//--------------------------------------------------------------------------------------
static int currentPuIndex;
void       ObV4lGmslDevicePort::setPuDevIndex(int index) {
    LOG_DEBUG("-set current Pu index:{}", index);
    currentPuIndex = index;
}
int ObV4lGmslDevicePort::getPuDevIndex() {
    return currentPuIndex;
}

bool ObV4lGmslDevicePort::setPu(uint32_t propertyId, int32_t value) {
    auto fd = deviceHandles_[getPuDevIndex()]->fd;
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPu propertyId={}, value:{}, devNode:{}", propertyId, value, deviceHandles_[getPuDevIndex()]->info->name);
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPu propertyId={}, value:{}, devNode-front():{}", propertyId, value, deviceHandles_.front()->info->name);

#if 0
    int num=deviceHandles_.size();
    for(int i=0;i<num;i++) {
        LOG_DEBUG("getPu deviceHandles_[i]->info->name:{}", deviceHandles_[i]->info->name);
    }
#endif

    // auto  fd  = deviceHandles_.front()->fd;
    auto cid = CIDFromOBPropertyIDGmsl(propertyId);
    // LOG_DEBUG("Entry ObV4lGmslDevicePort::setPu propertyId={}, cid={}, value:{} ", propertyId, cid, value);

    struct v4l2_control control = { cid, value };
    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = value ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
    }

    // Set value
    std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
    if(xioctlGmsl(fd, VIDIOC_S_CTRL, &control) < 0) {
        LOG_ERROR("set {0} xioctlGmsl(VIDIOC_S_CTRL) failed, {1}", propertyId, strerror(errno));
        ObV4lGmslDevicePort::setPuDevIndex(0);
        lk.unlock();
        return false;
    }
    ObV4lGmslDevicePort::setPuDevIndex(0);
    lk.unlock();

    LOG_DEBUG("Leave ObV4lGmslDevicePort::setPu Success! propertyId={}, cid={}, value:{} ", propertyId, cid, value);
    return true;
}

bool ObV4lGmslDevicePort::getPu(uint32_t propertyId, int32_t &value) {
    auto fd = deviceHandles_[getPuDevIndex()]->fd;
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getPu propertyId={}, devNode:{}", propertyId, deviceHandles_[getPuDevIndex()]->info->name);

#if 0
    int num=deviceHandles_.size();
    for(int i=0;i<num;i++) {
        LOG_DEBUG("getPu deviceHandles_[i]->info->name:{}", deviceHandles_[i]->info->name);
    }
#endif

    // auto  fd  = deviceHandles_.front()->fd;
    auto cid = CIDFromOBPropertyIDGmsl(propertyId);
    // LOG_DEBUG("Entry ObV4lGmslDevicePort::getPu propertyId={}, cid={} ", propertyId, cid);

    struct v4l2_control control = { cid, 0 };

    std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
    if(xioctlGmsl(fd, VIDIOC_G_CTRL, &control) < 0) {
        LOG_ERROR("get {0} xioctlGmsl(VIDIOC_G_CTRL) failed, {1}", propertyId, strerror(errno));
        ObV4lGmslDevicePort::setPuDevIndex(0);
        lk.unlock();
        return false;
    }
    ObV4lGmslDevicePort::setPuDevIndex(0);
    lk.unlock();

    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;
    }
    value = control.value;
    LOG_DEBUG("Leave ObV4lGmslDevicePort getPu Success! propertyId={}, value: {} ", propertyId, value);

    return true;
}

UvcControlRange ObV4lGmslDevicePort::getPuRange(uint32_t propertyId) {
    auto fd = deviceHandles_[getPuDevIndex()]->fd;
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getPuRange propertyId={}, devNode:{}", propertyId, deviceHandles_[getPuDevIndex()]->info->name);

#if 0
    int num=deviceHandles_.size();
    for(int i=0;i<num;i++) {
        LOG_DEBUG("getPuRange deviceHandles_[i]->info->name:{}", deviceHandles_[i]->info->name);
    }
#endif

    // auto fd = deviceHandles_.front()->fd;
    // LOG_DEBUG("Leave ObV4lGmslDevicePort::getPuRange deviceHandles_.front()->info->name:{} \n", deviceHandles_.front()->info->name );
    if(propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_PRIORITY_INT || propertyId == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL
       || propertyId == OB_PROP_COLOR_AUTO_WHITE_BALANCE_BOOL) {
        int             min        = 0;
        int             max        = 1;
        int             step       = 1;
        int             defaultVal = 0;
        UvcControlRange range(min, max, step, defaultVal);
        return range;
    }
    struct v4l2_queryctrl query = {};
    query.id                    = CIDFromOBPropertyIDGmsl(propertyId);
    // LOG_DEBUG("--->query.id:{} \n", query.id );

    std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
    if(xioctlGmsl(fd, VIDIOC_QUERYCTRL, &query) < 0) {
        query.minimum = query.maximum = 0;
    }
    ObV4lGmslDevicePort::setPuDevIndex(0);
    lk.unlock();

    UvcControlRange range(query.minimum, query.maximum, query.step, query.default_value);
    LOG_DEBUG("Leave ObV4lGmslDevicePort::getPuRange propertyId:{}, query.minimum:{}, query.maximum:{}, query.step:{}, query.default_value:{} ", propertyId,
              query.minimum, query.maximum, query.step, query.default_value);
    return range;
}

bool ObV4lGmslDevicePort::setPuExt(uint32_t propertyId, int32_t value) {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPuExt---propertyId={0} value:{1}", propertyId, value);

    auto fd  = deviceHandles_.front()->fd;
    auto cid = CIDFromOBPropertyIDGmsl(propertyId);

    struct v4l2_ext_control control {
        cid, 0, 0, value
    };

    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = value ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
    }

    // Extract the control group from the underlying control query
    struct v4l2_ext_controls ctrls_block {
        control.id & 0xffff0000, 1, 0, 0, 0, &control
    };
    if(xioctlGmsl(fd, VIDIOC_S_EXT_CTRLS, &ctrls_block) < 0) {
        if(errno == EIO || errno == EAGAIN)  // TODO: Log?
            return false;

        throw io_exception("set propertyId " + std::to_string(propertyId) + "xioctlGmsl(VIDIOC_S_EXT_CTRLS) failed! err: " + strerror(errno));
    }

    LOG_DEBUG("-Leave ObV4lGmslDevicePort::setPuExt---propertyId={0} ", propertyId);

    return true;
}

bool ObV4lGmslDevicePort::getPuExt(uint32_t propertyId, int32_t &value) {

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getPuExt---propertyId={0} ", propertyId);

    auto fd  = deviceHandles_.front()->fd;
    auto cid = CIDFromOBPropertyIDGmsl(propertyId);

    struct v4l2_ext_control control {
        cid, 0, 0, 0
    };
    // Extract the control group from the underlying control query
    struct v4l2_ext_controls ctrls_block {
        control.id & 0xffff0000, 1, 0, 0, 0, &control
    };

    if(xioctlGmsl(fd, VIDIOC_G_EXT_CTRLS, &ctrls_block) < 0) {
        if(errno == EIO || errno == EAGAIN)  // TODO: Log?
        {
            LOG_ERROR("get {0} xioctlGmsl(VIDIOC_G_EXT_CTRLS) failed, {1}", propertyId, strerror(errno));
            return false;
        }
        throw io_exception("propertyId:" + std::to_string(propertyId) + "xioctlGmsl(VIDIOC_G_EXT_CTRLS) failed! err:" + strerror(errno));
    }

    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;
    }
    value = control.value;

    LOG_DEBUG("---Leave ObV4lGmslDevicePort::getPuExt---propertyId={0} ", propertyId);
    return true;
}

UvcControlRange ObV4lGmslDevicePort::getPuRangeExt(uint32_t propertyId) {
    return getPuRange(propertyId);
}

bool ObV4lGmslDevicePort::setPuRaw(uint32_t propertyId, int32_t value) {
    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPuRaw---cid={0} value:{1}", propertyId, value);

    auto fd  = deviceHandles_.front()->fd;
    auto cid = propertyId;

    // LOG_DEBUG("---Entry ObV4lGmslDevicePort::setPuRaw---cid={0} ", cid);
    struct v4l2_control control = { cid, value };
    // Set value
    if(xioctlGmsl(fd, VIDIOC_S_CTRL, &control) < 0) {
        LOG_ERROR("set {0} xioctlGmsl(VIDIOC_S_CTRL) failed, {1}", cid, strerror(errno));
        return false;
    }
    // LOG_DEBUG("---Leave ObV4lGmslDevicePort::setPuRaw-cid={}", propertyId);
    return true;
}

bool ObV4lGmslDevicePort::setXu(uint32_t ctrl, const uint8_t *data, uint32_t len) {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::setXu-ctrl:{}, len:{}", ctrl, len);
    VALIDATE_NOT_NULL(data);
    auto                        fd    = deviceHandles_.front()->fd;
    struct uvc_xu_control_query query = { static_cast<uint8_t>(xuUnit_.unit), static_cast<uint8_t>(ctrl), UVC_SET_CUR, static_cast<uint16_t>(len),
                                          const_cast<uint8_t *>(data) };
    if(xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &query) < 0) {
        LOG_ERROR("set xu failed, errno: {}", strerror(errno));
        return false;
    }
    LOG_DEBUG("-Leave ObV4lGmslDevicePort::setXu-ctrl:{}, len:{}", ctrl, len);
    return true;
}

bool ObV4lGmslDevicePort::getXu(uint32_t ctrl, uint8_t *data, uint32_t *len) {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getXu-ctrl:{}, *len:{} ", ctrl, *len);
    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(len);
    auto fd = deviceHandles_.front()->fd;
    memset(data, 0, *len);
    struct uvc_xu_control_query query = { static_cast<uint8_t>(xuUnit_.unit), static_cast<uint8_t>(ctrl), UVC_GET_CUR, static_cast<uint16_t>(*len),
                                          const_cast<uint8_t *>(data) };
    if(xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &query) < 0) {
        LOG_ERROR("get xu failed, errno: {}", strerror(errno));
        return false;
    }

    LOG_DEBUG("-Leave ObV4lGmslDevicePort::getXu-ctrl:{}, *len:{} ", ctrl, *len);
    return true;
}

UvcControlRange ObV4lGmslDevicePort::getXuRange(uint32_t control, int len) const {

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getXuRange-control:{}, len:{} ", control, len);
    auto                        fd = deviceHandles_.front()->fd;
    UvcControlRange             range;
    struct uvc_xu_control_query xquery {};
    memset(&xquery, 0, sizeof(xquery));
    __u16 size   = 0;
    xquery.query = UVC_GET_LEN;
    xquery.size  = 2;  // size seems to always be 2 for the LEN query, but
                       // doesn't seem to be documented. Use result for size
    // in all future queries of the same control number
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = (__u8 *)&size;
    if(xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &xquery) < 0) {
        LOG_ERROR("xioctlGmsl(VIDIOC_QUERY_EXT_CTRL) failed!");
    }
    VALIDATE(size <= len);
    std::vector<uint8_t> buf;
    auto                 buf_size = std::max((size_t)len, sizeof(__u32));
    buf.resize(buf_size);

    xquery.query    = UVC_GET_MIN;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctlGmsl(UVC_GET_MIN) failed!");
    }
    range.min.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.min.begin());

    xquery.query    = UVC_GET_MAX;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctlGmsl(UVC_GET_MAX) failed!");
    }
    range.max.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.max.begin());

    xquery.query    = UVC_GET_DEF;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctlGmsl(UVC_GET_DEF) failed!");
    }
    // def means default
    range.def.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.def.begin());

    xquery.query    = UVC_GET_RES;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctlGmsl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctlGmsl(UVC_GET_CUR) failed!");
    }
    range.step.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.step.begin());

    LOG_DEBUG("-Leave ObV4lGmslDevicePort::getXuRange-control:{}, len:{} ", control, len);
    return range;
}

bool ObV4lGmslDevicePort::setXuExt(uint32_t ctrl, const uint8_t *data, uint32_t len) {
    VALIDATE_NOT_NULL(data);
    auto fd  = deviceHandles_.front()->fd;
    auto cid = ctrl;  // CIDFromOBPropertyID(ctrl);

    i2c_msg_t *msg        = (i2c_msg_t *)data;
    uint32_t   propertyId = *(uint32_t *)msg->_data;
    LOG_DEBUG("-Entry setXuExt PropertyId:{}, len:{}, ctrl:{}", propertyId, ctrl, len);

    if(G2R_CAMERA_CID_SET_DATA == ctrl) {
        // struct v4l2_ext_control xctrl { cid, G2R_RW_DATA_LEN, 0, 0 };
        struct v4l2_ext_control xctrl {
            cid, G2R_RW_DATA_LEN, 0, 0
        };
        xctrl.p_u8 = const_cast<uint8_t *>(data);

#if 0
        struct v4l2_ext_control xctrl{ cid, uint32_t(len), 0, 0 };
        switch(len) {
        case 1:
            xctrl.value = *(reinterpret_cast<const uint8_t *>(data));
            break;
        case 2:
            xctrl.value = *reinterpret_cast<const uint16_t *>(data);
            break;  // TODO check signed/unsigned
        case 4:
            xctrl.value = *reinterpret_cast<const int32_t *>(data);
            break;
        case 8:
            xctrl.value64 = *reinterpret_cast<const int64_t *>(data);
            break;
        default:
            xctrl.p_u8 = const_cast<uint8_t *>(data);  // TODO aggregate initialization with union
        }
#endif

        if(ctrl == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL)
            xctrl.value = xctrl.value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL;

        // Extract the control group from the underlying control query
        v4l2_ext_controls ctrls_block{ xctrl.id & 0xffff0000, 1, 0, 0, 0, &xctrl };

        // LOG_DEBUG("Entry v4l_mipi_device::set_xu  ctrls_block: " << ctrls_block);
        // LOG_DEBUG("ctrls_block.ctrl_class:{} ctrls_block.count:{} ctrls_block.request_fd:{} ", ctrls_block.ctrl_class, ctrls_block.count,
        // ctrls_block.request_fd);

        int retVal = xioctlGmsl(fd, VIDIOC_S_EXT_CTRLS, &ctrls_block);
        if(retVal < 0) {
            LOG_DEBUG("--->>> set_xu---NG---");
            if(errno == EIO || errno == EAGAIN)  // TODO: Log?
                return false;

            throw io_exception("set ctrl:" + std::to_string(ctrl) + "xioctlGmsl(VIDIOC_S_EXT_CTRLS) failed! err:" + strerror(errno));
        }
    }

    // LOG_DEBUG("-Leave ObV4lGmslDevicePort::setXuExt");
    return true;
}

bool ObV4lGmslDevicePort::getXuExt(uint32_t ctrl, uint8_t *data, uint32_t *len) {

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getXuExt-ctrl:{}, *len:{} ", ctrl, *len);

    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(len);
    auto fd  = deviceHandles_.front()->fd;
    auto cid = ctrl;  // CIDFromOBPropertyID(ctrl);

    std::vector<uint8_t> dataRecvBuf(MAX_I2C_PACKET_SIZE, 0);

    // struct v4l2_ext_control control{ cid, uint32_t(*len), 0, 0 };
    struct v4l2_ext_control control {
        cid, G2R_RW_DATA_LEN, 0, 0
    };
    // control.p_u8 = data;
    control.p_u8 = dataRecvBuf.data();

    v4l2_ext_controls ext{ control.id & 0xffff0000, 1, 0, 0, 0, &control };

    // the ioctl fails once when performing send and receive right after it
    // it succeeds on the second time
    int tries = 3;
    while(tries--) {
        std::fill(dataRecvBuf.begin(), dataRecvBuf.end(), 0);

        int ret = xioctlGmsl(fd, VIDIOC_G_EXT_CTRLS, &ext);
        if(ret < 0) {
            // exception is thrown if the ioctl fails twice
            continue;
        }

        if(ctrl == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL)
            control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;

        // used to parse the data when only a value is returned (e.g. laser power),
        // and not a pointer to a buffer of data (e.g. gvd)
        if(*len < sizeof(__s64))
            memcpy(data, (void *)(&control.value), *len);

        // else if(ctrl == 10108962)
        else if(ctrl == G2R_CAMERA_CID_GET_DATA) {

#if 0
            *len=*len-8;
            std::string sn="123456789012345";
            memcpy(control.p_u8+8+80, sn.c_str(), 16 );
            memcpy(data, control.p_u8+8, (*len-8));

            printf("GET DATA:");
            for(int i = 0;i<128;i++){
                printf("%d ,", data[i]);
            }
            printf("\n");
#endif

            auto *pRecvDataBuf = reinterpret_cast<i2c_msg_t *>(&dataRecvBuf[0]);
            LOG_DEBUG("--------------------------------------------------------------------------");
            LOG_DEBUG("recvData resp code:  {} ", std::to_string(pRecvDataBuf->header.code));
            LOG_DEBUG("recvData resp index: {} ", std::to_string(pRecvDataBuf->header.index));
            LOG_DEBUG("recvData resp len:   {} ", std::to_string(pRecvDataBuf->header.len));
            LOG_DEBUG("recvData resp res:   {} ", std::to_string(pRecvDataBuf->body.res));
            LOG_DEBUG("--------------------------------------------------------------------------");

            // repeat pack usb protocolHeader
            ProtocolMsg usbProtocolMsg;
            memset(&usbProtocolMsg, 0, sizeof(usbProtocolMsg));
            usbProtocolMsg.header.opcode = pRecvDataBuf->header.code;
            usbProtocolMsg.header.nId    = pRecvDataBuf->header.index;  // return I2C resp index
            // usbProtocolMsg.header.halfWordSize = ( (pRecvDataBuf->header.len - HP_HEADER_SIZE) +1 ) / sizeof(uint16_t) ;
            usbProtocolMsg.header.magic      = 0x4252;  // HP_RESPONSE_MAGIC;
            usbProtocolMsg.buf.data.resp.res = pRecvDataBuf->body.res;

            uint16_t readRespDataSize = (pRecvDataBuf->header.len - sizeof(pRecvDataBuf->header) - sizeof(pRecvDataBuf->body.res));
            // handle  pRecvDataBuf->header.len==0 status exception
            if((pRecvDataBuf->header.len == 0) || (pRecvDataBuf->header.code == 0) || (pRecvDataBuf->header.len == 65535) || (pRecvDataBuf->header.len > 248)) {
                readRespDataSize = 0;
                LOG_ERROR("I2C read data err!. pRecvDataBuf->header.len:{}, pRecvDataBuf->header.code:{}", std::to_string(pRecvDataBuf->header.len),
                          std::to_string(pRecvDataBuf->header.code));

                if(tries == 2) {
                    LOG_INFO("I2C retry read data, sleep milliseconds(1ms). tries:{}", tries);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else if(tries == 1) {
                    LOG_INFO("I2C retry read data, sleep milliseconds(5ms). tries:{}", tries);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }

                continue;
            }

            LOG_DEBUG("cal readRespDataSize: {} ", readRespDataSize);
            usbProtocolMsg.header.halfWordSize = (readRespDataSize + sizeof(usbProtocolMsg.buf.data.resp.res) + 1) / sizeof(uint16_t);
            LOG_DEBUG("cal usbProtocolMsg.header.halfWordSize: {} ", std::to_string(usbProtocolMsg.header.halfWordSize));

            LOG_DEBUG("cal readRespDataSize:    {} ", readRespDataSize);
            usbProtocolMsg.buf.len = readRespDataSize + sizeof(usbProtocolMsg.buf.data.resp.res);
            LOG_DEBUG("cal recal usbProtocolMsg.buf.len:    {}", std::to_string(usbProtocolMsg.buf.len));

#if 0
            if(usbProtocolMsg.header.opcode == 3) {
                auto       *tmp = reinterpret_cast<VersionInfoTypeDef *>(pRecvDataBuf->body.data);
                //std::string sn  = "987654321-01234";
                //strcpy(tmp->serial_number, sn.c_str());
#if 1
                LOG_DEBUG("---> firmware_version: {} ", tmp->firmware_version );
                LOG_DEBUG("---> system_version: {} ", tmp->system_version );
                LOG_DEBUG("---> sdk_version: {} ", tmp->sdk_version );
                LOG_DEBUG("---> depth_chip: {} ", tmp->depth_chip );
                LOG_DEBUG("---> system_chip: {} ", tmp->system_chip );
                LOG_DEBUG("---> serial_number: {} ", tmp->serial_number );
                LOG_DEBUG("---> device_type: {} ", std::to_string( tmp->device_type) );
                std::string deviceName(tmp->deviceName);
                LOG_DEBUG("---> deviceName.size: {} ", deviceName.size());
                deviceName = deviceName.substr(0,16);
                strcpy(tmp->deviceName, deviceName.c_str());
                LOG_DEBUG("---> deviceName: {} ", tmp->deviceName );

                LOG_DEBUG("---> VersionInfoTypeDef.size: {} ", std::to_string(sizeof(VersionInfoTypeDef)) );
                LOG_DEBUG("---> usbProtocolMsg.buf.len: {} ", std::to_string( usbProtocolMsg.buf.len) );
#endif
            }
#endif
            // check handle  pRecvDataBuf->header.len==0 status exception
            if(readRespDataSize > 0) {
                // memcpy(usbProtocolMsg.buf.data.resp.data, pRecvDataBuf->body.data, readRespDataSize);
                std::memcpy(usbProtocolMsg.buf.data.resp.data, pRecvDataBuf->body.data, readRespDataSize);
            }
            else {
                LOG_DEBUG("get read I2C dataSize=0 or dataSize error! readRespDataSize:{}", readRespDataSize);
            }
            // memcpy(usbProtocolMsg.buf.data.resp.data, pRecvDataBuf->body.data, readRespDataSize);
            // std::memcpy(usbProtocolMsg.buf.data.resp.data, pRecvDataBuf->body.data, readRespDataSize);

#if 0  // test
        if((usbProtocolMsg.header.opcode==29)){
            for(int i=0; i<(*len-2); i++){
                uint8_t data = *(uint8_t*)(&usbProtocolMsg.buf.data+i);
                LOG_DEBUG("--->>>i:{}, data: 0x{:0x} ", i, data);
            }
        }
#endif

            *len = sizeof(usbProtocolMsg.header) + usbProtocolMsg.buf.len + sizeof(usbProtocolMsg.buf.len);
            LOG_DEBUG("-copy_usbProtocolMsg *len: {}", *len);
            // memcpy(data, &usbProtocolMsg, *len);
            std::memcpy(data, &usbProtocolMsg, *len);
#if 0
            if(usbProtocolMsg.header.opcode==3){
                uint16_t respMsgDataOffset = sizeof(usbProtocolMsg.header) + sizeof(usbProtocolMsg.buf.data.resp.res);
                LOG_DEBUG("-----------------------------------------------------");
                auto *tmp3 = reinterpret_cast<VersionInfoTypeDef*>(data+respMsgDataOffset);
                LOG_DEBUG("---> firmware_version: {} ", tmp3->firmware_version );
                LOG_DEBUG("---> system_version: {} ", tmp3->system_version );
                LOG_DEBUG("---> sdk_version: {} ", tmp3->sdk_version );
                LOG_DEBUG("---> depth_chip: {} ", tmp3->depth_chip );
                LOG_DEBUG("---> system_chip: {} ", tmp3->system_chip );
                LOG_DEBUG("---> serial_number: {} ", tmp3->serial_number );
                LOG_DEBUG("---> deviceName: {} ", tmp3->deviceName );
                LOG_DEBUG("---> device_type: {} ", std::to_string( tmp3->device_type) );
            }
#endif
        }
        // LOG_DEBUG("Leave ObV4lGmslDevicePort::getXuExt--ctrl:{} ", ctrl);
        return true;
    }

    // sending error on ioctl failure
    if(errno == EIO || errno == EAGAIN)  // TODO: Log?
        return false;
    throw io_exception("set ctrl:" + std::to_string(ctrl) + "xioctlGmsl(VIDIOC_G_EXT_CTRLS) failed! err:" + strerror(errno));
}

UvcControlRange ObV4lGmslDevicePort::getXuRangeExt(uint32_t control, int len) const {
    utils::unusedVar(len);

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::getXuRangeExt");

    auto fd  = deviceHandles_.front()->fd;
    auto cid = control;  // CIDFromOBPropertyID(control);

    v4l2_query_ext_ctrl xctrl_query{};
    xctrl_query.id = cid;

    if(0 > ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &xctrl_query)) {
        LOG_DEBUG("xioctlGmsl(VIDIOC_QUERY_EXT_CTRL) failed, errno={}", strerror(errno));
        throw io_exception("xioctlGmsl(VIDIOC_QUERY_EXT_CTRL) failed ");
    }

    if((xctrl_query.elems != 1) || (xctrl_query.minimum < std::numeric_limits<int32_t>::min()) || (xctrl_query.maximum > std::numeric_limits<int32_t>::max()))
        throw io_exception("Mipi Control range for is not compliant with backend interface [min:" + std::to_string(xctrl_query.minimum)
                           + " max:" + std::to_string(xctrl_query.maximum) + " default:" + std::to_string(xctrl_query.default_value)
                           + " step:" + std::to_string(xctrl_query.step) + " Elements:" + std::to_string(xctrl_query.elems));

    if(control == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL)
        return { 0, 1, 1, 1 };

    LOG_DEBUG("-Leave ObV4lGmslDevicePort::getXuRangeExt");

    return { static_cast<int32_t>(xctrl_query.minimum), static_cast<int32_t>(xctrl_query.maximum), static_cast<int32_t>(xctrl_query.step),
             static_cast<int32_t>(xctrl_query.default_value) };
}

//--------------------------------------------------------------------------------------------------------------------------------

void ObV4lGmslDevicePort::subscribeToCtrlEvent(uint32_t ctrl_id) const {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::subscribeToCtrlEvent");
    auto                           fd = deviceHandles_.front()->fd;
    struct v4l2_event_subscription event_subscription {};
    event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
    event_subscription.type  = V4L2_EVENT_CTRL;
    event_subscription.id    = ctrl_id;
    memset(event_subscription.reserved, 0, sizeof(event_subscription.reserved));
    if(xioctlGmsl(fd, VIDIOC_SUBSCRIBE_EVENT, &event_subscription) < 0) {
        LOG_ERROR("xioctlGmsl(VIDIOC_SUBSCRIBE_EVENT) with control_id={} failed!", ctrl_id);
    }
}

void ObV4lGmslDevicePort::unsubscribeFromCtrlEvent(uint32_t ctrl_id) const {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::unsubscribeFromCtrlEvent");
    auto                           fd = deviceHandles_.front()->fd;
    struct v4l2_event_subscription event_subscription {};
    event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
    event_subscription.type  = V4L2_EVENT_CTRL;
    event_subscription.id    = ctrl_id;
    memset(event_subscription.reserved, 0, sizeof(event_subscription.reserved));
    if(xioctlGmsl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &event_subscription) < 0) {
        LOG_ERROR("xioctlGmsl(VIDIOC_UNSUBSCRIBE_EVENT) with control_id={} failed!", ctrl_id);
    }
}

bool ObV4lGmslDevicePort::pendForCtrlStatusEvent() const {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::pendForCtrlStatusEvent");
    auto              fd = deviceHandles_.front()->fd;
    struct v4l2_event event {};
    memset(&event, 0, sizeof(event));
    // Poll registered events and verify that set control event raised (wait max of 10 * 2 = 20 [ms])
    static int MAX_POLL_RETRIES = 10;
    for(int i = 0; i < MAX_POLL_RETRIES && event.type != V4L2_EVENT_CTRL; i++) {
        if(xioctlGmsl(fd, VIDIOC_DQEVENT, &event) < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return event.type == V4L2_EVENT_CTRL;
}

// reset firmware cmd to driver. driver reset gmsl9295/9296 & firmware.
// gmsl9295/9296 power on/off is controlled by driver.
int ObV4lGmslDevicePort::resetGmslDriver() {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::rebootFirmware");
    auto fd = deviceHandles_.front()->fd;

    struct v4l2_ext_control control {};
    memset(&control, 0, sizeof(control));
    control.id    = G2R_CAMERA_CID_RESET_POWER;
    control.value = 1;
    if(xioctlGmsl(fd, VIDIOC_S_CTRL, &control) < 0) {
        LOG_ERROR("xioctlGmsl(G2R_CAMERA_CID_RESET_POWER) with control_id={} failed!", G2R_CAMERA_CID_RESET_POWER);
        return -1;
    }
    LOG_DEBUG("-Leave ObV4lGmslDevicePort::rebootFirmware");
    return 0;
}

// SYNC GPIO
// gpio num; current support only 0.
// value: 0:1
int ObV4lGmslDevicePort::setSyncGpio(uint8_t gpio, int value) {
    LOG_DEBUG("-Entry ObV4lGmslDevicePort::setSyncGpio");
    auto    fd     = deviceHandles_.front()->fd;
    uint8_t buf[2] = { 0 };

    v4l2_ext_controls controls;
    v4l2_ext_control  control;
    memset(&controls, 0, sizeof(controls));
    memset(&control, 0, sizeof(control));
    controls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    controls.controls   = &control;
    controls.count      = 1;

    buf[0] = gpio;   // gpio num; current support only 0.
    buf[1] = value;  // value: 0:1

    control.id   = G2R_CAMERA_CID_SET_GPIO;
    control.size = 2;
    control.p_u8 = const_cast<uint8_t *>(buf);

    if(xioctlGmsl(fd, VIDIOC_S_EXT_CTRLS, &controls) < 0) {
        LOG_ERROR("xioctlGmsl(G2R_CAMERA_CID_RESET_POWER) with control_id={} failed!", G2R_CAMERA_CID_RESET_POWER);
        return -1;
    }
    return 0;
}

//===============================================================================================
//----------------------------------------------------------------------------------------------------------------------------------
// bus_info:platform:tegra-capture-vi:0
#define GMSL_MIPI_DEVICE_TAG "platform:tegra-capture-vi"
bool is_gmsl_mipi_device(const std::string bus_info) {
    return bus_info.find(GMSL_MIPI_DEVICE_TAG) != std::string::npos;
}

bool is_gmsl_mipi_device_for_nvidia(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    return (portInfo->hubId.find(GMSL_MIPI_DEVICE_TAG) != std::string::npos) && (portInfo->connSpec == "GMSL2");
}

bool ObV4lGmslDevicePort::isGmslDeviceForPlatformNvidia(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    // return is_gmsl_mipi_device(portInfo->hubId);
    return is_gmsl_mipi_device_for_nvidia(portInfo);
}

void get_mipi_device_info(const std::string &dev_name, std::string &bus_info, std::string &card) {
    struct v4l2_capability vcap;
    int                    fd = -1;

    LOG_DEBUG("-Entry get_mipi_device_info---dev_name:{}", dev_name);

    fd = open(dev_name.c_str(), O_RDWR);
    if(fd < 0) {
        LOG_DEBUG("Mipi device capability could not be grabbed");
        return;
    }

    memset(&vcap, 0, sizeof(vcap));
    if(xioctlGmsl(fd, VIDIOC_QUERYCAP, &vcap) < 0) {
        struct media_device_info mdi;
        memset(&mdi, 0, sizeof(mdi));
        if(xioctlGmsl(fd, MEDIA_IOC_DEVICE_INFO, &mdi) >= 0) {
            if(mdi.bus_info[0])
                bus_info = mdi.bus_info;
            else
                bus_info = std::string("platform:") + mdi.driver;

            if(mdi.model[0])
                card = mdi.model;
            else
                card = mdi.driver;

            LOG_DEBUG("-bus_info:{}, card:{}", bus_info, card);
        }
    }
    else {
        bus_info           = reinterpret_cast<const char *>(vcap.bus_info);
        card               = reinterpret_cast<const char *>(vcap.card);
        std::string driver = reinterpret_cast<const char *>(vcap.driver);

#if 0
        LOG_DEBUG("VIDIOC_QUERYCAP driver:{} ", driver);
        LOG_DEBUG("VIDIOC_QUERYCAP version:{} ", vcap.version);
        LOG_DEBUG("VIDIOC_QUERYCAP capabilities:{} ", vcap.capabilities);
        LOG_DEBUG("VIDIOC_QUERYCAP device_caps:{} ", vcap.device_caps);
        LOG_DEBUG("VIDIOC_QUERYCAP bus_info:{} ", bus_info);
        LOG_DEBUG("VIDIOC_QUERYCAP card:{} ", card);
#endif
    }

    close(fd);
}

// Retrieve device video capabilities to discriminate video capturing and
// metadata nodes
// static uint32_t get_dev_capabilities(const std::string dev_name) {
//     LOG_DEBUG("-Entry get_dev_capabilities-dev_name: {}", dev_name);
//     // RAII to handle exceptions
//     std::unique_ptr<int, std::function<void(int *)>> fd(new int(open(dev_name.c_str(), O_RDWR | O_NONBLOCK, 0)), [](int *d) {
//         if(d && (*d)) {
//             ::close(*d);
//         }
//         delete d;
//     });

//     if(*fd < 0) {
//         LOG_DEBUG(": Cannot open {}", dev_name);
//         return EINVAL;
//     }
//     v4l2_capability cap = {};
//     if(xioctlGmsl(*fd, VIDIOC_QUERYCAP, &cap) < 0) {
//         if(errno == EINVAL)
//             LOG_DEBUG(" dev_name:{} is no V4L2 device", dev_name);
//         else
//             LOG_DEBUG(" xioctlGmsl(VIDIOC_QUERYCAP) failed");
//     }

//     LOG_DEBUG("-cap.device_caps: {}", cap.device_caps);
//     return cap.device_caps;
// }

int file_exists(const char *filename) {
    return (access(filename, F_OK) != -1);
}

std::vector<std::string> get_video_paths(std::vector<std::string> &video_paths) {
    LOG_DEBUG("-Entry v4l_uvc_device::get_video_paths- ");
    // std::vector<std::string> video_paths;
    video_paths.resize(0);

    // Enumerate all subdevices present on the system
    DIR *dir = opendir("/sys/class/video4linux");
    if(!dir) {
        LOG_DEBUG("Cannot access /sys/class/video4linux");
        return video_paths;
    }
    while(dirent *entry = readdir(dir)) {
        std::string name = entry->d_name;
        if(name == "." || name == "..")
            continue;

        // Resolve a pathname to ignore virtual video devices and  sub-devices
        static const std::regex video_dev_pattern("(\\/video\\d+)$");

        std::string path = "/sys/class/video4linux/" + name;
        std::string real_path{};
        char        buff[PATH_MAX] = { 0 };
        if(realpath(path.c_str(), buff) != nullptr) {
            real_path = std::string(buff);
            if(real_path.find("virtual") != std::string::npos)
                continue;
            if(!std::regex_search(real_path, video_dev_pattern)) {
                // LOG_INFO("Skipping Video4Linux entry {} - not a device", real_path );
                continue;
            }
        }

        if(real_path.empty()) {
            LOG_DEBUG("-get_video_paths real_path.empty()");
        }
        else {
            LOG_DEBUG("-get_video_paths real_path:{}", real_path);
            video_paths.push_back(real_path);
        }
    }
    closedir(dir);

    // UVC nodes shall be traversed in ascending order for metadata nodes
    // assignment ("dev/video1, Video2.. Replace lexicographic with numeric sort
    // to ensure "video2" is listed before "video11"

    if(video_paths.size() > 0) {
        std::sort(video_paths.begin(), video_paths.end(), [](const std::string &first, const std::string &second) {
            // getting videoXX
            std::string first_video  = first.substr(first.find_last_of('/') + 1);
            std::string second_video = second.substr(second.find_last_of('/') + 1);

            // getting the index XX from videoXX
            std::stringstream first_index(first_video.substr(first_video.find_first_of("0123456789")));
            std::stringstream second_index(second_video.substr(second_video.find_first_of("0123456789")));
            int               left_id = 0, right_id = 0;
            first_index >> left_id;
            second_index >> right_id;
            return left_id < right_id;
        });
        LOG_DEBUG("-get_video_paths-video_paths.size:{}, video_paths.at(1):{}", video_paths.size(), video_paths.at(1));
    }
    LOG_DEBUG("Leave:-get_video_paths");
    return video_paths;
}

UsbInterfaceInfo get_ImuInfo_from_mipi_device_path(const std::string &video_path, const std::string &name) {
    uint16_t    vid{}, pid{}, mi{}, portCls{};
    UsbSpec     usb_specification(gmsl2_type);  // usb3_type);  // usb_undefined);
    std::string bus_info, card, sn, imuDev;

    auto dev_name = "/dev/" + name;

    LOG_DEBUG("-Entry get_info_from_mipi_device_path-video_path: {}, dev_name: {}", video_path, dev_name);
    // get_mipi_device_info(dev_name, bus_info, card);
    if(dev_name == "/dev/v4l-subdev35") {
        bus_info = "platform:tegra-capture-vi:0";
        card     = "vi-output, DS5 mux 30-0066";
        LOG_DEBUG("set bus_info: {}, card: {}", bus_info, card);
    }

    static std::regex video_dev_index("\\d+$");
    std::smatch       match;
    uint8_t           ind{};
    if(std::regex_search(name, match, video_dev_index)) {
        ind = static_cast<uint8_t>(std::stoi(match[0]));
    }
    else {
        LOG_DEBUG("Unresolved Video4Linux device pattern:  name device is skipped");
    }

    int cam_id = 0;  // ind / camera_video_nodes;

    // if(ind == 35)
    {
        mi      = 4;  // IMU node indicator
        portCls = 4;
        LOG_DEBUG("---set IMU port index 4-- ind:{}, portCls:{}", ind, portCls);
    }
    // LOG_DEBUG("-----------------------set test-------------------------------------");

    vid = GMSL_VID_ORBBEC;
    pid = devPid;  // 0x080B; ///0x06D0; //0x080B;  //0x06D0;  // D457 dev
    sn  = devSn;   //"0123456789";
    LOG_DEBUG("--->>>imu set dev pid&sn vid:{}, pid:{}, sn:{}", vid, pid, sn);

    UsbInterfaceInfo info{};
    info.pid              = pid;
    info.vid              = vid;
    info.infIndex         = ind;         // dev/video*  video index
    info.infName          = dev_name;    // devname
    info.infUrl           = video_path;  // devpath
    info.infNameDescIndex = mi;          // video type;

    info.serial = sn;                // dev sn
    info.hubId  = bus_info;          // std::to_string(cam_id); //camera index
    info.cls    = OB_USB_CLASS_HID;  // dev porttype

    info.url       = "usb://" + std::to_string(info.pid) + ":" + std::to_string(info.vid) + "/1/" + std::to_string(cam_id);  //"usb://06d0:2bc5/1/4";
    info.uid       = "1-1." + std::to_string(ind) + "-" + std::to_string(cam_id);
    info.conn_spec = usb_specification;

    LOG_DEBUG("-info.uid:{}, info.url:{}, info.infUrl:{}, info.infIndex:{}, info.hubId:{}, info.cls:{}", info.uid, info.url, info.infUrl, info.infIndex,
              info.hubId, info.cls);
    return info;
}

std::vector<std::string> get_imu_paths(std::vector<std::string> &imu_paths) {
    LOG_DEBUG("-Entry gmsl get_imu_paths ");
    // std::vector<std::string> imu_paths;
    imu_paths.resize(0);

    // Enumerate all subdevices present on the system
    DIR *dir = opendir("/sys/class/video4linux");
    if(!dir) {
        LOG_DEBUG("Cannot access /sys/class/video4linux \n");
        return imu_paths;
    }
    while(dirent *entry = readdir(dir)) {
        std::string name = entry->d_name;
        if(name == "." || name == "..")
            continue;

        // Resolve a pathname to ignore virtual video devices and  sub-devices
        static const std::regex video_dev_pattern("(\\/v4l-subdev\\d+)$");

        std::string path = "/sys/class/video4linux/" + name;
        std::string real_path{};
        char        buff[PATH_MAX] = { 0 };
        if(realpath(path.c_str(), buff) != nullptr) {
            real_path = std::string(buff);
            if(real_path.find("virtual") != std::string::npos)
                continue;
            if(!std::regex_search(real_path, video_dev_pattern)) {
                // printf("Skipping Video4Linux entry real path:%s  not a device \n",  real_path.c_str()  );
                continue;
            }
        }

        std::string pattern_imu_dev_num = real_path.substr(real_path.find_last_of('/') + 1);
        // printf("----->>>> pattern_imu_dev_num:%s \n", pattern_imu_dev_num.c_str() );
        if(pattern_imu_dev_num.find("35") == std::string::npos)
            continue;

        LOG_DEBUG("-get_imu_paths-real_path:{}", real_path);
        imu_paths.push_back(real_path);
    }
    closedir(dir);

    LOG_DEBUG("---Leave_get_imu_paths---");
    return imu_paths;
}

static const std::map<UsbSpec, std::string> usb_spec_names_v4l2 = { { usb_undefined, "Undefined" }, { usb1_type, "1.0" },   { usb1_1_type, "1.1" },
                                                                    { usb2_type, "2.0" },           { usb2_1_type, "2.1" }, { usb3_type, "3.0" },
                                                                    { usb3_1_type, "3.1" },         { usb3_2_type, "3.2" } };

bool is_usb_device_path(const std::string &video_path) {
    LOG_DEBUG("-Entry is_usb_device_path");
    static const std::regex uvc_pattern("(\\/usb\\d+\\/)\\w+");  // Locate UVC device path pattern ../usbX/...
    return std::regex_search(video_path, uvc_pattern);
}

// retrieve the USB specification attributed to a specific USB device.
// This functionality is required to find the USB connection type for UVC device
// Note that the input parameter is passed by value
static UsbSpec get_usb_connection_type(std::string path) {
    LOG_DEBUG("Entry get_usb_connection_type-path: " + path);
    UsbSpec res{ usb_undefined };

    char usb_actual_path[PATH_MAX] = { 0 };
    if(realpath(path.c_str(), usb_actual_path) != nullptr) {
        path = std::string(usb_actual_path);
        std::string val;
        if(!(std::ifstream(path + "/version") >> val))
            throw pal_exception("Failed to read usb version specification ");

        LOG_DEBUG("-val:{}", val);

#if 0
        auto kvp = std::find_if(usb_spec_names.begin(), usb_spec_names.end(), [&val](const std::pair<UsbSpec, std::string> &kpv) { return (std::string::npos != val.find(kpv.second)); });
        if(kvp != std::end(usb_spec_names))
            res = kvp->first;
#endif

        auto kvp = std::find_if(usb_spec_names_v4l2.begin(), usb_spec_names_v4l2.end(),
                                [&val](const std::pair<UsbSpec, std::string> &kpv) { return (std::string::npos != val.find(kpv.second)); });
        if(kvp != std::end(usb_spec_names_v4l2))
            res = kvp->first;
    }
    LOG_DEBUG("Leave get_usb_connection_type-res:0x{:0x}", res);
    return res;
}

const size_t MAX_DEV_PARENT_DIR = 10;
bool         is_usb_path_valid(const std::string &usb_video_path, const std::string &dev_name, std::string &busnum, std::string &devnum, std::string &devpath) {
    utils::unusedVar(usb_video_path);
    struct stat st = {};
    if(stat(dev_name.c_str(), &st) < 0) {
        throw pal_exception("Cannot identify " + dev_name);
    }
    if(!S_ISCHR(st.st_mode))
        throw pal_exception(dev_name + " is no device");

    // Search directory and up to three parent directories to find busnum/devnum
    auto               valid_path = false;
    std::ostringstream ss;
    ss << "/sys/dev/char/" << major(st.st_rdev) << ":" << minor(st.st_rdev) << "/device/";

    auto char_dev_path = ss.str();

    for(auto i = 0U; i < MAX_DEV_PARENT_DIR; ++i) {
        if(std::ifstream(char_dev_path + "busnum") >> busnum) {
            if(std::ifstream(char_dev_path + "devnum") >> devnum) {
                if(std::ifstream(char_dev_path + "devpath") >> devpath) {
                    valid_path = true;
                    break;
                }
            }
        }
        char_dev_path += "../";
    }
    LOG_DEBUG("Leave v4l_uvc_device::get_video_paths-valid_path:{} \n", valid_path);
    return valid_path;
}

UsbInterfaceInfo get_info_from_usb_device_path(const std::string &video_path, const std::string &name) {
    std::string busnum, devnum, devpath;
    auto        dev_name = "/dev/" + name;

    LOG_DEBUG("Entry get_info_from_usb_device_path-video_path:{} ", video_path.c_str());
    if(!is_usb_path_valid(video_path, dev_name, busnum, devnum, devpath)) {

#ifndef RS2_USE_CUDA
        /* On the Jetson TX, the camera module is CSI & I2C and does not report as this code expects
        Patch suggested by JetsonHacks: https://github.com/jetsonhacks/buildLibrealsense2TX */
        LOG_INFO("Failed to read busnum/devnum. Device Path: " + ("/sys/class/video4linux/" + name));
#endif
        throw pal_exception("Failed to read busnum/devnum of usb device");
    }

    LOG_INFO("Enumerating UVC " + name + " realpath=" + video_path);
    uint16_t vid{}, pid{}, mi{};
    UsbSpec  usb_specification(usb_undefined);

    std::string modalias;
    if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/modalias") >> modalias))
        throw pal_exception("Failed to read modalias");
    if(modalias.size() < 14 || modalias.substr(0, 5) != "usb:v" || modalias[9] != 'p')
        throw pal_exception("Not a usb format modalias");
    if(!(std::istringstream(modalias.substr(5, 4)) >> std::hex >> vid))
        throw pal_exception("Failed to read vendor ID");
    if(!(std::istringstream(modalias.substr(10, 4)) >> std::hex >> pid))
        throw pal_exception("Failed to read product ID");
    if(!(std::ifstream("/sys/class/video4linux/" + name + "/device/bInterfaceNumber") >> std::hex >> mi))
        throw pal_exception("Failed to read interface number");

    // Find the USB specification (USB2/3) type from the underlying device
    // Use device mapping obtained in previous step to traverse node tree
    // and extract the required descriptors
    // Traverse from
    // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/3-6:1.0/video4linux/video0
    // to
    // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/version
    usb_specification = get_usb_connection_type(video_path + "/../../../");

    UsbInterfaceInfo info{};
    info.pid       = pid;
    info.vid       = vid;
    info.infIndex  = mi;          // usb interface
    info.infName   = dev_name;    // video name
    info.infUrl    = video_path;  // video path
    info.uid       = busnum + "-" + devpath + "-" + devnum;
    info.conn_spec = usb_specification;
    // info.uvc_capabilities = get_dev_capabilities(dev_name);

    LOG_DEBUG("Leave get_info_from_usb_device_path-info.id: {} \n", info.uid.c_str());
    return info;
}

UsbInterfaceInfo get_info_from_mipi_device_path(const std::string &video_path, const std::string &name) {
    uint16_t vid{}, pid{}, mi{}, portCls{};
    uint16_t video_type = 0, sub_num = 1, cam_id = 0;
    uint32_t cam_num = 0;

    UsbSpec     usb_specification(gmsl2_type);
    std::string bus_info, card, sn, imuDev;
    auto        dev_name = "/dev/" + name;

    UsbInterfaceInfo info{};

    LOG_DEBUG("-Entry get_info_from_mipi_device_path video_path:{}, dev_name:{}", video_path, dev_name);

    get_mipi_device_info(dev_name, bus_info, card);

    static std::regex video_dev_index("\\d+$");
    std::smatch       match;
    uint8_t           ind{};
    if(std::regex_search(name, match, video_dev_index)) {
        ind = static_cast<uint8_t>(std::stoi(match[0]));
    }
    else {
        LOG_DEBUG("Unresolved Video4Linux device pattern:  name device is skipped");
    }

    // if(ind==0)//depth
    {
        struct orbbec_device_info devInfo;
        memset(&devInfo, 0, sizeof(orbbec_device_info));
        int res = getDeviceInfoFromFW(dev_name, &devInfo);
        if(res == 0) {
            LOG_DEBUG("-GetPidSn VID:{}, PID:{}, SN:{}, AsicSN:{}, video_type:{}, cam_num:{}, sub_num:{}", devInfo.vid, devInfo.pid, (char *)devInfo.sn,
                      (char *)devInfo.asic_sn, devInfo.video_type, devInfo.cam_num, devInfo.sub_num);
            devPid    = devInfo.pid;
            devSn     = (char *)devInfo.sn;
            devAsicSn = (char *)devInfo.asic_sn;

            video_type = devInfo.video_type;
            sub_num    = devInfo.sub_num;
            cam_num    = devInfo.cam_num;

            /**
             * 1:ORB_MUX_PAD_DEPTH
             * 2:ORB_MUX_PAD_RGB
             * 3:ORB_MUX_PAD_IR_L
             * 4:ORB_MUX_PAD_IR_R
             */
            if(video_type == ORB_MUX_PAD_DEPTH) {
                info.infIndex = INTERFACE_DEPTH;
            }
            else if(video_type == ORB_MUX_PAD_RGB) {
                info.infIndex = INTERFACE_COLOR;
            }
            else if(video_type == ORB_MUX_PAD_IR_L) {
                info.infIndex = INTERFACE_IR_LEFT;
            }
            else if(video_type == ORB_MUX_PAD_IR_R) {
                info.infIndex = INTERFACE_IR_RIGHT;
            }

            mi      = 0;  // video node indicator
            portCls = 1;
            cam_id  = cam_num;
            LOG_INFO("-video_type:{}, sub_num:{}, cam_num:{}, info.infIndex:{}, portCls:{}, cam_id:{} ", video_type, sub_num, cam_num, info.infIndex, portCls,
                     cam_id);
        }
        else {
            devPid    = GMSL_PID_G335L;
            devSn     = GMSL_SN_DEFAULT;
            devAsicSn = GMSL_ASIC_SN_DEFAULT;
            LOG_DEBUG("-GetPidSn failed, set faultPidSN devPid:{}, SN:{}, AsicSN:{}", devPid, devSn, devAsicSn);
        }
    }

    vid = GMSL_VID_ORBBEC;
    pid = devPid;  // 0x080B; ///0x06D0; //0x080B;  //0x06D0;  // D457 dev
    sn  = devSn;   //"0123456789";
    LOG_DEBUG("-set dev pid&sn vid:{}, pid:{}, sn:{}, sub_num", vid, pid, sn, sub_num);

    info.pid = pid;
    info.vid = vid;
    // info.infIndex     = ind;         // dev/video*  video index
    info.infName          = dev_name;    // video name
    info.infUrl           = video_path;  // video path
    info.infNameDescIndex = mi;          // video type;

    info.serial = sn;        // dev sn
    info.hubId  = bus_info;  // std::to_string(cam_id); //camera index
    if(portCls == 1) {
        info.cls = OB_USB_CLASS_VIDEO;  // dev porttype
    }
    else if(portCls == 4) {
        info.cls = OB_USB_CLASS_HID;  // dev porttype
    }
    // unique id for MIPI: This will assign sensor set for each camera.
    // it cannot be generated as in usb, because the params busnum, devpath and
    // devnum are not available via mipi assign unique id for mipi by appending
    // camera id to bus_info (bus_info is same for each mipi port) Note - jetson
    // can use only bus_info, as card is different for each sensor and metadata
    // node.
    //"usb://06d0:2bc5/1/4";
    info.url = "gmsl2://" + std::to_string(info.pid) + ":" + std::to_string(info.vid) + "/1/" + std::to_string(cam_id);
    // info.uid = bus_info + "-" + std::to_string(cam_id);  // use bus_info as per camera unique id for mipi
    info.uid = "gmsl2-" + std::to_string(info.pid) + "-" + std::to_string(info.vid) + "-" + std::to_string(cam_id) + "-" + std::to_string(ind) + "-"
               + std::to_string(sub_num) + "-" + std::to_string(video_type);
    info.conn_spec = usb_specification;
    // info.uvc_capabilities = get_dev_capabilities(dev_name);

    LOG_DEBUG("-info.uid:{}, info.url:{}, info.infUrl:{}, info.infIndex:{}, info.hubId:{}", info.uid, info.url, info.infUrl, info.infIndex, info.hubId);
    return info;
}

int ObV4lGmslDevicePort::foreach_uvc_device(std::vector<std::string> video_paths) {
    for(auto &&video_path: video_paths) {
        // following line grabs video0 from
        auto name = video_path.substr(video_path.find_last_of('/') + 1);

        try {
            UsbInterfaceInfo info{};
            { info = get_info_from_mipi_device_path(video_path, name); }
            auto dev_name = "/dev/" + name;
            LOG_DEBUG("--->>> dev_name: {}", dev_name);
            // uvc_nodes.emplace_back(info, dev_name);
        }
        catch(const std::exception &e) {
            LOG_DEBUG("Not a USB video device! ex:{}", e.what());
        }
    }

    return 0;
}

const std::vector<UsbInterfaceInfo> ObV4lGmslDevicePort::queryDevicesInfo() {

    LOG_DEBUG("-Entry ObV4lGmslDevicePort::queryDevicesInfo");

    std::vector<UsbInterfaceInfo> devInfoList_;
    std::vector<std::string>      video_paths, imu_paths;
    devInfoList_.resize(0);

    get_video_paths(video_paths);
    if(video_paths.size() > 0) {
        for(auto &&video_path: video_paths) {
            // following line grabs video0 from
            auto name = video_path.substr(video_path.find_last_of('/') + 1);

            try {
                UsbInterfaceInfo info{};

                if(is_usb_device_path(video_path)) {
                    info = get_info_from_usb_device_path(video_path, name);
                }
                else {
                    info = get_info_from_mipi_device_path(video_path, name);

#if 0
                    auto uid=info.uid;
                    LOG_DEBUG("--uid:{}",uid);
                    auto streamType = uid.substr(uid.find_last_of('-') + 1);

                    LOG_DEBUG("--streamType:{}", streamType );
                    auto tmp_substr=uid.substr(0, uid.find_last_of('-'));
                    LOG_DEBUG("--tmp_substr:{}", tmp_substr );
                    auto imu_num = tmp_substr.substr(tmp_substr.find_last_of('-') + 1);
                    LOG_DEBUG("--imu_num:{}", imu_num );

                    if( stoi(streamType) == ORB_MUX_PAD_DEPTH ){
                        UsbInterfaceInfo info2{};
                        std::memcpy(&info2, &info, sizeof(UsbInterfaceInfo));
                        info2.cls = OB_USB_CLASS_HID;  // dev porttype

                        auto dev_imu_name = "/dev/v4l-subdev" + imu_num;
                        LOG_DEBUG("-dev_imu_name:{}, info2.infUrl:{}, info2.hubId:{}", dev_imu_name, info2.infUrl, info2.hubId);

                        //if( (info.vid == ORBBEC_USB_VID) && (is_gmsl_mipi_device(info.hubId)) ) // 通过vid==0X2bc5过滤掉非奥比设备
                        if(info2.vid == ORBBEC_USB_VID)
                        {
                            //devInfoList_.push_back(info2);
                        }
                    }
#endif
                }

                //{ info = get_info_from_mipi_device_path(video_path, name); }
                auto dev_name = "/dev/" + name;
                LOG_DEBUG("-dev_name:{}, info.infUrl:{}, info.hubId:{}", dev_name, info.infUrl, info.hubId);
                // uvc_nodes.emplace_back(info, dev_name);

                // if( (info.vid == ORBBEC_USB_VID) && (is_gmsl_mipi_device(info.hubId)) ) // 通过vid==0X2bc5过滤掉非奥比设备
                if(info.vid == ORBBEC_USB_VID) {
                    devInfoList_.push_back(info);
                }
            }
            catch(const std::exception &e) {
                LOG_DEBUG("Not a GMSL MIPI video device! ex:{}", e.what());
            }
        }
    }

    LOG_DEBUG("video queryDevicesInfo devInfoList_.size():{}", devInfoList_.size());

#if 1
    if(video_paths.size() > 0) {
        for(auto &&video_path: video_paths) {
            // following line grabs video0 from
            auto name = video_path.substr(video_path.find_last_of('/') + 1);
            try {
                UsbInterfaceInfo info{};
                if(!is_usb_device_path(video_path)) {
                    info = get_info_from_mipi_device_path(video_path, name);

                    auto uid = info.uid;
                    LOG_DEBUG("--uid:{}", uid);
                    auto streamType = uid.substr(uid.find_last_of('-') + 1);
                    LOG_DEBUG("--streamType:{}", streamType);
                    auto tmp_substr = uid.substr(0, uid.find_last_of('-'));
                    LOG_DEBUG("--tmp_substr:{}", tmp_substr);
                    auto imu_num = tmp_substr.substr(tmp_substr.find_last_of('-') + 1);
                    LOG_DEBUG("--imu_num:{}", imu_num);

                    if(stoi(streamType) == ORB_MUX_PAD_DEPTH) {
                        info.cls          = OB_USB_CLASS_HID;  // dev porttype
                        auto dev_imu_name = "/dev/v4l-subdev" + imu_num;
                        LOG_DEBUG("-dev_imu_name:{}, info.infUrl:{}, info.hubId:{}", dev_imu_name, info.infUrl, info.hubId);
                        info.infName = dev_imu_name;

                        // if( (info.vid == ORBBEC_USB_VID) && (is_gmsl_mipi_device(info.hubId)) ) // 通过vid==0X2bc5过滤掉非奥比设备
                        if(info.vid == ORBBEC_USB_VID) {
                            devInfoList_.push_back(info);
                        }
                    }
                }
            }
            catch(const std::exception &e) {
                LOG_DEBUG("Get a GMSL MIPI IMU device! ex:{}", e.what());
            }
        }
    }
#endif

#if 0
    get_imu_paths(imu_paths);
    if(imu_paths.size() > 0) {
        //LOG_DEBUG("---001---");
        for(auto &&imu_path: imu_paths) {
            //LOG_DEBUG("---002---");
            // following line grabs video0 from
            auto name = imu_path.substr(imu_path.find_last_of('/') + 1);
            LOG_DEBUG("IMU device name:{}", name );
            try {
                UsbInterfaceInfo info{};
                { info = get_ImuInfo_from_mipi_device_path(imu_path, name); }
                auto dev_name = "/dev/" + name;
                LOG_DEBUG("-dev_name:{}, info.infUrl:{}, info.hubId:{}", dev_name, info.infUrl, info.hubId);
                // uvc_nodes.emplace_back(info, dev_name);

                if( (info.vid == ORBBEC_USB_VID) && (is_gmsl_mipi_device(info.hubId)) ) // 通过vid==0X2bc5过滤掉非奥比设备
                //if(is_gmsl_mipi_device(info.hubId))
                {
                    LOG_DEBUG("-2-dev_name:{}, info.infUrl:{}, info.hubId:{}", dev_name, info.infUrl, info.hubId);
                    devInfoList_.push_back(info);
                }
            }
            catch(const std::exception &e) {
                LOG_DEBUG("Not a GMSL MIPI video device! ex:{}", e.what());
            }
        }
    }
#endif

    LOG_DEBUG("Leave queryDevicesInfo devInfoList_.size():{}", devInfoList_.size());
    LOG_DEBUG("queryDevicesInfo done!");
    return devInfoList_;
}

//----------------------------------------------------------------------------------------------------------------------------------

#if 0
//for mutil-device-sync
typedef struct {
    uint8_t mode;
    uint16_t fps;
} cs_param_t;

int setSocHardwareSync(){
    cs_param_t rd_par = {0, 0}, param = {1, 3040}; //30.4
    int ret, fd;

    // 检查设备节点是否存在
    if (access("/dev/camsync", F_OK) != 0) {
        std::cerr << "Device node /dev/camsync does not exist." << std::endl;
        return -1;
    }

    fd = open("/dev/camsync", O_RDWR);
    if(fd < 0) {
        printf("open /dev/camsync failed\n");
        return 0;
    }

    ret = write(fd, &param, sizeof(param));
    if(ret < 0) {
        printf("write /dev/camsync failed\n");
        return 0;
    }

    ret = read(fd, &rd_par, sizeof(rd_par));
    if(ret < 0) {
        printf("write /dev/camsync failed\n");
        return 0;
    }
    printf("mode=%d, fps=%d\n", rd_par.mode, rd_par.fps);

    close(fd);
    return 0;
}
#endif

}  // namespace libobsensor