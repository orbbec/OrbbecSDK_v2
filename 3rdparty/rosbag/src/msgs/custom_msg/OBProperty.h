#ifndef CUSTOM_MSG_MESSAGE_OBPROPERTY_H
#define CUSTOM_MSG_MESSAGE_OBPROPERTY_H

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

template <class ContainerAllocator> struct OBProperty_ {
    typedef OBProperty_<ContainerAllocator> Type;
    OBProperty_() : header(), propertyId(0), data(0), datasize(0) {}

    OBProperty_(const ContainerAllocator &_alloc) : header(_alloc), propertyId(_alloc), data(_alloc), datasize(_alloc) {
        (void)_alloc;
    }

    typedef ::std_msgs::Header_<ContainerAllocator> _header_type;
    _header_type                                    header;

    typedef uint32_t _propertyId;
    _propertyId      propertyId;

    typedef std::vector<uint8_t, typename ContainerAllocator::template rebind<uint8_t>::other> _data;
    _data                                                                                      data;

    typedef uint32_t _datasize;
    _datasize        datasize;

    typedef std::shared_ptr<::custom_msg::OBProperty_<ContainerAllocator>>       Ptr;
    typedef std::shared_ptr<::custom_msg::OBProperty_<ContainerAllocator> const> ConstPtr;
};

typedef ::custom_msg::OBProperty_<std::allocator<void>> property;
typedef std::shared_ptr<::custom_msg::property>         propertyPtr;
typedef std::shared_ptr<::custom_msg::property const>   propertyConstPtr;

// constants requiring out of line definition

template <typename ContainerAllocator> std::ostream &operator<<(std::ostream &s, const ::custom_msg::OBProperty_<ContainerAllocator> &v) {
    orbbecRosbag::message_operations::Printer<::custom_msg::OBProperty_<ContainerAllocator>>::stream(s, "", v);
    return s;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator==(const ::custom_msg::OBProperty_<ContainerAllocator1> &lhs, const ::custom_msg::OBProperty_<ContainerAllocator2> &rhs) {
    return lhs.header == rhs.header && lhs.propertyId == rhs.propertyId && lhs.data == rhs.data && lhs.datasize == rhs.datasize;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator!=(const ::custom_msg::OBProperty_<ContainerAllocator1> &lhs, const ::custom_msg::OBProperty_<ContainerAllocator2> &rhs) {
    return !(lhs == rhs);
}

}  // namespace custom_msg

namespace orbbecRosbag {
namespace message_traits {

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBProperty_<ContainerAllocator>> : public FalseType {};

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBProperty_<ContainerAllocator> const> : public FalseType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBProperty_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBProperty_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBProperty_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBProperty_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct MD5Sum<::custom_msg::OBProperty_<ContainerAllocator>> {
    static const char *value() {
        return "0123456789abcdef0123456789abcdef";
    }

    static const char *value(const ::custom_msg::OBProperty_<ContainerAllocator> &) {
        return value();
    }

    static const uint64_t static_value1 = 0x0123456789abcdefULL;
    static const uint64_t static_value2 = 0x0123456789abcdefULL;
};

template <class ContainerAllocator> struct DataType<::custom_msg::OBProperty_<ContainerAllocator>> {
    static const char *value() {
        return "custom_msg/OBProperty";
    }

    static const char *value(const ::custom_msg::OBProperty_<ContainerAllocator> &) {
        return value();
    }
};

template <class ContainerAllocator> struct Definition<::custom_msg::OBProperty_<ContainerAllocator>> {
    static const char *value() {
        return "std_msgs/Header header\n"
               "uint8 propertyId\n"
               "uint8[] data  \n"
               "uint32 datasize\n";
    }

    static const char *value(const ::custom_msg::OBProperty_<ContainerAllocator> &) {
        return value();
    }
};

}  // namespace message_traits
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace serialization {

template <class ContainerAllocator> struct Serializer<::custom_msg::OBProperty_<ContainerAllocator>> {
    template <typename Stream, typename T> inline static void allInOne(Stream &stream, T m) {
        stream.next(m.header);
        stream.next(m.propertyId);
        stream.next(m.data);
        stream.next(m.datasize);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
};

}  // namespace serialization
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace message_operations {

template <class ContainerAllocator> struct Printer<::custom_msg::OBProperty_<ContainerAllocator>> {
    template <typename Stream> static void stream(Stream &s, const std::string &indent, const ::custom_msg::OBProperty_<ContainerAllocator> &v) {
        s << indent << "header: ";
        s << std::endl;
        Printer<::std_msgs::Header_<ContainerAllocator>>::stream(s, indent + "  ", v.header);
        s << indent << "propertyId: " << static_cast<unsigned int>(v.propertyId) << std::endl;
        s << indent << "data ";
        for(size_t i = 0; i < v.data.size(); ++i) {
            s << indent << "  data[" << i << "]: ";
            Printer<uint8_t>::stream(s, indent + "  ", v.data[i]);
        }
        s << indent << "datasize: ";
        Printer<uint32_t>::stream(s, indent + "  ", v.datasize);
    }
};

}  // namespace message_operations
}  // namespace orbbecRosbag

#endif  // CUSTOM_MSG_MESSAGE_OBPROPERTY_H
