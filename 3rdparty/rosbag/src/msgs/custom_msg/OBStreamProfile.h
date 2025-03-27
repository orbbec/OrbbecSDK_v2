#ifndef CUSTOM_MSG_MESSAGE_OBSTEAMPROFILEINFO_H
#define CUSTOM_MSG_MESSAGE_OBSTEAMPROFILEINFO_H

#include <string>
#include <vector>
#include <map>
#include <array>

#include <ros/types.h>
#include <ros/serialization.h>
#include <ros/builtin_message_traits.h>
#include <ros/message_operations.h>

#include <std_msgs/Header.h>

namespace custom_msg {

template <class ContainerAllocator> struct OBStreamProfileInfo_ {
    typedef OBStreamProfileInfo_<ContainerAllocator> Type;
    OBStreamProfileInfo_()
        : header(),
          streamType(0),
          format(0),
          rotationMatrix(),
          translationMatrix(),
          width(0),
          height(0),
          fps(0),
          cameraIntrinsic(),
          cameraDistortion(),
          distortionModel(0) {
        rotationMatrix.fill(0.0f);
        translationMatrix.fill(0.0f);
        cameraIntrinsic.fill(0.0f);
    }

    OBStreamProfileInfo_(const ContainerAllocator &_alloc)
        : header(_alloc),
          streamType(0),
          format(0),
          rotationMatrix(),
          translationMatrix(),
          width(0),
          height(0),
          fps(0),
          cameraIntrinsic(),
          cameraDistortion(),
          distortionModel(0) {
        rotationMatrix.fill(0.0f);
        translationMatrix.fill(0.0f);
        cameraIntrinsic.fill(0.0f);
        (void)_alloc;
    }

    typedef ::std_msgs::Header_<ContainerAllocator> _header_type;
    _header_type                                    header;

    typedef uint8_t _streamType;
    _streamType     streamType;

    typedef uint8_t _formatType;
    _formatType     format;

    typedef std::array<float, 9> _rotationMatrix;
    _rotationMatrix              rotationMatrix;

    typedef std::array<float, 3> _translationMatrix;
    _translationMatrix           translationMatrix;

    typedef uint16_t _width;
    _width           width;

    typedef uint16_t _height;
    _height          height;

    typedef uint16_t _fps;
    _fps             fps;

    typedef std::array<float, 4> _cameraIntrinsic;
    _cameraIntrinsic             cameraIntrinsic;

    typedef std::array<float, 8> _cameraDistortion;
    _cameraDistortion            cameraDistortion;

    typedef uint8_t  _distortionModel;
    _distortionModel distortionModel;

    typedef std::shared_ptr<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>>       Ptr;
    typedef std::shared_ptr<::custom_msg::OBStreamProfileInfo_<ContainerAllocator> const> ConstPtr;
};

typedef ::custom_msg::OBStreamProfileInfo_<std::allocator<void>> StreamProfileInfo;
typedef std::shared_ptr<::custom_msg::StreamProfileInfo>         StreamProfileInfoPtr;
typedef std::shared_ptr<::custom_msg::StreamProfileInfo const>   StreamProfileInfoConstPtr;

template <typename ContainerAllocator> std::ostream &operator<<(std::ostream &s, const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator> &v) {
    orbbecRosbag::message_operations::Printer<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>>::stream(s, "", v);
    return s;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator==(const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return lhs.header == rhs.header && lhs.streamType == rhs.streamType && lhs.format == rhs.format && lhs.rotationMatrix == rhs.rotationMatrix
           && lhs.translationMatrix == rhs.translationMatrix && lhs.width == rhs.width && lhs.height == rhs.height && lhs.fps == rhs.fps
           && lhs.cameraIntrinsic == rhs.cameraIntrinsic && lhs.cameraDistortion == rhs.cameraDistortion && lhs.distortionModel == rhs.distortionModel;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator!=(const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return !(lhs == rhs);
}

}  // namespace custom_msg

namespace orbbecRosbag {
namespace message_traits {

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> : public FalseType {};

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBStreamProfileInfo_<ContainerAllocator> const> : public FalseType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct MD5Sum<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "0123456789abcdef0123456789abcdef";
    }

    static const char *value(const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }

    static const uint64_t static_value1 = 0x0123456789abcdefULL;
    static const uint64_t static_value2 = 0x0123456789abcdefULL;
};

template <class ContainerAllocator> struct DataType<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "custom_msg/OBStreamProfileInfo";
    }

    static const char *value(const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

template <class ContainerAllocator> struct Definition<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "std_msgs/Header header\n"
               "uint8 streamType\n"
               "uint8 format\n"
               "float32[9] rotationMatrix\n"
               "float32[3] translationMatrix\n"
               "uint16 width\n"
               "uint16 height\n"
               "uint16 fps\n"
               "float32[4] cameraIntrinsic\n"
               "float32[8] cameraDistortion\n"
               "uint8 distortionModel\n";
    }

    static const char *value(const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

}  // namespace message_traits
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace serialization {

template <class ContainerAllocator> struct Serializer<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream, typename T> inline static void allInOne(Stream &stream, T m) {
        stream.next(m.header);
        stream.next(m.streamType);
        stream.next(m.format);
        for(size_t i = 0; i < m.rotationMatrix.size(); ++i) {
            stream.next(m.rotationMatrix[i]);
        }
        for(size_t i = 0; i < m.translationMatrix.size(); ++i) {
            stream.next(m.translationMatrix[i]);
        }
        stream.next(m.width);
        stream.next(m.height);
        stream.next(m.fps);
        for(size_t i = 0; i < m.cameraIntrinsic.size(); ++i) {
            stream.next(m.cameraIntrinsic[i]);
        }
        for(size_t i = 0; i < m.cameraDistortion.size(); ++i) {
            stream.next(m.cameraDistortion[i]);
        }
        stream.next(m.distortionModel);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
};

}  // namespace serialization
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace message_operations {

template <class ContainerAllocator> struct Printer<::custom_msg::OBStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream> static void stream(Stream &s, const std::string &indent, const ::custom_msg::OBStreamProfileInfo_<ContainerAllocator> &v) {
        s << indent << "header: ";
        s << std::endl;
        Printer<::std_msgs::Header_<ContainerAllocator>>::stream(s, indent + "  ", v.header);
        s << indent << "streamType: " << static_cast<unsigned int>(v.streamType) << std::endl;
        s << indent << "format: " << static_cast<unsigned int>(v.format) << std::endl;
        s << indent << "rotationMatrix: [";
        for(size_t i = 0; i < v.rotationMatrix.size(); ++i) {
            s << v.rotationMatrix[i];
            if(i < v.rotationMatrix.size() - 1)
                s << ", ";
        }
        s << "]" << std::endl;
        s << indent << "translationMatrix: [";
        for(size_t i = 0; i < v.translationMatrix.size(); ++i) {
            s << v.translationMatrix[i];
            if(i < v.translationMatrix.size() - 1)
                s << ", ";
        }
        s << "]" << std::endl;
        s << indent << "Width: " << v.width << std::endl;
        s << indent << "Height: " << v.height << std::endl;
        s << indent << "fps: " << v.fps << std::endl;
        s << indent << "cameraIntrinsic: [";
        for(size_t i = 0; i < v.cameraIntrinsic.size(); ++i) {
            s << v.cameraIntrinsic[i];
            if(i < v.cameraIntrinsic.size() - 1)
                s << ", ";
        }
        s << "]";
        s << indent << "cameraDistortion: [";
        for(size_t i = 0; i < v.cameraDistortion.size(); ++i) {
            s << v.cameraDistortion[i];
            if(i < v.cameraDistortion.size() - 1)
                s << ", ";
        }
        s << "]";
        s << indent << "distortionModel: " << v.distortionModel << std::endl;
    }
};

}  // namespace message_operations
}  // namespace orbbecRosbag

#endif  // CUSTOM_MSG_MESSAGE_OBSTREAMPROFILEINFO_H
