#ifndef CUSTOM_MSG_MESSAGE_OBLIDARSTREAMPROFILEINFO_H
#define CUSTOM_MSG_MESSAGE_OBLIDARSTREAMPROFILEINFO_H

#include <string>
#include <vector>
#include <map>
#include <array>
#include <memory>
#include <cstring>

#include <ros/types.h>
#include <ros/serialization.h>
#include <ros/builtin_message_traits.h>
#include <ros/message_operations.h>

#include <std_msgs/Header.h>

namespace custom_msg {

template <class ContainerAllocator> struct OBLiDARStreamProfileInfo_ {
    typedef OBLiDARStreamProfileInfo_<ContainerAllocator> Type;

    OBLiDARStreamProfileInfo_()
        : header(),
          streamType(0),
          format(0),
          scanRate(0) {
    }

    OBLiDARStreamProfileInfo_(const ContainerAllocator &_alloc)
        : header(_alloc),
          streamType(0),
          format(0),
          scanRate(0) {
        (void)_alloc;
    }

    typedef ::std_msgs::Header_<ContainerAllocator> _header_type;
    _header_type                                    header;

    typedef uint8_t _streamType;
    _streamType     streamType;

    typedef uint8_t _formatType;
    _formatType     format;

    typedef uint32_t     _scanRateType;
    _scanRateType        scanRate;

    typedef std::shared_ptr<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>>       Ptr;
    typedef std::shared_ptr<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> const> ConstPtr;
};

typedef ::custom_msg::OBLiDARStreamProfileInfo_<std::allocator<void>> LiDARStreamProfileInfo;
typedef std::shared_ptr<::custom_msg::LiDARStreamProfileInfo>         LiDARStreamProfileInfoPtr;
typedef std::shared_ptr<::custom_msg::LiDARStreamProfileInfo const>   LiDARStreamProfileInfoConstPtr;

template <typename ContainerAllocator> std::ostream &operator<<(std::ostream &s, const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> &v) {
    orbbecRosbag::message_operations::Printer<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>>::stream(s, "", v);
    return s;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator==(const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return lhs.header == rhs.header && lhs.streamType == rhs.streamType && lhs.format == rhs.format && lhs.scanRate == rhs.scanRate;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator!=(const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return !(lhs == rhs);
}

}  // namespace custom_msg

namespace orbbecRosbag {
namespace message_traits {

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> : public FalseType {};

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> const> : public FalseType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct MD5Sum<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "6e5053dfe4a506d0981a31d1b76f70dc";
    }
    static const char *value(const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
    static const uint64_t static_value1 = 0x6e5053dfe4a506d0ULL;
    static const uint64_t static_value2 = 0x981a31d1b76f70dcULL;
};

template <class ContainerAllocator> struct DataType<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "custom_msg/OBLiDARStreamProfileInfo";
    }
    static const char *value(const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

template <class ContainerAllocator> struct Definition<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "std_msgs/Header header\n"
               "uint8 streamType\n"
               "uint8 format\n"
               "uint32 scanRate\n"
               "\n";
    }
    static const char *value(const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

}  // namespace message_traits
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace serialization {

template <class ContainerAllocator> struct Serializer<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream, typename T> inline static void allInOne(Stream &stream, T m) {
        stream.next(m.header);
        stream.next(m.streamType);
        stream.next(m.format);
        stream.next(m.scanRate);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
};

}  // namespace serialization
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace message_operations {

template <class ContainerAllocator> struct Printer<::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream> static void stream(Stream &s, const std::string &indent, const ::custom_msg::OBLiDARStreamProfileInfo_<ContainerAllocator> &v) {
        s << indent << "header: " << std::endl;
        Printer<::std_msgs::Header_<ContainerAllocator>>::stream(s, indent + "  ", v.header);
        s << indent << "streamType: " << static_cast<unsigned int>(v.streamType) << std::endl;
        s << indent << "format: " << static_cast<unsigned int>(v.format) << std::endl;
        s << indent << "scanRate: " << static_cast<unsigned int>(v.scanRate) << std::endl;
    }
};

}  // namespace message_operations
}  // namespace orbbecRosbag

#endif  // CUSTOM_MSG_MESSAGE_OBLIDARSTREAMPROFILEINFO_H
