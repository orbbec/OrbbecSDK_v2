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
    { 0x47524559, 0x59382020 }, /*'GREY' to 'Y8  '*/
    { 0x48455643, 0x48323635 }, /*'HEVC' to 'H265'*/
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

int getGmslDeviceInfoFromFW(const std::string &dev_name, void *data) {
    int ret = 0, fd = -1;
    // LOG_DEBUG("-Entry get_pid_sn dev_name:{}", dev_name);

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
        LOG_DEBUG("ioctl failed on getGmslDeviceInfoFromFW-pid-vid from videox strerror:{}", strerror(errno));  // printf err message
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
        // Assume the version string starts with "Linux version ", followed by the version number
        // Check for the "-tegra" suffix and other identifiers that may be included in the version number
        if(line.find("5.15.136-tegra") != std::string::npos) {
            return Platform::Orin;
        }
        else if(line.find("5.10.104-tegra") != std::string::npos) {
            return Platform::Xavier;
        }
        // You can add more conditions to check for other versions or platforms if needed
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

    std::string portInfo_streamType = portInfo->uid.substr(portInfo->uid.find_last_of('-') + 1);
    // int portInfo_streamType_int = std::stoi(portInfo_streamType);

    int portinfo_videoIndex = checkVideoIndex(portInfo->infName);

    min_node = portinfo_videoIndex;
    max_node = portinfo_videoIndex + 1;

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
            // auto url = busNum + "-" + devPath + "-" + devNum;
            // auto url = "usb://06d0:2bc5/1/0";
            // LOG_DEBUG("----------------------vid: {0}, pid: {1}, portInfo->vid: {2}, portInfo->pid: {3}", vid, pid, portInfo->vid, portInfo->pid);
            //  if(portInfo->vid == vid && portInfo->pid == pid) && portInfo->infIndex == mi && portInfo->url == url)
            {

                // Find the USB specification (USB2/3) type from the underlying device
                // Use device mapping obtained in previous step to traverse node tree
                // and extract the required descriptors
                // Traverse from
                /// sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/3-6:1.0/video4linux/video0
                // to
                /// sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/version
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

                while(!quit && xioctlGmsl(devHandle->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0) {
                    if(frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if(frame_interval.discrete.numerator != 0) {
                            auto format = utils::uvcFourccToOBFormat(fourcc);
                            if(format != OB_FORMAT_UNKNOWN) {
                                auto width  = frame_size.discrete.width;
                                auto height = frame_size.discrete.height;
                                auto fps    = static_cast<float>(frame_interval.discrete.denominator) / static_cast<float>(frame_interval.discrete.numerator);
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
            // devHandle->info->name = "/dev/video1";  //for test use.
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

//------------------------------------------------------------------------------------------------------------------------------
// Crop depth map function
void cropDepthImage(const uint8_t *src, uint8_t *dst, int srcWidth, int srcHeight, int cropRitght, int cropBottom) {
    assert(src != nullptr && dst != nullptr);
    int cropWidth  = srcWidth - cropRitght;
    int cropHeight = srcHeight - cropBottom;
    assert(srcWidth >= cropWidth && srcHeight >= cropHeight);

    // LOG_DEBUG("--->cropWidth:{}, cropHeight:{}, cropRitght:{}, cropBottom:{} \n", cropWidth, cropHeight, cropRitght, cropBottom);

    // Calculate the number of pixels per row of the source and destination images (in bytes)
    // const size_t srcBytesPerRow = srcWidth *sizeof(uint8_t);
    const size_t dstBytesPerRow = cropWidth * sizeof(uint8_t);

    // Iterate through each line of the image and copy the pixels to the destination buffer
    for(int y = 0; y < cropHeight; y++) {
        // Calculate the source and destination pointers of the current row
        const uint8_t *srcRow = src + y * srcWidth;
        uint8_t       *dstRow = dst + y * cropWidth;

        // Copy the pixels of the current row (except the rightmost 32 columns)
        // memcpy(dstRow, srcRow, dstBytesPerRow);
        std::memcpy(dstRow, srcRow, dstBytesPerRow);
    }
}

// Crop depth map function
void cropDepthImage16(const uint16_t *src, uint16_t *dst, int srcWidth, int srcHeight, int cropRitght, int cropBottom) {
    assert(src != nullptr && dst != nullptr);
    int cropWidth  = srcWidth - cropRitght;
    int cropHeight = srcHeight - cropBottom;
    assert(srcWidth >= cropWidth && srcHeight >= cropHeight);

    // LOG_DEBUG("--->>>cropWidth:%d, cropHeight:%d, cropRitght:%d, cropBottom:%d\n", cropWidth, cropHeight, cropRitght, cropBottom);

    // Calculate the number of pixels per row of the source and destination images (in bytes)
    // const size_t srcBytesPerRow = srcWidth *sizeof(uint16_t);
    const size_t dstBytesPerRow = cropWidth * sizeof(uint16_t);

    // Iterate through each line of the image and copy the pixels to the destination buffer
    for(int y = 0; y < cropHeight; y++) {
        // Calculate the source and destination pointers of the current row
        const uint16_t *srcRow = src + y * srcWidth;
        uint16_t       *dstRow = dst + y * cropWidth;

        // Copy the pixels of the current row (except the rightmost 32 columns)
        // memcpy(dstRow, srcRow, dstBytesPerRow);
        std::memcpy(dstRow, srcRow, dstBytesPerRow);
    }
}

// Assume buf is a pointer to data and size is the size of the data (in bytes)
void writeBufferToFile(const char *buf, std::size_t size, const std::string &filename) {
    std::ofstream file(filename, std::ios::out | std::ios::binary);  // Open the file in binary mode

    if(!file.is_open()) {
        // If the file fails to open, throw an exception or handle the error
        throw std::runtime_error("无法打开文件: " + filename);
    }

    // Write the data in buf to the file
    file.write(buf, size);

    // close file
    file.close();
}

void ObV4lGmslDevicePort::captureLoop(std::shared_ptr<V4lDeviceHandleGmsl> devHandle) {
    int metadataBufferIndex = -1;
    int colorFrameNum       = 0;  // color drop 1~3 frame -> fix color green screen issue.
    try {
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
                // Handle timeout situations
                continue;
            }

            if(FD_ISSET(devHandle->stopPipeFd[0], &fds) || FD_ISSET(devHandle->stopPipeFd[1], &fds)) {
                if(!devHandle->isCapturing) {
                    LOG_DEBUG("V4L stream is closed: {}", devHandle->info->name);
                }
                else {
                    LOG_ERROR("Stop pipe was signalled during streaming: {}", devHandle->info->name);
                }
                break;
            }

            if(devHandle->metadataFd >= 0 && FD_ISSET(devHandle->metadataFd, &fds)) {
                FD_CLR(devHandle->metadataFd, &fds);
                v4l2_buffer buf = { 0 };
                memset(&buf, 0, sizeof(buf));
                buf.type   = LOCAL_V4L2_BUF_TYPE_META_CAPTURE_GMSL;
                buf.memory = USE_MEMORY_MMAP ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                if(xioctlGmsl(devHandle->metadataFd, VIDIOC_DQBUF, &buf) < 0) {
                    LOG_DEBUG("VIDIOC_DQBUF failed, {}, {}", strerror(errno), devHandle->metadataInfo->name);
                }

                if((buf.bytesused) && (!(buf.flags & V4L2_BUF_FLAG_ERROR))) {
                    devHandle->metadataBuffers[buf.index].actual_length = buf.bytesused;
                    devHandle->metadataBuffers[buf.index].sequence      = buf.sequence;
                    metadataBufferIndex                                 = buf.index;
                    // LOG_DEBUG("captureLoop-metadata devname:{}, buf.sequence:{}, buf.bytesused:{}, buf.index:{}", devHandle->info->name, buf.sequence,
                    // buf.bytesused, buf.index);
                }

                if(devHandle->isCapturing) {
                    if(xioctlGmsl(devHandle->metadataFd, VIDIOC_QBUF, &buf) < 0) {
                        LOG_ERROR("devHandle->metadataFd VIDIOC_QBUF, errno: {0}, {1}, {3}", strerror(errno), errno, __LINE__);
                    }
                }
            }

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

                    if(metadataBufferIndex >= 0) {  // temp fix orbbecviewer metadata view flash issue. reason:Occasional missing of one frame in metadata data.
                        auto &metaBuf                = devHandle->metadataBuffers[metadataBufferIndex];
                        auto  uvc_payload_header     = metaBuf.ptr;
                        auto  uvc_payload_header_len = metaBuf.actual_length;
                        videoFrame->updateMetadata(static_cast<const uint8_t *>(uvc_payload_header), 12);
                        videoFrame->appendMetadata(static_cast<const uint8_t *>(uvc_payload_header), uvc_payload_header_len);

                        if(uvc_payload_header_len >= sizeof(StandardUvcFramePayloadHeader)) {
                            auto payloadHeader = (StandardUvcFramePayloadHeader *)uvc_payload_header;
                            videoFrame->setTimeStampUsec(payloadHeader->dwPresentationTime);
                        }

                        auto realtime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        videoFrame->setSystemTimeStampUsec(realtime);
                        videoFrame->setNumber(buf.sequence);

                        if(devHandle->profile->getType() == OB_STREAM_COLOR) {
                            if(colorFrameNum >= 3) {
                                devHandle->frameCallback(videoFrame);
                            }
                            else {
                                colorFrameNum++;
                                LOG_DEBUG("captureLoop colorFrameNum<3 drop. colorFrameNum:{}", colorFrameNum);
                            }
                        }
                        else {
                            devHandle->frameCallback(videoFrame);
                        }
                    }
                }

                if(devHandle->isCapturing) {
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
    // handle 848x480; 848x100; 424x240, 480x270 resolution
    auto width      = devHandle->profile->getWidth();
    auto height     = devHandle->profile->getHeight();
    auto streamType = devHandle->profile->getType();
    if((width % 424) == 0 || (width == 480 && height == 270)) {
        int originalWidth  = width;
        int originalHeight = height;
        int paddedWidth    = 0;
        int paddedHeight   = 0;
        int trimRight      = 0;
        int trimBottom     = 0;

        if(streamType == OB_STREAM_IR_LEFT || streamType == OB_STREAM_IR_RIGHT || streamType == OB_STREAM_IR) {
            if(originalWidth == 848) {
                paddedWidth = (originalWidth + 48);
            }
            else if(originalWidth == 424) {
                paddedWidth = (originalWidth + 24);
            }
            else if((originalWidth == 480) && (originalHeight == 270)) {
                paddedWidth = (originalWidth + 32);
            }

            paddedHeight = originalHeight;

            // Calculate the number of pixels to fill
            trimRight  = paddedWidth - originalWidth;
            trimBottom = paddedHeight - originalHeight;
            // LOG_DEBUG("-SpecialResolution paddedWidth:{}, paddedHeight:{}, trimRight:{}, trimBottom:{}", paddedWidth, paddedHeight, trimRight, trimBottom);
            try {
                cropDepthImage(srcData, videoFrame->getDataMutable(), paddedWidth, paddedHeight, trimRight, trimBottom);
                videoFrame->setDataSize(originalWidth * originalHeight);
                return;
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
            paddedHeight = originalHeight;

            // Calculate the number of pixels to fill
            trimRight  = paddedWidth - originalWidth;
            trimBottom = paddedHeight - originalHeight;
            // LOG_DEBUG("-SpecialResolution paddedWidth:{}, paddedHeight:{}, trimRight:{}, trimBottom:{}", paddedWidth, paddedHeight, trimRight, trimBottom);

            try {
                cropDepthImage16(reinterpret_cast<const uint16_t *>(srcData), reinterpret_cast<uint16_t *>(videoFrame->getDataMutable()), paddedWidth,
                                 paddedHeight, trimRight, trimBottom);
                videoFrame->setDataSize(originalWidth * originalHeight * 2);
                return;
            }
            catch(const std::exception &e) {
                // Handle exceptions
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }
    videoFrame->updateData(srcData, srcSize);
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
        if(prof->getWidth() == videoProfile->getWidth() && prof->getHeight() == videoProfile->getHeight() && prof->getFps() == videoProfile->getFps()
           && prof->getFormat() == videoProfile->getFormat()) {
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

        fmt.fmt.pix.width       = 1482175047;
        fmt.fmt.pix.height      = 96;
        fmt.fmt.pix.pixelformat = 0;

        if(xioctlGmsl(devHandle->metadataFd, VIDIOC_S_FMT, &fmt) >= 0) {
            LOG_DEBUG("-metadata-Set metadata format to {}", fmt.fmt.pix.pixelformat);
            set_format_success = true;
        }

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
                devHandle->buffers[i].ptr    = static_cast<uint8_t *>(malloc(_length));
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
        if(!devHandle->profile || !(*devHandle->profile == *videoProfile)) {
            LOG_DEBUG("-deviceHandles devHandle->profile:{}, profile:{}, continue...", devHandle->profile, profile);
            continue;
        }
        LOG_DEBUG("-deviceHandles- devHandle->isCapturing:{}, devHandle->profile:{}, profile:{}", devHandle->isCapturing, devHandle->profile, profile);

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

#define BASE_WAIT_RESPONSE_TIME_US 300
uint32_t ObV4lGmslDevicePort::sendAndReceive(const uint8_t *send, uint32_t sendLen, uint8_t *recv, uint32_t exceptedRecvLen) {
    std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
    if(!sendData(send, sendLen)) {
        return -1;
    }
    utils::sleepUs(BASE_WAIT_RESPONSE_TIME_US);
    if(!recvData(recv, &exceptedRecvLen)) {
        return -1;
    }
    return exceptedRecvLen;
}

bool ObV4lGmslDevicePort::sendData(const uint8_t *data, const uint32_t dataLen) {
    VALIDATE_NOT_NULL(data);

    uint16_t opcode, nId = 0;
    // , halfWordSize = 0, magic = 0;
    uint32_t propertyId = 0, alignDataLen = 0, alignI2CDataLen = 0, ctrl = 0;
    uint8_t  mI2cPackDataLen = 0;
    // , mI2cPackLen = 0;
    bool ret = false;

    // LOG_DEBUG("-Entry sendData-dataLen:{} ", dataLen);
    if(alignDataLen >= OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX) {
        alignDataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else {
        alignDataLen = dataLen;
    }

    {
        opcode = ((ProtocolHeader *)(data))->opcode;
        nId    = ((ProtocolHeader *)(data))->nId;
        // halfWordSize = ((ProtocolHeader *)(data))->halfWordSize;
        // magic        = ((ProtocolHeader *)(data))->magic;
        // LOG_DEBUG("------------------------------------------------------------------------");

        uint8_t *pDataBuf = ((uint8_t *)(data)) + sizeof(ProtocolHeader);
        propertyId        = *(uint32_t *)pDataBuf;
        // LOG_DEBUG("sendData opcode:{}, nId:{}, halfWordSize:{}, magic:0x{:0x}, PropertyId:{}", opcode, nId, halfWordSize, magic, propertyId);

        mI2cPackDataLen = dataLen - sizeof(ProtocolHeader);  //-sizeof(uint16_t);
                                                             // mI2cPackLen     = mI2cPackDataLen + sizeof(i2c_msg_header_t);
        // LOG_DEBUG("sendData mI2cPackDataLen:{}, alignDataLen:{}, mI2cPackLen:{} ", mI2cPackDataLen, alignDataLen, mI2cPackLen);

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
            // LOG_DEBUG("sendData alignI2CDataLen:{} ", alignI2CDataLen);
            i2c_msg_t send_i2c_pack_msg;
            memset(&send_i2c_pack_msg, 0, sizeof(i2c_msg_t));
            send_i2c_pack_msg.header.len   = alignI2CDataLen;  // G2R_GET_VERSION_CMD_LEN;
            send_i2c_pack_msg.header.code  = opcode;           // G2R_GET_VERSION_CMD_CODE;
            send_i2c_pack_msg.header.index = nId;              // inde++;
            // memcpy(send_i2c_pack_msg._data, pDataBuf, mI2cPackDataLen);
            std::memcpy(send_i2c_pack_msg._data, pDataBuf, mI2cPackDataLen);

            if((mI2cPackDataLen <= 2) && (mI2cPackDataLen > 0)) {
                propertyId = *(uint16_t *)send_i2c_pack_msg._data;
                // LOG_DEBUG("sendData PropertyId:{} ", propertyId);
                // LOG_DEBUG("sendData 04data-PropertyId dat0:{}, data1:{} ", send_i2c_pack_msg._data[0], send_i2c_pack_msg._data[1]);
            }
            else if(mI2cPackDataLen >= 4) {
                propertyId = *(uint32_t *)send_i2c_pack_msg._data;
                // LOG_DEBUG("sendData PropertyId:{} ", propertyId);
                // LOG_DEBUG("sendData 4data-PropertyId dat0:{}, data1:{}, data2:{}, data3:{} ", send_i2c_pack_msg._data[0], send_i2c_pack_msg._data[1],
                // send_i2c_pack_msg._data[2], send_i2c_pack_msg._data[3]);
            }

            if(propertyId == 202) {  // OB_PROP_DEVICE_REPOWER_BOOL
                int value = *(uint8_t *)(send_i2c_pack_msg._data + 4);
                LOG_DEBUG("sendData value:{} ", value);
                if(value == 1) {
                    // LOG_DEBUG("sendData PropertyId:{} -not need read i2c response status. handle resetGmslDriver.", propertyId);
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
            // LOG_DEBUG("--------------------------------------------------------------------------");

            // alignDataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;  //252; //172;  GMSL I2C read 252 bytes/per
            ctrl = G2R_CAMERA_CID_SET_DATA;
            ret  = setXuExt(ctrl, (uint8_t *)(&send_i2c_pack_msg), alignI2CDataLen);
            // LOG_DEBUG("-Leave ObV4lGmslDevicePort::sendData ret:{} ", ret);
            return ret;
            // return setXuExt(ctrl, data, alignDataLen);
        }
    }

    // LOG_DEBUG("-Leave ObV4lGmslDevicePort::sendData ret:{} ", ret);
    return ret;
}

bool ObV4lGmslDevicePort::recvData(uint8_t *data, uint32_t *dataLen) {

    // LOG_DEBUG("-Entry recvData-dataLen:{0}", *dataLen);
    uint32_t ctrl = 0;

    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(dataLen);

    if(*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_16) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_16;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_16) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_32)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_32;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_32) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_64)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_64;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_64) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_128)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_128;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_128) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_192)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_192;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_192) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_256)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else if((*dataLen > OB_GMSL_FW_I2C_DATA_LEN_256) && (*dataLen <= OB_GMSL_FW_I2C_DATA_LEN_512)) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else if(*dataLen > OB_GMSL_FW_I2C_DATA_LEN_512) {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }
    else {
        *dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
    }

    // LOG_DEBUG("-PuRaw send read data *dataLen:{}", *dataLen);
    // note fw read data len
    {
        //*dataLen = OB_GMSL_FW_I2C_DATA_LEN_CUR_MAX;
        // setPuRaw(G2R_CAMERA_CID_SET_DATA_LEN, G2R_GET_VERSION_DATA_LEN);
        *dataLen += 10;  //*dataLen(real data len) + packhead(8) +propid(2)
        // LOG_DEBUG("-PuRaw send read data cal(*dataLen+10) real *dataLen:{}", *dataLen);
        setPuRaw(G2R_CAMERA_CID_SET_DATA_LEN, *dataLen);
        ctrl = G2R_CAMERA_CID_GET_DATA;
    }

    return getXuExt(ctrl, data, dataLen);
}

bool ObV4lGmslDevicePort::setPu(uint32_t propertyId, int32_t value) {
    auto fd = deviceHandles_.front()->fd;
    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPu propertyId={}, value:{}, devNode:{}", propertyId, value, deviceHandles_[getPuDevIndex()]->info->name);
    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::setPu propertyId={}, value:{}, devNode-front():{}", propertyId, value, deviceHandles_.front()->info->name);

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
    {
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        if(xioctlGmsl(fd, VIDIOC_S_CTRL, &control) < 0) {
            LOG_ERROR("set {0} xioctlGmsl(VIDIOC_S_CTRL) failed, {1}", propertyId, strerror(errno));
            return false;
        }
    }

    // LOG_DEBUG("Leave ObV4lGmslDevicePort::setPu Success! propertyId={}, cid={}, value:{} ", propertyId, cid, value);
    return true;
}

bool ObV4lGmslDevicePort::getPu(uint32_t propertyId, int32_t &value) {
    auto fd = deviceHandles_.front()->fd;

    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::getPu propertyId={}, devNode:{}", propertyId, deviceHandles_.front()->info->name);

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

    {
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        if(xioctlGmsl(fd, VIDIOC_G_CTRL, &control) < 0) {
            LOG_ERROR("get {0} xioctlGmsl(VIDIOC_G_CTRL) failed, {1}", propertyId, strerror(errno));
            return false;
        }
    }

    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;
    }
    value = control.value;
    // LOG_DEBUG("Leave ObV4lGmslDevicePort getPu Success! propertyId={}, value: {} ", propertyId, value);

    return true;
}

UvcControlRange ObV4lGmslDevicePort::getPuRange(uint32_t propertyId) {
    auto fd = deviceHandles_.front()->fd;
    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::getPuRange propertyId={}, devNode:{}", propertyId, deviceHandles_.front()->info->name);

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
    {
        std::unique_lock<std::mutex> lk(mMultiThreadI2CMutex);
        if(xioctlGmsl(fd, VIDIOC_QUERYCTRL, &query) < 0) {
            query.minimum = query.maximum = 0;
        }
    }

    UvcControlRange range(query.minimum, query.maximum, query.step, query.default_value);
    // LOG_DEBUG("Leave ObV4lGmslDevicePort::getPuRange propertyId:{}, query.minimum:{}, query.maximum:{}, query.step:{}, query.default_value:{} ", propertyId,
    //           query.minimum, query.maximum, query.step, query.default_value);
    return range;
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

bool ObV4lGmslDevicePort::setXuExt(uint32_t ctrl, const uint8_t *data, uint32_t len) {
    VALIDATE_NOT_NULL(data);
    (void)len;
    auto fd  = deviceHandles_.front()->fd;
    auto cid = ctrl;  // CIDFromOBPropertyID(ctrl);

    // i2c_msg_t *msg        = (i2c_msg_t *)data;
    // uint32_t   propertyId = *(uint32_t *)msg->_data;
    // LOG_DEBUG("-Entry setXuExt PropertyId:{}, len:{}, ctrl:{}", propertyId, ctrl, len);

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
            break;//TODO check signed/unsigned
        case 4:
            xctrl.value = *reinterpret_cast<const int32_t *>(data);
            break;
        case 8:
            xctrl.value64 = *reinterpret_cast<const int64_t *>(data);
            break;
        default:
            xctrl.p_u8 = const_cast<uint8_t *>(data);//TODO aggregate initialization with union
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

    // LOG_DEBUG("-Entry ObV4lGmslDevicePort::getXuExt-ctrl:{}, *len:{} ", ctrl, *len);

    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(len);
    auto                    fd  = deviceHandles_.front()->fd;
    auto                    cid = ctrl;  // CIDFromOBPropertyID(ctrl);
    struct v4l2_ext_control control {
        cid, G2R_RW_DATA_LEN, 0, 0
    };
    std::vector<uint8_t> dataRecvBuf(MAX_I2C_PACKET_SIZE, 0);
    control.p_u8 = dataRecvBuf.data();
    v4l2_ext_controls ext{ control.id & 0xffff0000, 1, 0, 0, 0, &control };

    // the ioctl fails once when performing send and receive right after it
    // it succeeds on the second time
    const int MAX_TRIES       = 200;
    const int TRY_INTERVAL_MS = 10;
    int       tries           = 0;
    while(++tries < MAX_TRIES) {
        std::fill(dataRecvBuf.begin(), dataRecvBuf.end(), 0);
        int ret = xioctlGmsl(fd, VIDIOC_G_EXT_CTRLS, &ext);
        if(ret < 0) {
            utils::sleepMs(TRY_INTERVAL_MS);
            // exception is thrown if the ioctl fails twice
            continue;
        }

        if(ctrl == OB_PROP_COLOR_AUTO_EXPOSURE_BOOL)
            control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;

        if(*len < sizeof(__s64)) {
            memcpy(data, (void *)(&control.value), *len);
        }
        else if(ctrl == G2R_CAMERA_CID_GET_DATA) {
            auto *pRecvDataBuf = reinterpret_cast<i2c_msg_t *>(&dataRecvBuf[0]);
            // LOG_DEBUG("--------------------------------------------------------------------------");
            // LOG_DEBUG("recvData resp code:  {} ", std::to_string(pRecvDataBuf->header.code));
            // LOG_DEBUG("recvData resp index: {} ", std::to_string(pRecvDataBuf->header.index));
            // LOG_DEBUG("recvData resp len:   {} ", std::to_string(pRecvDataBuf->header.len));
            // LOG_DEBUG("recvData resp res:   {} ", std::to_string(pRecvDataBuf->body.res));
            // LOG_DEBUG("--------------------------------------------------------------------------");

            uint16_t readRespDataSize = (pRecvDataBuf->header.len - sizeof(pRecvDataBuf->header) - sizeof(pRecvDataBuf->body.res));
            // handle  pRecvDataBuf->header.len==0 status exception
            if((pRecvDataBuf->header.len == 0) || (pRecvDataBuf->header.code == 0) || (pRecvDataBuf->header.len == 65535) || (pRecvDataBuf->header.len > 248)) {
                readRespDataSize = 0;
                // LOG_DEBUG("I2C read data err!. pRecvDataBuf->header.len:{}, pRecvDataBuf->header.code:{}, tries:{}",
                // std::to_string(pRecvDataBuf->header.len),
                //           std::to_string(pRecvDataBuf->header.code), tries);

                utils::sleepMs(TRY_INTERVAL_MS);

                continue;
            }

            // repeat pack usb protocolHeader
            ProtocolMsg usbProtocolMsg;
            memset(&usbProtocolMsg, 0, sizeof(usbProtocolMsg));
            usbProtocolMsg.header.opcode       = pRecvDataBuf->header.code;
            usbProtocolMsg.header.nId          = pRecvDataBuf->header.index;  // return I2C resp index
            usbProtocolMsg.header.magic        = 0x4252;                      // HP_RESPONSE_MAGIC;
            usbProtocolMsg.buf.data.resp.res   = pRecvDataBuf->body.res;
            usbProtocolMsg.header.halfWordSize = (readRespDataSize + sizeof(usbProtocolMsg.buf.data.resp.res) + 1) / sizeof(uint16_t);
            usbProtocolMsg.buf.len             = readRespDataSize + sizeof(usbProtocolMsg.buf.data.resp.res);

            // LOG_DEBUG("cal readRespDataSize: {} ", readRespDataSize);
            // LOG_DEBUG("cal usbProtocolMsg.header.halfWordSize: {} ", std::to_string(usbProtocolMsg.header.halfWordSize));
            // LOG_DEBUG("cal readRespDataSize:    {} ", readRespDataSize);
            // LOG_DEBUG("cal recal usbProtocolMsg.buf.len:    {}", std::to_string(usbProtocolMsg.buf.len));

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

            *len = sizeof(usbProtocolMsg.header) + usbProtocolMsg.buf.len + sizeof(usbProtocolMsg.buf.len);
            // LOG_DEBUG("-copy_usbProtocolMsg *len: {}", *len);
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
    if(errno == EIO || errno == EAGAIN) {
        return false;
    }
    throw io_exception("set ctrl:" + std::to_string(ctrl) + "xioctlGmsl(VIDIOC_G_EXT_CTRLS) failed! err:" + strerror(errno)
                       + " tries:" + std::to_string(tries));
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

// bus_info:platform:tegra-capture-vi:0
#define GMSL_MIPI_DEVICE_TAG "platform:tegra-capture-vi"
bool is_gmsl_mipi_device(const std::string bus_info) {
    return bus_info.find(GMSL_MIPI_DEVICE_TAG) != std::string::npos;
}

bool isGmslMipiDeviceForNvidia(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    return (portInfo->hubId.find(GMSL_MIPI_DEVICE_TAG) != std::string::npos) && (portInfo->connSpec == "GMSL2");
}

bool ObV4lGmslDevicePort::isGmslDeviceForPlatformNvidia(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    return isGmslMipiDeviceForNvidia(portInfo);
}

void getV4lDeviceBusInfo(const std::string &dev_name, std::string &bus_info, std::string &card) {
    struct v4l2_capability vcap;
    int                    fd = -1;

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

int file_exists(const char *filename) {
    return (access(filename, F_OK) != -1);
}

std::vector<std::string> getVideoPaths(std::vector<std::string> &video_paths) {
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
                continue;
            }
        }

        if(real_path.empty()) {
            continue;
        }

        video_paths.push_back(real_path);
    }
    closedir(dir);

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
    }
    return video_paths;
}

bool isUsbDevicePath(const std::string &video_path) {
    static const std::regex uvc_pattern("(\\/usb\\d+\\/)\\w+");  // Locate UVC device path pattern ../usbX/...
    return std::regex_search(video_path, uvc_pattern);
}

const std::vector<UsbInterfaceInfo> ObV4lGmslDevicePort::queryDevicesInfo() {
    std::vector<std::string> video_paths;
    getVideoPaths(video_paths);
    if(video_paths.empty()) {
        return {};
    }

    std::vector<UsbInterfaceInfo> devInfoList;

    for(auto &&video_path: video_paths) {
        if(isUsbDevicePath(video_path)) {
            continue;
        }

        auto name    = video_path.substr(video_path.find_last_of('/') + 1);  // from "/sys/class/video4linux/videoX" to get "videoX"
        auto devName = "/dev/" + name;

        std::string bus_info, card;
        getV4lDeviceBusInfo(devName, bus_info, card);

        struct orbbec_device_info devInfo;
        memset(&devInfo, 0, sizeof(orbbec_device_info));
        int res = getGmslDeviceInfoFromFW(devName, &devInfo);
        if(res != 0) {
            LOG_DEBUG("getGmslDeviceInfoFromFW failed! devName:{}", devName);
            continue;
        }

        UsbInterfaceInfo info{};
        info.vid              = GMSL_VID_ORBBEC;
        info.pid              = devInfo.pid;
        info.infName          = devName;
        info.infUrl           = video_path;
        info.infNameDescIndex = 0;  // unsupported
        info.serial           = reinterpret_cast<char *>(devInfo.sn);
        info.hubId            = bus_info;
        info.url              = "gmsl2://" + std::to_string(info.pid) + ":" + std::to_string(info.vid) + "/1/" + std::to_string(devInfo.cam_num);
        info.uid              = "gmsl2-" + std::to_string(devInfo.cam_num);
        info.conn_spec        = gmsl2_type;
        info.cls              = OB_USB_CLASS_VIDEO;  // borrowed from usb
        if(devInfo.video_type == ORB_MUX_PAD_DEPTH) {
            info.infIndex = INTERFACE_DEPTH;
        }
        else if(devInfo.video_type == ORB_MUX_PAD_RGB) {
            info.infIndex = INTERFACE_COLOR;
        }
        else if(devInfo.video_type == ORB_MUX_PAD_IR_L) {
            info.infIndex = INTERFACE_IR_LEFT;
        }
        else if(devInfo.video_type == ORB_MUX_PAD_IR_R) {
            info.infIndex = INTERFACE_IR_RIGHT;
        }
        else {
            continue;
            LOG_DEBUG("Unsupported video type:{}", devInfo.video_type);
        }
        devInfoList.push_back(info);
        LOG_DEBUG("Gmsl video port found! devName:{}, infName:{}", devName, info.infName);

        if(info.infIndex == INTERFACE_DEPTH) {
            UsbInterfaceInfo imuInfo = info;
            imuInfo.cls              = OB_USB_CLASS_HID;  // borrowed from usb
            imuInfo.infName          = "/dev/v4l-subdev" + std::to_string(devInfo.sub_num);
            devInfoList.push_back(imuInfo);
            LOG_DEBUG("Gmsl hid device found! devName:{}, infName:{}", devName, imuInfo.infName);
        }
    }

    LOG_DEBUG("video queryDevicesInfo devInfoList.size():{}", devInfoList.size());
    LOG_DEBUG("queryDevicesInfo done!");
    return devInfoList;
}
}  // namespace libobsensor