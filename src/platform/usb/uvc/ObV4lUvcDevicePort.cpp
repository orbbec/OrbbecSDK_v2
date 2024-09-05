#include <fcntl.h>
#include <unistd.h>
#include <limits>
#include <algorithm>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "ObV4lUvcDevicePort.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

#define USE_MEMORY_MMAP true

static std::map<uint32_t, uint32_t> v4lFourccMap = {
    { 0x47524559, 0x59382020 }, /* 'GREY' to 'Y8  ' */
    { 0x48455643, 0x48323635 }, /* 'HEVC' to 'H265' */
};

int xioctl(int fh, unsigned long request, void *arg) {
    int ret   = 0;
    int retry = 5;
    do {
        ret = ioctl(fh, request, arg);
    } while(ret < 0 && (errno == EINTR || errno == EAGAIN) && retry--);
    return ret;
}

v4l2_capability getV4l2DeviceCapabilities(const std::string &dev_name) {
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
        LOG_ERROR("getV4l2DeviceCapabilities: Cannot open {}", dev_name);
        return cap;
    }

    if(xioctl(*fd, VIDIOC_QUERYCAP, &cap) < 0) {
        if(errno == EINVAL) {
            LOG_ERROR("getV4l2DeviceCapabilities {} is no V4L2 device", dev_name);
        }
        else {
            LOG_ERROR("getV4l2DeviceCapabilities  xioctl(VIDIOC_QUERYCAP) failed!");
        }
    }
    return cap;
}

std::vector<std::shared_ptr<V4lDeviceInfo>> ObV4lUvcDevicePort::queryRelatedDevices(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    std::vector<std::shared_ptr<V4lDeviceInfo>> devs;
    DIR                                        *dir = opendir("/sys/class/video4linux");
    if(!dir) {
        LOG_DEBUG("Failed to open /sys/class/video4linux, possibly no device connected");
        return devs;
    }

    struct dirent *entry;
    while((entry = readdir(dir))) {
        std::string name = entry->d_name;
        if(name == "." || name == ".." || name.find("video") == std::string::npos) {
            continue;
        }
        LOG_DEBUG("Found video4linux: {}", name);
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
            uint16_t    vid, pid, mi;
            std::string busNum, devNum, devPath;
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
            auto url = busNum + "-" + devPath + "-" + devNum;
            if(portInfo->vid == vid && portInfo->pid == pid && portInfo->infIndex == mi && portInfo->url == url) {

                // Find the USB specification (USB2/3) type from the underlying device
                // Use device mapping obtained in previous step to traverse node tree
                // and extract the required descriptors
                // Traverse from
                // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/3-6:1.0/video4linux/video0
                // to
                // /sys/devices/pci0000:00/0000:00:xx.0/ABC/M-N/version
                auto info  = std::make_shared<V4lDeviceInfo>();
                info->name = devname;
                info->cap  = getV4l2DeviceCapabilities(devname);
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
    std::sort(devs.begin(), devs.end(), [](const std::shared_ptr<V4lDeviceInfo> &a, const std::shared_ptr<V4lDeviceInfo> &b) {
        int numA = std::stoi(a->name.substr(10));
        int numB = std::stoi(b->name.substr(10));
        return numA < numB;
    });
    return devs;
}

bool ObV4lUvcDevicePort::isContainedMetadataDevice(std::shared_ptr<const USBSourcePortInfo> portInfo) {
    auto devs = queryRelatedDevices(portInfo);
    for(auto &dev: devs) {
        if(dev->cap.device_caps & V4L2_CAP_META_CAPTURE) {
            return true;
        }
    }
    return false;
}

void foreachProfile(std::vector<std::shared_ptr<V4lDeviceHandle>>                                              deviceHandles,
                    std::function<bool(std::shared_ptr<V4lDeviceHandle>, std::shared_ptr<VideoStreamProfile>)> func) {
    bool quit = false;
    for(auto &devHandle: deviceHandles) {
        if(quit) {
            break;
        }
        // enum format
        v4l2_fmtdesc pixel_format = {};
        pixel_format.type         = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while(!quit && xioctl(devHandle->fd, VIDIOC_ENUM_FMT, &pixel_format) == 0) {
            v4l2_frmsizeenum frame_size = {};
            frame_size.pixel_format     = pixel_format.pixelformat;
            uint32_t fourcc             = (const utils::big_endian<int> &)pixel_format.pixelformat;
            if(v4lFourccMap.count(fourcc)) {
                fourcc = v4lFourccMap.at(fourcc);
            }

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
                LOG_DEBUG("Recognized pixel-format {}", (char *)pixel_format.description);
            }
            // enum format params
            while(!quit && xioctl(devHandle->fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0) {
                v4l2_frmivalenum frame_interval = {};
                frame_interval.pixel_format     = pixel_format.pixelformat;
                frame_interval.width            = frame_size.discrete.width;
                frame_interval.height           = frame_size.discrete.height;
                while(!quit && xioctl(devHandle->fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0) {
                    if(frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if(frame_interval.discrete.numerator != 0) {
                            auto format = utils::uvcFourccToOBFormat(fourcc);
                            if(format != OB_FORMAT_UNKNOWN) {
                                auto width   = frame_size.discrete.width;
                                auto height  = frame_size.discrete.height;
                                auto fps     = static_cast<float>(frame_interval.discrete.denominator) / static_cast<float>(frame_interval.discrete.numerator);
                                auto profile = std::make_shared<VideoStreamProfile>(std::shared_ptr<LazySensor>(), OB_STREAM_VIDEO, format, width, height, fps);
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

uint32_t CIDFromOBPropertyID(OBPropertyID id) {
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

std::string fourccToString(uint32_t id) {
    uint32_t device_fourcc = id;
    char     fourcc_buff[sizeof(device_fourcc) + 1];
    std::memcpy(fourcc_buff, &device_fourcc, sizeof(device_fourcc));
    fourcc_buff[sizeof(device_fourcc)] = 0;
    return fourcc_buff;
}

ObV4lUvcDevicePort::ObV4lUvcDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo) : portInfo_(portInfo) {
    auto devs = queryRelatedDevices(portInfo_);
    if(devs.empty()) {
        throw libobsensor::camera_disconnected_exception("No v4l device found for port: " + portInfo_->infUrl);
    }

    std::shared_ptr<V4lDeviceHandle> devHandle = nullptr;
    auto                             iter      = devs.begin();
    while(iter != devs.end()) {
        if(((*iter)->cap.device_caps & V4L2_CAP_META_CAPTURE) && devHandle != nullptr) {
            devHandle->metadataInfo = *iter;
            auto fd                 = open(devHandle->metadataInfo->name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(fd < 0) {
                LOG_ERROR("Failed to open metadata dev: {}", devHandle->metadataInfo->name);
                continue;
            }
            LOG_DEBUG("Opened metadata dev: {}", devHandle->info->name);
            devHandle->metadataFd = fd;
        }
        else {
            devHandle             = std::make_shared<V4lDeviceHandle>();
            devHandle->metadataFd = -1;
            devHandle->info       = *iter;
            auto fd               = open(devHandle->info->name.c_str(), O_RDWR | O_NONBLOCK, 0);
            if(fd < 0) {
                throw libobsensor::io_exception("Failed to open: " + devHandle->info->name);
            }
            devHandle->fd = fd;
            LOG_DEBUG("Opened: {}", devHandle->info->name);
            deviceHandles_.push_back(devHandle);
        }
        iter++;
    }
    if(deviceHandles_.empty()) {
        throw libobsensor::camera_disconnected_exception("No v4l device found for port: " + portInfo_->infUrl);
    }

    LOG_DEBUG("V4L device port created for {} with {} v4l2 device", portInfo_->infUrl, deviceHandles_.size());
}

ObV4lUvcDevicePort::~ObV4lUvcDevicePort() noexcept {
    try {
        stopAllStream();
    }
    catch(const std::exception &ex) {
        LOG_ERROR(ex.what());
    }
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
}

void ObV4lUvcDevicePort::captureLoop(std::shared_ptr<V4lDeviceHandle> devHandle) {
    int metadataBufferIndex = -1;
    try {

        int max_fd = std::max({ devHandle->fd, devHandle->metadataFd, devHandle->stopPipeFd[0], devHandle->stopPipeFd[1] });

        if(devHandle->metadataFd >= 0) {
            v4l2_buffer buf = {};
            buf.type        = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            xioctl(devHandle->metadataFd, VIDIOC_QBUF, &buf);
        }

        if(devHandle->fd >= 0) {
            v4l2_buffer buf = {};
            buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory      = V4L2_MEMORY_MMAP;
            xioctl(devHandle->fd, VIDIOC_QBUF, &buf);
        }

        while(devHandle->isCapturing) {
            fd_set fds{};
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
            struct timeval remaining = { 0, 500000 };  // 500ms
            int            val       = select(max_fd + 1, &fds, nullptr, nullptr, &remaining);
            if(val < 0) {
                if(errno == EINTR) {
                    LOG_DEBUG("select interrupted: {}", strerror(errno));
                }
                else {
                    LOG_DEBUG("select failed: {}", strerror(errno));
                }
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
                v4l2_buffer buf = {};
                buf.type        = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                if(xioctl(devHandle->metadataFd, VIDIOC_DQBUF, &buf) < 0) {
                    LOG_DEBUG("VIDIOC_DQBUF failed, {}, {}", strerror(errno), devHandle->metadataInfo->name);
                }
                if(buf.bytesused) {
                    devHandle->metadataBuffers[buf.index].actual_length = buf.bytesused;
                    devHandle->metadataBuffers[buf.index].sequence      = buf.sequence;
                    metadataBufferIndex                                 = buf.index;
                }

                if(devHandle->isCapturing) {
                    xioctl(devHandle->metadataFd, VIDIOC_QBUF, &buf);
                }
            }

            if(FD_ISSET(devHandle->fd, &fds)) {
                FD_CLR(devHandle->fd, &fds);
                v4l2_buffer buf = {};
                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                // reader buffer
                if(xioctl(devHandle->fd, VIDIOC_DQBUF, &buf) < 0) {
                    LOG_DEBUG("VIDIOC_DQBUF failed, {}, {}", strerror(errno), devHandle->info->name);
                }

                if(buf.bytesused) {
                    auto timestamp = (double)buf.timestamp.tv_sec * 1000.f + (double)buf.timestamp.tv_usec / 1000.f;
                    (void)timestamp;

                    auto rawframe   = FrameFactory::createFrameFromStreamProfile(devHandle->profile);
                    auto videoFrame = rawframe->as<VideoFrame>();
                    videoFrame->updateData(static_cast<const uint8_t *>(devHandle->buffers[buf.index].ptr), buf.bytesused);
                    if(metadataBufferIndex >= 0 && devHandle->metadataBuffers[metadataBufferIndex].sequence == buf.sequence) {
                        auto uvc_payload_header     = devHandle->metadataBuffers[metadataBufferIndex].ptr + sizeof(V4L2UvcMetaHeader);
                        auto uvc_payload_header_len = devHandle->metadataBuffers[metadataBufferIndex].actual_length - sizeof(V4L2UvcMetaHeader);
                        if(uvc_payload_header_len >= sizeof(StandardUvcFramePayloadHeader)) {
                            auto payloadHeader = (StandardUvcFramePayloadHeader *)uvc_payload_header;
                            videoFrame->updateMetadata(static_cast<const uint8_t *>(payloadHeader->scrSourceClock),
                                                       sizeof(StandardUvcFramePayloadHeader::scrSourceClock));
                            videoFrame->appendMetadata(static_cast<const uint8_t *>(uvc_payload_header + sizeof(StandardUvcFramePayloadHeader)),
                                                       uvc_payload_header_len - sizeof(StandardUvcFramePayloadHeader));
                            videoFrame->setTimeStampUsec(payloadHeader->dwPresentationTime);
                        }
                    }

                    auto realtime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    videoFrame->setSystemTimeStampUsec(realtime);
                    videoFrame->setNumber(buf.sequence);
                    devHandle->frameCallback(videoFrame);
                }

                if(devHandle->isCapturing) {
                    xioctl(devHandle->fd, VIDIOC_QBUF, &buf);
                }
            }
        }
    }
    catch(const std::exception &ex) {
        LOG_ERROR(ex.what());
    }
}

StreamProfileList ObV4lUvcDevicePort::getStreamProfileList() {
    StreamProfileList profileList;
    foreachProfile(deviceHandles_, [&profileList](std::shared_ptr<V4lDeviceHandle> devHandle, std::shared_ptr<VideoStreamProfile> profile) {
        (void)devHandle;
        profileList.push_back(profile);
        return false;  // false means continue
    });
    return profileList;
}

uint32_t phaseProfileFormatToFourcc(std::shared_ptr<const VideoStreamProfile> profile) {
    int      formatFourcc    = 0;
    OBFormat format          = profile->getFormat();
    auto     foundFormatIter = std::find_if(v4lFourccMap.begin(), v4lFourccMap.end(),
                                            [&](const std::pair<uint32_t, uint32_t> &item) { return item.second == utils::obFormatToUvcFourcc(format); });
    if(foundFormatIter != v4lFourccMap.end()) {
        return (const utils::big_endian<int> &)(foundFormatIter->first);
    }

    formatFourcc = utils::obFormatToUvcFourcc(format);
    if(formatFourcc == 0) {
        LOG_ERROR("unsupported format {}", profile->getFormat());
        return 0;
    }

    return (const utils::big_endian<int> &)(formatFourcc);
}

void ObV4lUvcDevicePort::startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    std::shared_ptr<V4lDeviceHandle> devHandle    = nullptr;
    auto                             videoProfile = profile->as<VideoStreamProfile>();
    foreachProfile(deviceHandles_, [&](std::shared_ptr<V4lDeviceHandle> handle, std::shared_ptr<VideoStreamProfile> prof) {
        if(prof->getWidth() == videoProfile->getWidth() && prof->getHeight() == videoProfile->getHeight() && prof->getFps() == videoProfile->getFps()
           && prof->getFormat() == videoProfile->getFormat()) {
            devHandle = handle;
            return true;
        }
        return false;
    });
    if(!devHandle) {
        throw libobsensor::pal_exception("No v4l device found for profile: width=" + std::to_string(videoProfile->getWidth())
                                         + ", height=" + std::to_string(videoProfile->getHeight()) + ", fps=" + std::to_string(videoProfile->getFps())
                                         + ", format=" + std::to_string(videoProfile->getFormat()));
    }
    if(devHandle->isCapturing) {
        throw libobsensor::pal_exception("V4l device is already capturing");
    }

    if(devHandle->metadataFd >= 0) {
        v4l2_format fmt = {};
        fmt.type        = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
        if(xioctl(devHandle->metadataFd, VIDIOC_G_FMT, &fmt) < 0) {
            throw libobsensor::io_exception("Failed to get metadata format! " + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
        if(fmt.type != LOCAL_V4L2_BUF_TYPE_META_CAPTURE) {
            throw libobsensor::io_exception("Invalid metadata type!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }

        const std::vector<uint32_t> requires_formats   = { LOCAL_V4L2_META_FMT_D4XX, V4L2_META_FMT_UVC };
        bool                        set_format_success = false;
        for(auto required_format: requires_formats) {
            memcpy(fmt.fmt.raw_data, &required_format, sizeof(required_format));
            if(xioctl(devHandle->metadataFd, VIDIOC_S_FMT, &fmt) >= 0) {
                LOG_DEBUG("Set metadata format to {}", fourccToString(required_format));
                set_format_success = true;
                break;
            }
        }
        if(!set_format_success) {
            throw libobsensor::io_exception("Failed to set metadata format!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }

        struct v4l2_requestbuffers req = {};
        req.count                      = MAX_BUFFER_COUNT;
        req.type                       = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
        req.memory                     = V4L2_MEMORY_MMAP;
        if(xioctl(devHandle->metadataFd, VIDIOC_REQBUFS, &req) < 0) {
            throw libobsensor::io_exception("Failed to request metadata buffers!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
        for(uint32_t i = 0; i < req.count && i < MAX_BUFFER_COUNT; i++) {
            struct v4l2_buffer buf = {};
            buf.type               = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
            buf.memory             = V4L2_MEMORY_MMAP;
            buf.index              = i;  // only one buffer
            if(xioctl(devHandle->metadataFd, VIDIOC_QUERYBUF, &buf) < 0) {
                throw libobsensor::io_exception("Failed to query metadata buffer!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }
            devHandle->metadataBuffers[i].ptr    = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, devHandle->metadataFd, buf.m.offset);
            devHandle->metadataBuffers[i].length = buf.length;
        }

        v4l2_buf_type bufType = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
        if(xioctl(devHandle->metadataFd, VIDIOC_STREAMON, &bufType) < 0) {
            throw libobsensor::io_exception("Failed to stream on metadata!" + devHandle->metadataInfo->name + ", " + strerror(errno));
        }
    }

    v4l2_format fmt         = {};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = videoProfile->getWidth();
    fmt.fmt.pix.height      = videoProfile->getHeight();
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    fmt.fmt.pix.pixelformat = phaseProfileFormatToFourcc(videoProfile);
    if(xioctl(devHandle->fd, VIDIOC_S_FMT, &fmt) < 0) {
        throw libobsensor::io_exception("Failed to set format!" + devHandle->info->name + ", " + strerror(errno));
    }
    if(xioctl(devHandle->fd, VIDIOC_G_FMT, &fmt) < 0) {
        throw libobsensor::io_exception("Failed to get format!" + devHandle->info->name + ", " + strerror(errno));
    }
    LOG_DEBUG("Video node was successfully configured to {0} format, fd {1}", fourccToString(fmt.fmt.pix.pixelformat), devHandle->fd);

    v4l2_streamparm streamparm                       = {};
    streamparm.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator   = 1;
    streamparm.parm.capture.timeperframe.denominator = videoProfile->getFps();
    if(xioctl(devHandle->fd, VIDIOC_S_PARM, &streamparm) < 0) {
        throw libobsensor::io_exception("Failed to set streamparm!" + devHandle->info->name + ", " + strerror(errno));
    }
    if(xioctl(devHandle->fd, VIDIOC_G_PARM, &streamparm) < 0) {
        throw libobsensor::io_exception("Failed to get streamparm!" + devHandle->info->name + ", " + strerror(errno));
    }

    struct v4l2_requestbuffers req = {};
    req.count                      = MAX_BUFFER_COUNT;
    req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory                     = V4L2_MEMORY_MMAP;
    if(xioctl(devHandle->fd, VIDIOC_REQBUFS, &req) < 0) {
        throw libobsensor::io_exception("Failed to request buffers!" + devHandle->info->name + ", " + strerror(errno));
    }
    for(uint32_t i = 0; i < req.count && i < MAX_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        buf.index              = i;
        if(xioctl(devHandle->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            throw libobsensor::io_exception("Failed to query buffer!" + devHandle->info->name + ", " + strerror(errno));
        }
        devHandle->buffers[i].ptr    = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, devHandle->fd, buf.m.offset);
        devHandle->buffers[i].length = buf.length;
    }

    v4l2_buf_type bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(devHandle->fd, VIDIOC_STREAMON, &bufType) < 0) {
        throw libobsensor::io_exception("Failed to stream on!" + devHandle->info->name + ", " + strerror(errno));
    }

    if(pipe(devHandle->stopPipeFd) < 0) {
        throw libobsensor::io_exception("Failed to create stop pipe!" + devHandle->info->name + ", " + strerror(errno));
    }

    devHandle->isCapturing   = true;
    devHandle->profile       = videoProfile;
    devHandle->frameCallback = callback;
    devHandle->captureThread = std::make_shared<std::thread>([devHandle]() { captureLoop(devHandle); });
}

void ObV4lUvcDevicePort::stopStream(std::shared_ptr<const StreamProfile> profile) {
    if(deviceHandles_.empty()) {
        return;
    }

    auto clearUp = [](std::shared_ptr<V4lDeviceHandle> devHandle) {
        // cleanup
        for(uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
            if(devHandle->buffers[i].ptr) {
                munmap(devHandle->buffers[i].ptr, devHandle->buffers[i].length);
                devHandle->buffers[i].ptr    = nullptr;
                devHandle->buffers[i].length = 0;
            }
            if(devHandle->metadataBuffers[i].ptr) {
                munmap(devHandle->metadataBuffers[i].ptr, devHandle->metadataBuffers[i].length);
                devHandle->metadataBuffers[i].ptr    = nullptr;
                devHandle->metadataBuffers[i].length = 0;
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
        if(!devHandle->profile || !(*devHandle->profile == *videoProfile) || !devHandle->isCapturing) {
            continue;
        }

        devHandle->isCapturing = false;
        // signal the capture loop to stop
        char    buff[1] = { 0 };
        ssize_t ret     = write(devHandle->stopPipeFd[1], buff, 1);
        if(ret < 0) {
            throw libobsensor::io_exception("failed to write stop pipe " + std::string(strerror(errno)));
        }

        // wait for the capture loop to stop
        if(devHandle->captureThread && devHandle->captureThread->joinable()) {
            devHandle->captureThread->join();
        }
        devHandle->captureThread.reset();

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(devHandle->fd, VIDIOC_STREAMOFF, &type) < 0) {
            throw libobsensor::io_exception("Failed to stream off!" + devHandle->info->name + ", " + strerror(errno));
        }

        clearUp(devHandle);

        struct v4l2_requestbuffers req = {};
        req.count                      = 0;
        req.type                       = type;
        req.memory                     = V4L2_MEMORY_MMAP;
        if(xioctl(devHandle->fd, VIDIOC_REQBUFS, &req) < 0) {
            throw libobsensor::io_exception("Failed to request buffers!" + devHandle->info->name + ", " + strerror(errno));
        }

        if(devHandle->metadataFd >= 0) {
            type = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;
            if(xioctl(devHandle->metadataFd, VIDIOC_STREAMOFF, &type) < 0) {
                throw libobsensor::io_exception("Failed to stream off metadata!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }
            req.count  = 0;
            req.type   = type;
            req.memory = V4L2_MEMORY_MMAP;
            if(xioctl(devHandle->metadataFd, VIDIOC_REQBUFS, &req) < 0) {
                throw libobsensor::io_exception("Failed to request metadata buffers!" + devHandle->metadataInfo->name + ", " + strerror(errno));
            }
        }
    }
}

void ObV4lUvcDevicePort::stopAllStream() {
    for(auto &devHandle: deviceHandles_) {
        if(devHandle->isCapturing) {
            stopStream(devHandle->profile);
        }
    }
}
uint32_t ObV4lUvcDevicePort::sendAndReceive(const uint8_t *sendData, uint32_t sendLen, uint8_t *recvData, uint32_t exceptedRecvLen) {
    uint8_t ctrl = OB_VENDOR_XU_CTRL_ID_64;

    auto alignDataLen = sendLen;
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

    if(!setXu(ctrl, sendData, alignDataLen)) {
        return 0;
    }

    ctrl = OB_VENDOR_XU_CTRL_ID_512;
    if(exceptedRecvLen <= 64) {
        ctrl = OB_VENDOR_XU_CTRL_ID_64;
    }
    else if(exceptedRecvLen > 512) {
        ctrl = OB_VENDOR_XU_CTRL_ID_1024;
    }
    else {
        ctrl = OB_VENDOR_XU_CTRL_ID_512;
    }

    if(!getXu(ctrl, recvData, &exceptedRecvLen)) {
        return 0;
    }
    return exceptedRecvLen;
}

bool ObV4lUvcDevicePort::getPu(uint32_t propertyId, int32_t &value) {
    auto                fd      = deviceHandles_.front()->fd;
    auto                cid     = CIDFromOBPropertyID(static_cast<OBPropertyID>(propertyId));
    struct v4l2_control control = { cid, 0 };
    if(xioctl(fd, VIDIOC_G_CTRL, &control) < 0) {
        LOG_ERROR("get {0} xioctl(VIDIOC_G_CTRL) failed, {1}", propertyId, strerror(errno));
        return false;
    }

    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = (V4L2_EXPOSURE_MANUAL == control.value) ? 0 : 1;
    }
    value = control.value;

    return true;
}

bool ObV4lUvcDevicePort::setPu(uint32_t propertyId, int32_t value) {
    auto                fd      = deviceHandles_.front()->fd;
    auto                cid     = CIDFromOBPropertyID(static_cast<OBPropertyID>(propertyId));
    struct v4l2_control control = { cid, value };
    if(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL == propertyId) {
        control.value = value ? V4L2_EXPOSURE_AUTO : V4L2_EXPOSURE_MANUAL;
    }
    // We chose not to protect the subscribe / unsubscribe with mutex due to performance reasons,
    // we prefer returning on timeout (and let the retry mechanism try again if exist) than blocking the main thread on every set command

    // RAII to handle unsubscribe in case of exceptions
    std::unique_ptr<uint32_t, std::function<void(uint32_t *)>> unsubscriber(new uint32_t(control.id), [this](const uint32_t *id) {
        if(id) {
            // `unsubscribe_from_ctrl_event()` may throw so we first release the memory allocated and than call it.
            auto local_id = *id;
            delete id;
            unsubscribeFromCtrlEvent(local_id);
        }
    });
    subscribeToCtrlEvent(control.id);
    // Set value
    if(xioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
        LOG_ERROR("set {0} xioctl(VIDIOC_S_CTRL) failed, {1}", propertyId, strerror(errno));
        return false;
    }

    if(!pendForCtrlStatusEvent()) {
        return false;
    }
    return true;
}

UvcControlRange ObV4lUvcDevicePort::getXuRange(uint8_t control, int len) const {
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
    if(xioctl(fd, UVCIOC_CTRL_QUERY, &xquery) < 0) {
        LOG_ERROR("xioctl(VIDIOC_QUERY_EXT_CTRL) failed!");
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
    if(-1 == xioctl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctl(UVC_GET_MIN) failed!");
    }
    range.min.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.min.begin());

    xquery.query    = UVC_GET_MAX;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctl(UVC_GET_MAX) failed!");
    }
    range.max.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.max.begin());

    xquery.query    = UVC_GET_DEF;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctl(UVC_GET_DEF) failed!");
    }
    // def means default
    range.def.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.def.begin());

    xquery.query    = UVC_GET_RES;
    xquery.size     = size;
    xquery.selector = control;
    xquery.unit     = xuUnit_.unit;
    xquery.data     = buf.data();
    if(-1 == xioctl(fd, UVCIOC_CTRL_QUERY, &xquery)) {
        LOG_ERROR("xioctl(UVC_GET_CUR) failed!");
    }
    range.step.resize(buf_size);
    std::copy(buf.begin(), buf.end(), range.step.begin());

    return range;
}

UvcControlRange ObV4lUvcDevicePort::getPuRange(uint32_t propertyId) {
    auto fd = deviceHandles_.front()->fd;
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
    query.id                    = CIDFromOBPropertyID(static_cast<OBPropertyID>(propertyId));
    VALIDATE_GE(fd, 0);
    if(xioctl(fd, VIDIOC_QUERYCTRL, &query) < 0) {
        query.minimum = query.maximum = 0;
    }
    UvcControlRange range(query.minimum, query.maximum, query.step, query.default_value);
    return range;
}

std::shared_ptr<const SourcePortInfo> ObV4lUvcDevicePort::getSourcePortInfo() const {
    return portInfo_;
}

bool ObV4lUvcDevicePort::getXu(uint8_t ctrl, uint8_t *data, uint32_t *len) {
    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(len);
    auto fd = deviceHandles_.front()->fd;
    memset(data, 0, *len);
    struct uvc_xu_control_query query = { static_cast<uint8_t>(xuUnit_.unit), ctrl, UVC_GET_CUR, static_cast<uint16_t>(*len), const_cast<uint8_t *>(data) };
    if(xioctl(fd, UVCIOC_CTRL_QUERY, &query) < 0) {
        LOG_ERROR("get xu failed, errno: {}", strerror(errno));
        return false;
    }

    return true;
}

bool ObV4lUvcDevicePort::setXu(uint8_t ctrl, const uint8_t *data, uint32_t len) {
    VALIDATE_NOT_NULL(data);
    auto                        fd    = deviceHandles_.front()->fd;
    struct uvc_xu_control_query query = { static_cast<uint8_t>(xuUnit_.unit), ctrl, UVC_SET_CUR, static_cast<uint16_t>(len), const_cast<uint8_t *>(data) };
    if(xioctl(fd, UVCIOC_CTRL_QUERY, &query) < 0) {
        LOG_ERROR("set xu failed, errno: {}", strerror(errno));
        return false;
    }
    return true;
}

void ObV4lUvcDevicePort::subscribeToCtrlEvent(uint32_t ctrl_id) const {
    auto                           fd = deviceHandles_.front()->fd;
    struct v4l2_event_subscription event_subscription {};
    event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
    event_subscription.type  = V4L2_EVENT_CTRL;
    event_subscription.id    = ctrl_id;
    memset(event_subscription.reserved, 0, sizeof(event_subscription.reserved));
    if(xioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &event_subscription) < 0) {
        LOG_ERROR("xioctl(VIDIOC_SUBSCRIBE_EVENT) with control_id={} failed!", ctrl_id);
    }
}

void ObV4lUvcDevicePort::unsubscribeFromCtrlEvent(uint32_t ctrl_id) const {
    auto                           fd = deviceHandles_.front()->fd;
    struct v4l2_event_subscription event_subscription {};
    event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
    event_subscription.type  = V4L2_EVENT_CTRL;
    event_subscription.id    = ctrl_id;
    memset(event_subscription.reserved, 0, sizeof(event_subscription.reserved));
    if(xioctl(fd, VIDIOC_UNSUBSCRIBE_EVENT, &event_subscription) < 0) {
        LOG_ERROR("xioctl(VIDIOC_UNSUBSCRIBE_EVENT) with control_id={} failed!", ctrl_id);
    }
}

bool ObV4lUvcDevicePort::pendForCtrlStatusEvent() const {
    auto              fd = deviceHandles_.front()->fd;
    struct v4l2_event event {};
    memset(&event, 0, sizeof(event));
    // Poll registered events and verify that set control event raised (wait max of 10 * 2 = 20 [ms])
    static int MAX_POLL_RETRIES = 10;
    for(int i = 0; i < MAX_POLL_RETRIES && event.type != V4L2_EVENT_CTRL; i++) {
        if(xioctl(fd, VIDIOC_DQEVENT, &event) < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return event.type == V4L2_EVENT_CTRL;
}

}  // namespace libobsensor
