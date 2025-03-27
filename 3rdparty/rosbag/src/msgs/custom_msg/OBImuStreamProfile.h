#ifndef CUSTOM_MSG_MESSAGE_OBIMUSTREAMPROFILEINFO_H
#define CUSTOM_MSG_MESSAGE_OBIMUSTREAMPROFILEINFO_H

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

template <class ContainerAllocator> struct OBImuStreamProfileInfo_ {
    typedef OBImuStreamProfileInfo_<ContainerAllocator> Type;

    OBImuStreamProfileInfo_()
        : header(),
          streamType(0),
          format(0),
          accelFullScaleRange(0),
          accelSampleRate(0),
          gyroFullScaleRange(0),
          gyroSampleRate(0),
          rotationMatrix(),
          translationMatrix(),
          noiseDensity(0),
          randomWalk(0),
          referenceTemp(0),
          bias(),
          gravity(),
          scaleMisalignment(),
          tempSlope() {
        rotationMatrix.fill(0.0f);
        translationMatrix.fill(0.0f);
        bias.fill(0.0f);
        gravity.fill(0.0f);
        scaleMisalignment.fill(0.0f);
        tempSlope.fill(0.0f);
    }

    OBImuStreamProfileInfo_(const ContainerAllocator &_alloc)
        : header(_alloc),
          streamType(0),
          format(0),
          accelFullScaleRange(0),
          accelSampleRate(0),
          gyroFullScaleRange(0),
          gyroSampleRate(0),
          rotationMatrix(),
          translationMatrix(),
          noiseDensity(0),
          randomWalk(0),
          referenceTemp(0),
          bias(),
          gravity(),
          scaleMisalignment(),
          tempSlope() {
        rotationMatrix.fill(0.0f);
        translationMatrix.fill(0.0f);
        bias.fill(0.0f);
        gravity.fill(0.0f);
        scaleMisalignment.fill(0.0f);
        tempSlope.fill(0.0f);
        (void)_alloc;
    }

    typedef ::std_msgs::Header_<ContainerAllocator> _header_type;
    _header_type                                    header;

    typedef uint8_t _streamType;
    _streamType     streamType;

    typedef uint8_t _formatType;
    _formatType     format;

    typedef uint8_t      _accelFullScaleRange;
    _accelFullScaleRange accelFullScaleRange;

    typedef uint8_t  _accelSampleRate;
    _accelSampleRate accelSampleRate;

    typedef uint8_t     _gyroFullScaleRange;
    _gyroFullScaleRange gyroFullScaleRange;

    typedef uint8_t _gyroSampleRate;
    _gyroSampleRate gyroSampleRate;

    typedef std::array<float, 9> _rotationMatrix;
    _rotationMatrix              rotationMatrix;

    typedef std::array<float, 3> _translationMatrix;
    _translationMatrix           translationMatrix;

    typedef double _noiseDensity;
    _noiseDensity  noiseDensity;

    typedef double _randomWalk;
    _randomWalk    randomWalk;

    typedef double _referenceTemp;
    _referenceTemp referenceTemp;

    typedef std::array<double, 3> _bias;
    _bias                         bias;

    typedef std::array<double, 3> _gravity;
    _gravity                      gravity;

    typedef std::array<double, 9> _scaleMisalignment;
    _scaleMisalignment            scaleMisalignment;

    typedef std::array<double, 9> _tempSlope;
    _tempSlope                    tempSlope;

    typedef std::shared_ptr<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>>       Ptr;
    typedef std::shared_ptr<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> const> ConstPtr;
};

typedef ::custom_msg::OBImuStreamProfileInfo_<std::allocator<void>> ImuStreamProfileInfo;
typedef std::shared_ptr<::custom_msg::ImuStreamProfileInfo>         ImuStreamProfileInfoPtr;
typedef std::shared_ptr<::custom_msg::ImuStreamProfileInfo const>   ImuStreamProfileInfoConstPtr;

template <typename ContainerAllocator> std::ostream &operator<<(std::ostream &s, const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> &v) {
    orbbecRosbag::message_operations::Printer<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>>::stream(s, "", v);
    return s;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator==(const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return lhs.header == rhs.header && lhs.streamType == rhs.streamType && lhs.format == rhs.format && lhs.accelFullScaleRange == rhs.accelFullScaleRange
           && lhs.accelSampleRate == rhs.accelSampleRate && lhs.gyroFullScaleRange == rhs.gyroFullScaleRange && lhs.gyroSampleRate == rhs.gyroSampleRate
           && lhs.rotationMatrix == rhs.rotationMatrix && lhs.translationMatrix == rhs.translationMatrix && lhs.noiseDensity == rhs.noiseDensity
           && lhs.randomWalk == rhs.randomWalk && lhs.referenceTemp == rhs.referenceTemp && lhs.bias == rhs.bias && lhs.gravity == rhs.gravity
           && lhs.scaleMisalignment == rhs.scaleMisalignment && lhs.tempSlope == rhs.tempSlope;
}

template <typename ContainerAllocator1, typename ContainerAllocator2>
bool operator!=(const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator1> &lhs, const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator2> &rhs) {
    return !(lhs == rhs);
}

}  // namespace custom_msg

namespace orbbecRosbag {
namespace message_traits {

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> : public FalseType {};

template <class ContainerAllocator> struct IsFixedSize<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> const> : public FalseType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct IsMessage<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> : public TrueType {};

template <class ContainerAllocator> struct HasHeader<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> const> : public TrueType {};

template <class ContainerAllocator> struct MD5Sum<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "0123456789abcdef0123456789abcdef";
    }
    static const char *value(const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
    static const uint64_t static_value1 = 0x0123456789abcdefULL;
    static const uint64_t static_value2 = 0x0123456789abcdefULL;
};

template <class ContainerAllocator> struct DataType<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "custom_msg/OBImuStreamProfileInfo";
    }
    static const char *value(const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

template <class ContainerAllocator> struct Definition<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> {
    static const char *value() {
        return "std_msgs/Header header\n"
               "uint8 streamType\n"
               "uint8 format\n"
               "uint8 accelFullScaleRange\n"
               "uint8 accelSampleRate\n"
               "uint8 gyroFullScaleRange\n"
               "uint8 gyroSampleRate\n"
               "float32[9] rotationMatrix\n"
               "float32[3] translationMatrix\n"
               "float64 noiseDensity\n"
               "float64 randomWalk\n"
               "float64 referenceTemp\n"
               "float64[3] bias\n"
               "float64[3] gravity\n"
               "float64[9] scaleMisalignment\n"
               "float64[9] tempSlope\n"
               "\n";
    }
    static const char *value(const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> &) {
        return value();
    }
};

}  // namespace message_traits
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace serialization {

template <class ContainerAllocator> struct Serializer<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream, typename T> inline static void allInOne(Stream &stream, T m) {
        stream.next(m.header);
        stream.next(m.streamType);
        stream.next(m.format);
        stream.next(m.accelFullScaleRange);
        stream.next(m.accelSampleRate);
        stream.next(m.gyroFullScaleRange);
        stream.next(m.gyroSampleRate);
        for(size_t i = 0; i < m.rotationMatrix.size(); ++i) {
            stream.next(m.rotationMatrix[i]);
        }
        for(size_t i = 0; i < m.translationMatrix.size(); ++i) {
            stream.next(m.translationMatrix[i]);
        }
        stream.next(m.noiseDensity);
        stream.next(m.randomWalk);
        stream.next(m.referenceTemp);
        for(size_t i = 0; i < 3; ++i)
            stream.next(m.bias[i]);
        for(size_t i = 0; i < 3; ++i)
            stream.next(m.gravity[i]);
        for(size_t i = 0; i < 9; ++i)
            stream.next(m.scaleMisalignment[i]);
        for(size_t i = 0; i < 9; ++i)
            stream.next(m.tempSlope[i]);
    }

    ROS_DECLARE_ALLINONE_SERIALIZER
};

}  // namespace serialization
}  // namespace orbbecRosbag

namespace orbbecRosbag {
namespace message_operations {

template <class ContainerAllocator> struct Printer<::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator>> {
    template <typename Stream> static void stream(Stream &s, const std::string &indent, const ::custom_msg::OBImuStreamProfileInfo_<ContainerAllocator> &v) {
        s << indent << "header: " << std::endl;
        Printer<::std_msgs::Header_<ContainerAllocator>>::stream(s, indent + "  ", v.header);
        s << indent << "streamType: " << static_cast<unsigned int>(v.streamType) << std::endl;
        s << indent << "format: " << static_cast<unsigned int>(v.format) << std::endl;
        s << indent << "accelFullScaleRange: " << static_cast<unsigned int>(v.accelFullScaleRange) << std::endl;
        s << indent << "accelSampleRate: " << static_cast<unsigned int>(v.accelSampleRate) << std::endl;
        s << indent << "gyroFullScaleRange: " << static_cast<unsigned int>(v.gyroFullScaleRange) << std::endl;
        s << indent << "gyroSampleRate: " << static_cast<unsigned int>(v.gyroSampleRate) << std::endl;
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

        s << indent << "  noiseDensity: " << v.noiseDensity << std::endl;
        s << indent << "  randomWalk: " << v.randomWalk << std::endl;
        s << indent << "  referenceTemp: " << v.referenceTemp << std::endl;
        s << indent << "  bias: [";
        for(size_t i = 0; i < 3; ++i) {
            s << v.bias[i];
            if(i < 2)
                s << ", ";
        }
        s << "]" << std::endl;
        s << indent << "  gravity: [";
        for(size_t i = 0; i < 3; ++i) {
            s << v.gravity[i];
            if(i < 2)
                s << ", ";
        }
        s << "]" << std::endl;
        s << indent << "  scaleMisalignment: [";
        for(size_t i = 0; i < v.scaleMisalignment.size(); ++i) {
            s << v.scaleMisalignment[i];
            if(i < v.scaleMisalignment.size() - 1)
                s << ", ";
        }
        s << "]" << std::endl;
        s << indent << "  tempSlope: [";
        for(size_t i = 0; i < v.tempSlope.size(); ++i) {
            s << v.tempSlope[i];
            if(i < v.tempSlope.size() - 1)
                s << ", ";
        }
        s << "]";
    }
};

}  // namespace message_operations
}  // namespace orbbecRosbag

#endif  // CUSTOM_MSG_MESSAGE_OBIMUSTREAMPROFILEINFO_H
