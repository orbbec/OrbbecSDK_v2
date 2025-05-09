// Generated by gencpp from file custom_msg/DeviceInfo.msg
// DO NOT EDIT!

#ifndef CUSTOM_MSG_MESSAGE_OBDEVICEINFO_H
#define CUSTOM_MSG_MESSAGE_OBDEVICEINFO_H

#include <string>
#include <vector>
#include <map>

#include <ros/types.h>
#include <ros/serialization.h>
#include <ros/builtin_message_traits.h>
#include <ros/message_operations.h>

#include <std_msgs/Header.h>

namespace custom_msg {
template <class ContainerAllocator> struct OBDeviceInfo_ {
    typedef OBDeviceInfo_<ContainerAllocator> Type;

    OBDeviceInfo_()
        : header(),
          name(),
          fullName(),
          asicName(),
          vid(0),
          pid(0),
          uid(),
          sn(),
          fwVersion(),
          hwVersion(),
          supportedSdkVersion(),
          connectionType(),
          type(0),
          backendType(0),
          ipAddress(),
          localMac() {}
    OBDeviceInfo_(const ContainerAllocator &_alloc)
        : header(_alloc),
          name(_alloc),
          fullName(_alloc),
          asicName(_alloc),
          vid(0),
          pid(0),
          uid(_alloc),
          sn(_alloc),
          fwVersion(_alloc),
          hwVersion(_alloc),
          supportedSdkVersion(_alloc),
          connectionType(_alloc),
          type(0),
          backendType(_alloc),
          ipAddress(_alloc),
          localMac(_alloc) {
        (void)_alloc;
    }

    typedef ::std_msgs::Header_<ContainerAllocator> _header_type;
    _header_type                                    header;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _name;
    _name                                                                                                              name;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _fullName;
    _fullName                                                                                                          fullName;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _asicName;
    _asicName                                                                                                          asicName;

    typedef uint16_t _vid;
    _vid             vid;

    typedef uint16_t _pid;
    _pid             pid;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _uid;
    _uid                                                                                                               uid;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _sn;
    _sn                                                                                                                sn;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _fwVersion;
    _fwVersion                                                                                                         fwVersion;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _hwVersion;
    _hwVersion                                                                                                         hwVersion;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _supportedSdkVersion;
    _supportedSdkVersion                                                                                               supportedSdkVersion;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _connectionType;
    _connectionType                                                                                                    connectionType;

    typedef uint16_t _type;
    _type            type;

    typedef uint8_t _backendType;
    _backendType    backendType;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _ipAddress;
    _ipAddress                                                                                                         ipAddress;

    typedef std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other> _localMac;
    _localMac                                                                                                          localMac;

    typedef std::shared_ptr<::custom_msg::OBDeviceInfo_<ContainerAllocator>>       Ptr;
    typedef std::shared_ptr<::custom_msg::OBDeviceInfo_<ContainerAllocator> const> ConstPtr;
};

typedef ::custom_msg::OBDeviceInfo_<std::allocator<void>> DeviceInfo;

typedef std::shared_ptr<::custom_msg::DeviceInfo>       DeviceInfoPtr;
typedef std::shared_ptr<::custom_msg::DeviceInfo const> DeviceInfoConstPtr;

template <typename ContainerAllocator> std::ostream &operator<<(std::ostream &s, const ::custom_msg::OBDeviceInfo_<ContainerAllocator> &v) {
    orbbecRosbag::message_operations::Printer<::custom_msg::OBDeviceInfo_<ContainerAllocator>>::stream(s, "", v);
    return s;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator==(const ::custom_msg::OBDeviceInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBDeviceInfo_<ContainerAllocator2> &rhs) {
    return lhs.header == rhs.header && lhs.name == rhs.name && lhs.fullName == rhs.fullName && lhs.asicName == rhs.asicName && lhs.vid == rhs.vid
           && lhs.pid == rhs.pid && lhs.uid == rhs.uid && lhs.sn == rhs.sn && lhs.fwVersion == rhs.fwVersion && lhs.hwVersion == rhs.hwVersion
           && lhs.supportedSdkVersion == rhs.supportedSdkVersion && lhs.connectionType == rhs.connectionType && lhs.backendType == rhs.backendType
           && lhs.type == rhs.type && lhs.ipAddress == rhs.ipAddress && lhs.localMac == rhs.localMac;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator!=(const ::custom_msg::OBDeviceInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBDeviceInfo_<ContainerAllocator2> &rhs) {
    return !(lhs == rhs);
}

}  // namespace custom_msg

namespace orbbecRosbag {
namespace message_traits {

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBDeviceInfo_<ContainerAllocator>> : FalseType {};

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBDeviceInfo_<ContainerAllocator> const> : FalseType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBDeviceInfo_<ContainerAllocator>> : TrueType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBDeviceInfo_<ContainerAllocator> const> : TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBDeviceInfo_<ContainerAllocator>> : TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBDeviceInfo_<ContainerAllocator> const> : TrueType {};

template <class ContainerAllocator> struct MD5Sum<::custom_msg::OBDeviceInfo_<ContainerAllocator>> {
    static const char *value() {
        return "afef5328bb4abebd1a309e1ea8b26b3a";
    }

    static const char *value(const ::custom_msg::OBDeviceInfo_<ContainerAllocator> &) {
        return value();
    }
    static const uint64_t static_value1 = 0xafef5328bb4abebdULL;
    static const uint64_t static_value2 = 0x1a309e1ea8b26b3aULL;
};

template <class ContainerAllocator> struct DataType<::custom_msg::OBDeviceInfo_<ContainerAllocator>> {
    static const char *value() {
        return "custom_msg/OBDeviceInfo";
    }

    static const char *value(const ::custom_msg::OBDeviceInfo_<ContainerAllocator> &) {
        return value();
    }
};

template <class ContainerAllocator> struct Definition<::custom_msg::OBDeviceInfo_<ContainerAllocator>> {
    static const char *value() {
        return "std_msgs/Header header\n"
               "string name\n"
               "string fullName\n"
               "string asicName\n"
               "uint16 vid\n"
               "uint16 pid\n"
               "string uid\n"
               "string sn\n"
               "string fwVersion\n"
               "string hwVersion\n"
               "string supportedSdkVersion\n"
               "string connectionType\n"
               "uint16 type\n"
               "uint8 backendType\n"
               "string ipAddress\n"
               "string localMac\n"
               "\n"
               "================================================================================\n"
               "MSG: std_msgs/Header\n"
               "# Standard metadata for higher-level stamped data types.\n"
               "# This is generally used to communicate timestamped data \n"
               "# in a particular coordinate frame.\n"
               "# \n"
               "# sequence ID: consecutively increasing ID \n"
               "uint32 seq\n"
               "#Two-integer timestamp that is expressed as:\n"
               "# * stamp.sec: seconds (stamp_secs) since epoch (in Python the variable is called 'secs')\n"
               "# * stamp.nsec: nanoseconds since stamp_secs (in Python the variable is called 'nsecs')\n"
               "# time-handling sugar is provided by the client library\n"
               "time stamp\n"
               "#Frame this data is associated with\n"
               "string frame_id\n";
    }

    static const char *value(const ::custom_msg::OBDeviceInfo_<ContainerAllocator> &) {
        return value();
    }
};

}  // namespace message_traits
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace serialization {

template <class ContainerAllocator> struct Serializer<::custom_msg::OBDeviceInfo_<ContainerAllocator>> {
    template <typename Stream, typename T> inline static void allInOne(Stream &stream, T m) {
        stream.next(m.header);
        stream.next(m.name);
        stream.next(m.fullName);
        stream.next(m.asicName);
        stream.next(m.vid);
        stream.next(m.pid);
        stream.next(m.uid);
        stream.next(m.sn);
        stream.next(m.fwVersion);
        stream.next(m.hwVersion);
        stream.next(m.supportedSdkVersion);
        stream.next(m.connectionType);
        stream.next(m.type);
        stream.next(m.backendType);
        stream.next(m.ipAddress);
        stream.next(m.localMac);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
};

}  // namespace serialization
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace message_operations {

template <class ContainerAllocator> struct Printer<::custom_msg::OBDeviceInfo_<ContainerAllocator>> {
    template <typename Stream> static void stream(Stream &s, const std::string &indent, const ::custom_msg::OBDeviceInfo_<ContainerAllocator> &v) {
        s << indent << "header: ";
        s << std::endl;
        Printer<::std_msgs::Header_<ContainerAllocator>>::stream(s, indent + "  ", v.header);
        s << indent << "name: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ", v.name);
        s << indent << "fullName: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.fullName);
        s << indent << "asicName: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.asicName);
        s << indent << "vid: ";
        Printer<uint16_t>::stream(s, indent + "  ", v.vid);
        s << indent << "pid: ";
        Printer<uint16_t>::stream(s, indent + "  ", v.pid);
        s << indent << "uid: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ", v.uid);
        s << indent << "sn: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ", v.sn);
        s << indent << "fwVersion: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.fwVersion);
        s << indent << "hwVersion: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.hwVersion);
        s << indent << "supportedSdkVersion: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.supportedSdkVersion);
        s << indent << "connectionType: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.connectionType);
        s << indent << "type: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ", v.type);
        s << indent << "backendType: ";
        Printer<uint8_t>::stream(s, indent + "  ", v.backendType);
        s << indent << "ipAddress: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.ipAddress);
        s << indent << "localMac: ";
        Printer<std::basic_string<char, std::char_traits<char>, typename ContainerAllocator::template rebind<char>::other>>::stream(s, indent + "  ",
                                                                                                                                    v.localMac);
    }
};

}  // namespace message_operations
}  // namespace orbbecRosbag

#endif  // CUSTOM_MSG_MESSAGE_OBDEVICEINFO_H
