#include "libobsensor/h/Utils.h"
#include "libobsensor/ObSensor.hpp"
#include "exception/ObException.hpp"
#include "exception/ObException.hpp"
#include "utils/CoordinateUtil.hpp"
#include "frame/FrameFactory.hpp"
#include "FilterFactory.hpp"
#include "ImplTypes.hpp"

#ifdef __cplusplus
extern "C" {
 #endif

bool ob_calibration_3d_to_3d(const ob_calibration_param calibration_param, const ob_point3f source_point3f,
                                              const ob_sensor_type source_sensor_type, const ob_sensor_type target_sensor_type, ob_point3f *target_point3f,
                                              ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point3f);
    // If the source and target are the same, no transformation is needed
    if(target_sensor_type == source_sensor_type) 
    {
        *target_point3f = source_point3f;
        return true;
    }
    OBExtrinsic     extrinsic;
    memcpy(&extrinsic, &calibration_param.extrinsics[source_sensor_type][target_sensor_type], sizeof(OBExtrinsic));

    return libobsensor::CoordinateUtil::transformation3dTo3d(source_point3f, extrinsic, target_point3f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, calibration_param, source_point3f, source_sensor_type, target_sensor_type, target_point3f)

bool ob_transformation_3d_to_3d(const OBPoint3f source_point3f, OBExtrinsic extrinsic, OBPoint3f *target_point3f, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point3f);
    return libobsensor::CoordinateUtil::transformation3dTo3d(source_point3f, extrinsic, target_point3f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, source_point3f, extrinsic, target_point3f)

bool ob_calibration_2d_to_3d(const ob_calibration_param calibration_param, const ob_point2f source_point2f,
                                              const float source_depth_pixel_value, const ob_sensor_type source_sensor_type,
                                              const ob_sensor_type target_sensor_type, ob_point3f *target_point3f, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point3f);
    OBExtrinsic     extrinsic;
    OBCameraIntrinsic  sourceIntrinsic;
    memcpy(&extrinsic, &calibration_param.extrinsics[source_sensor_type][target_sensor_type], sizeof(OBExtrinsic));

    // Depending on the source sensor type, copy the corresponding intrinsic data
    if(OB_SENSOR_DEPTH == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_DEPTH], sizeof(OBCameraIntrinsic));
    } else if(OB_SENSOR_COLOR == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_COLOR], sizeof(OBCameraIntrinsic)); 
    } else {
        return false;
    }
    return libobsensor::CoordinateUtil::transformation2dTo3d(sourceIntrinsic, source_point2f, source_depth_pixel_value, extrinsic, target_point3f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, calibration_param, source_point2f, source_depth_pixel_value, source_sensor_type, target_sensor_type, target_point3f)

bool ob_transformation_2d_to_3d(const OBPoint2f source_point2f, const float source_depth_pixel_value, const OBCameraIntrinsic source_intrinsic, 
                                         OBExtrinsic extrinsic, OBPoint3f *target_point3f, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point3f);
    return libobsensor::CoordinateUtil::transformation2dTo3d(source_intrinsic, source_point2f, source_depth_pixel_value, extrinsic, target_point3f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, source_point2f, source_depth_pixel_value, source_intrinsic, extrinsic, target_point3f)

bool ob_calibration_3d_to_2d(const ob_calibration_param calibration_param, const ob_point3f source_point3f,
                                              const ob_sensor_type source_sensor_type, const ob_sensor_type target_sensor_type, ob_point2f *target_point2f,
                                              ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point2f);
    OBExtrinsic     extrinsic;
    OBCameraIntrinsic  sourceIntrinsic; 
    OBCameraDistortion depthDistortion;

    memcpy(&extrinsic, &calibration_param.extrinsics[source_sensor_type][target_sensor_type], sizeof(OBExtrinsic));
    // Depending on the source sensor type, copy the corresponding intrinsic data
    if(OB_SENSOR_DEPTH == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_DEPTH], sizeof(OBCameraIntrinsic));
        memcpy(&depthDistortion, &calibration_param.distortion[OB_SENSOR_DEPTH], sizeof(OBCameraDistortion));
    } else if(OB_SENSOR_COLOR == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_COLOR], sizeof(OBCameraIntrinsic)); 
        memcpy(&depthDistortion, &calibration_param.distortion[OB_SENSOR_COLOR], sizeof(OBCameraDistortion));
    } else {
        return false;
    }

    return libobsensor::CoordinateUtil::transformation3dTo2d(source_point3f, sourceIntrinsic, depthDistortion, extrinsic, target_point2f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, calibration_param, source_point3f, source_sensor_type, target_sensor_type, target_point2f)

bool ob_transformation_3d_to_2d(const OBPoint3f source_point3f, const OBCameraIntrinsic target_intrinsic, const OBCameraDistortion target_distortion,
                                          OBExtrinsic extrinsic, OBPoint2f *target_point2f, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point2f);
    return libobsensor::CoordinateUtil::transformation3dTo2d(source_point3f, target_intrinsic, target_distortion, extrinsic, target_point2f);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, source_point3f, target_intrinsic, target_distortion, extrinsic, target_point2f)


bool ob_calibration_2d_to_2d(const ob_calibration_param calibration_param, const ob_point2f source_point2f,
                                              const float source_depth_pixel_value, const ob_sensor_type source_sensor_type,
                                              const ob_sensor_type target_sensor_type, ob_point2f *target_point2f,
                                               ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point2f);
    OBExtrinsic     extrinsic;
    OBCameraIntrinsic  sourceIntrinsic; 
    OBCameraIntrinsic  targetIntrinsic; 
    OBCameraDistortion sourceDistortion;
    OBCameraDistortion targetDistortion;

    memcpy(&extrinsic, &calibration_param.extrinsics[source_sensor_type][target_sensor_type], sizeof(OBExtrinsic));
    // Depending on the source sensor type, copy the corresponding intrinsic data
    if(OB_SENSOR_DEPTH == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_DEPTH], sizeof(OBCameraIntrinsic));
        memcpy(&sourceDistortion, &calibration_param.distortion[OB_SENSOR_DEPTH], sizeof(OBCameraDistortion));
    } else if(OB_SENSOR_COLOR == source_sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_COLOR], sizeof(OBCameraIntrinsic)); 
        memcpy(&sourceDistortion, &calibration_param.distortion[OB_SENSOR_COLOR], sizeof(OBCameraDistortion));
    } else {
        return false;
    }

    // Depending on the target sensor type, copy the corresponding intrinsic data
    if(OB_SENSOR_DEPTH == target_sensor_type) {
        memcpy(&targetIntrinsic, &calibration_param.intrinsics[OB_SENSOR_DEPTH], sizeof(OBCameraIntrinsic));
        memcpy(&targetDistortion, &calibration_param.distortion[OB_SENSOR_DEPTH], sizeof(OBCameraDistortion));
    } else if(OB_SENSOR_COLOR == target_sensor_type) {
        memcpy(&targetIntrinsic, &calibration_param.intrinsics[OB_SENSOR_COLOR], sizeof(OBCameraIntrinsic)); 
        memcpy(&targetDistortion, &calibration_param.distortion[OB_SENSOR_COLOR], sizeof(OBCameraDistortion));
    } else {
        return false;
    }

    return libobsensor::CoordinateUtil::transformation2dTo2d(sourceIntrinsic, sourceDistortion, source_point2f, 
                        source_depth_pixel_value, targetIntrinsic, targetDistortion, extrinsic, target_point2f );
}
HANDLE_EXCEPTIONS_AND_RETURN(false, calibration_param, source_point2f, source_depth_pixel_value, source_sensor_type, target_sensor_type, target_point2f)

bool ob_transformation_2d_to_2d(const OBPoint2f source_point2f, const float source_depth_pixel_value, const OBCameraIntrinsic source_intrinsic,
                            const OBCameraDistortion source_distortion, const OBCameraIntrinsic target_intrinsic, const OBCameraDistortion target_distortion,
                            OBExtrinsic extrinsic, OBPoint2f *target_point2f, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(target_point2f);
    return libobsensor::CoordinateUtil::transformation2dTo2d(source_intrinsic, source_distortion, source_point2f, 
                        source_depth_pixel_value, target_intrinsic, target_distortion, extrinsic, target_point2f );
}
HANDLE_EXCEPTIONS_AND_RETURN(false, source_point2f, source_depth_pixel_value, source_intrinsic, source_distortion, target_intrinsic, target_distortion, extrinsic, target_point2f)

ob_frame *transformation_depth_frame_to_color_camera(ob_device *device, ob_frame *depth_frame, uint32_t target_color_camera_width,
                                                                      uint32_t target_color_camera_height, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(device->device);
    VALIDATE_NOT_NULL(depth_frame );
    VALIDATE_NOT_NULL(depth_frame->frame);

    VALIDATE_NOT_EQUAL(target_color_camera_width, 0);
    VALIDATE_NOT_EQUAL(target_color_camera_height, 0);

    //Get the color sensor from the device to get the streamprofile matching the target width/height.
    auto sensor     = device->device->getSensor(OB_SENSOR_COLOR);    
    auto profileList = sensor->getStreamProfileList();
    auto matchedProfileList = matchVideoStreamProfile(profileList, target_color_camera_width, target_color_camera_height, OB_FPS_ANY, OB_FORMAT_ANY);
    if(matchedProfileList.empty()) {
         throw std::runtime_error("Error:No matched color stream profile");
    }
   
    //Create a color frame from the streamprofile 
    auto colorFrame =  libobsensor::FrameFactory::createFrameFromStreamProfile(matchedProfileList[0]);
    
    //Create frameset and put colorframe and depthframe in it
    auto frameset = libobsensor::FrameFactory::createFrameSet();
    frameset->pushFrame(colorFrame);
    frameset->pushFrame(depth_frame->frame);  

    //The process frameset gets the resultFrameSet
    auto filterFactory = libobsensor::FilterFactory::getInstance();
    auto filter        = filterFactory->createFilter("Align");    
    filter->setConfigValue("AlignType", static_cast<double>(OB_STREAM_COLOR));
    auto resultFrameSet = filter->process(frameset);

    //Take out the depth frame from frameset
    if(!resultFrameSet->is<libobsensor::FrameSet>()) {
        throw libobsensor::unsupported_operation_exception("Error: not a frameset");
    }
    auto resultFrame = resultFrameSet->as<libobsensor::FrameSet>()->getFrame(OB_FRAME_DEPTH);
    if(resultFrame == nullptr) {
        return nullptr;
    }
    auto obframe   = new ob_frame();
    obframe->frame = std::const_pointer_cast<libobsensor::Frame>(resultFrame);  
    return obframe;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, depth_frame, target_color_camera_width, target_color_camera_height)

bool transformation_init_xy_tables(const ob_calibration_param calibration_param, const ob_sensor_type sensor_type, float *data,
                                                    uint32_t *data_size, ob_xy_tables *xy_tables, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(data_size);
    VALIDATE_NOT_NULL(xy_tables);

    OBCameraIntrinsic  sourceIntrinsic; 
    OBCameraDistortion depthDistortion;

    // Depending on the source sensor type, copy the corresponding intrinsic data
    if(OB_SENSOR_DEPTH == sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_DEPTH], sizeof(OBCameraIntrinsic));
        memcpy(&depthDistortion, &calibration_param.distortion[OB_SENSOR_DEPTH], sizeof(OBCameraDistortion));
    } else if(OB_SENSOR_COLOR == sensor_type) {
        memcpy(&sourceIntrinsic, &calibration_param.intrinsics[OB_SENSOR_COLOR], sizeof(OBCameraIntrinsic)); 
        memcpy(&depthDistortion, &calibration_param.distortion[OB_SENSOR_COLOR], sizeof(OBCameraDistortion));
    } else {
        return false;
    }
    return libobsensor::CoordinateUtil::transformationInitXYTables(sourceIntrinsic, depthDistortion, data, data_size, xy_tables);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, calibration_param, sensor_type, data, data_size, xy_tables)

void transformation_depth_to_pointcloud(ob_xy_tables *xy_tables, const void *depth_image_data, void *pointcloud_data,
                                                         ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(xy_tables);
    VALIDATE_NOT_NULL(depth_image_data);
    VALIDATE_NOT_NULL(pointcloud_data);

    return libobsensor::CoordinateUtil::transformationDepthToPointCloud(xy_tables, depth_image_data, pointcloud_data);
}
HANDLE_EXCEPTIONS_NO_RETURN(xy_tables, depth_image_data, pointcloud_data)

void transformation_depth_to_rgbd_pointcloud(ob_xy_tables *xy_tables, const void *depth_image_data, const void *color_image_data,
                                                              void *pointcloud_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(xy_tables);
    VALIDATE_NOT_NULL(depth_image_data);
    VALIDATE_NOT_NULL(color_image_data);    
    VALIDATE_NOT_NULL(pointcloud_data);

    return libobsensor::CoordinateUtil::transformationDepthToRGBDPointCloud(xy_tables, depth_image_data, color_image_data, pointcloud_data);
}
HANDLE_EXCEPTIONS_NO_RETURN(xy_tables, depth_image_data, pointcloud_data)

#ifdef __cplusplus
}
#endif