// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Advanced.h"

#include "ImplTypes.hpp"

#include "IDepthWorkModeManager.hpp"
#include "IPresetManager.hpp"
#include "IDevice.hpp"
#include "IDeviceComponent.hpp"
#include "IFrameInterleaveManager.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ob_depth_work_mode ob_device_get_current_depth_work_mode(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto               workModeMgr = device->device->getComponentT<libobsensor::IDepthWorkModeManager>(libobsensor::OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto               checksum    = workModeMgr->getCurrentDepthWorkMode();
    ob_depth_work_mode work_mode;
    memcpy(work_mode.checksum, checksum.checksum, sizeof(checksum.checksum));
    memcpy(work_mode.name, checksum.name, sizeof(work_mode.name));
    return work_mode;
}
HANDLE_EXCEPTIONS_AND_RETURN({}, device)

const char *ob_device_get_current_depth_work_mode_name(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto  workModeMgr = device->device->getComponentT<libobsensor::IDepthWorkModeManager>(libobsensor::OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto &checksum    = workModeMgr->getCurrentDepthWorkMode();
    return checksum.name;
}
HANDLE_EXCEPTIONS_AND_RETURN({}, device)

ob_status ob_device_switch_depth_work_mode(ob_device *device, const ob_depth_work_mode *work_mode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(work_mode);

    auto workModeMgr = device->device->getComponentT<libobsensor::IDepthWorkModeManager>(libobsensor::OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    workModeMgr->switchDepthWorkMode(work_mode->name);
    return OB_STATUS_OK;
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_STATUS_ERROR, device, work_mode)

ob_status ob_device_switch_depth_work_mode_by_name(ob_device *device, const char *mode_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(mode_name);

    auto workModeMgr = device->device->getComponentT<libobsensor::IDepthWorkModeManager>(libobsensor::OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    workModeMgr->switchDepthWorkMode(mode_name);
    return OB_STATUS_OK;
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_STATUS_ERROR, device, mode_name)

ob_depth_work_mode_list *ob_device_get_depth_work_mode_list(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto workModeMgr   = device->device->getComponentT<libobsensor::IDepthWorkModeManager>(libobsensor::OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    auto workModeList  = workModeMgr->getDepthWorkModeList();
    auto impl          = new ob_depth_work_mode_list();
    impl->workModeList = workModeList;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

uint32_t ob_depth_work_mode_list_get_count(const ob_depth_work_mode_list *work_mode_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(work_mode_list);
    return static_cast<uint32_t>(work_mode_list->workModeList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, work_mode_list)

ob_depth_work_mode ob_depth_work_mode_list_get_item(const ob_depth_work_mode_list *work_mode_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(work_mode_list);
    VALIDATE_UNSIGNED_INDEX(index, work_mode_list->workModeList.size());
    auto               workMode = work_mode_list->workModeList.at(index);
    ob_depth_work_mode impl;
    memcpy(impl.checksum, workMode.checksum, sizeof(workMode.checksum));
    memcpy(impl.name, workMode.name, sizeof(workMode.name));
    if(workMode.optionCode & CUSTOM_DEPTH_MODE_TAG) {
        impl.tag = OB_CUSTOM_DEPTH_WORK_MODE;
    }
    else {
        impl.tag = OB_DEVICE_DEPTH_WORK_MODE;
    }

    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN({}, work_mode_list, index)

void ob_delete_depth_work_mode_list(ob_depth_work_mode_list *work_mode_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(work_mode_list);
    delete work_mode_list;
}
HANDLE_EXCEPTIONS_NO_RETURN(work_mode_list)

const char *ob_device_get_current_preset_name(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);
    return presetMgr->getCurrentPresetName().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_device_load_preset(ob_device *device, const char *preset_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(preset_name);

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);

    presetMgr->loadPreset(preset_name);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, preset_name)

void ob_device_load_preset_from_json_file(ob_device *device, const char *json_file_path, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(json_file_path);

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);
    presetMgr->loadPresetFromJsonFile(json_file_path);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, json_file_path)

void ob_device_load_preset_from_json_data(ob_device *device, const char *presetName, const uint8_t *data, uint32_t size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data);

    auto                 presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);
    std::vector<uint8_t> vecData(data, data + size);
    presetMgr->loadPresetFromJsonData(presetName, vecData);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, data)

void ob_device_export_current_settings_as_preset_json_file(ob_device *device, const char *json_file_path, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(json_file_path);

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);
    presetMgr->exportSettingsAsPresetJsonFile(json_file_path);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, json_file_path)

void ob_device_export_current_settings_as_preset_json_data(ob_device *device, const char *presetName, const uint8_t **data, uint32_t *dataSize,
                                                           ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto        presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER);
    const auto &jsonData  = presetMgr->exportSettingsAsPresetJsonData(presetName);
    *data                 = (const uint8_t *)jsonData.data();
    *dataSize             = (uint32_t)jsonData.size();
}
HANDLE_EXCEPTIONS_NO_RETURN(device, presetName)

ob_device_preset_list *ob_device_get_available_preset_list(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto impl = new ob_device_preset_list();

    auto presetMgr = device->device->getComponentT<libobsensor::IPresetManager>(libobsensor::OB_DEV_COMPONENT_PRESET_MANAGER, false);
    if(presetMgr) {
        auto presetList  = presetMgr->getAvailablePresetList();
        impl->presetList = presetList;
    }

    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_delete_preset_list(ob_device_preset_list *preset_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(preset_list);
    delete preset_list;
}
HANDLE_EXCEPTIONS_NO_RETURN(preset_list)

uint32_t ob_device_preset_list_get_count(const ob_device_preset_list *preset_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(preset_list);
    return static_cast<uint32_t>(preset_list->presetList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, preset_list)

const char *ob_device_preset_list_get_name(const ob_device_preset_list *preset_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(preset_list);
    VALIDATE_UNSIGNED_INDEX(index, preset_list->presetList.size());
    return preset_list->presetList.at(index).c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, preset_list, index)

bool ob_device_preset_list_has_preset(const ob_device_preset_list *preset_list, const char *preset_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(preset_list);
    VALIDATE_NOT_NULL(preset_name);
    for(auto &name: preset_list->presetList) {
        if(name == preset_name) {
            return true;
        }
    }
    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN(false, preset_list, preset_name)

bool ob_device_is_frame_interleave_supported(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    return device->device->isComponentExists(libobsensor::OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_device_load_frame_interleave(ob_device *device, const char *frame_interleave_name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(frame_interleave_name);
    auto frameInterleaveMgr = device->device->getComponentT<libobsensor::IFrameInterleaveManager>(libobsensor::OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER);

    frameInterleaveMgr->loadFrameInterleave(frame_interleave_name);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, frame_interleave_name)

ob_device_frame_interleave_list *ob_device_get_available_frame_interleave_list(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto impl = new ob_device_frame_interleave_list();

    auto frameInterleaveMgr =
        device->device->getComponentT<libobsensor::IFrameInterleaveManager>(libobsensor::OB_DEV_COMPONENT_FRAME_INTERLEAVE_MANAGER, false);
    if(frameInterleaveMgr) {
        auto frameInterleaveList  = frameInterleaveMgr->getAvailableFrameInterleaveList();
        impl->frameInterleaveList = frameInterleaveList;
    }

    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_delete_frame_interleave_list(ob_device_frame_interleave_list *frame_interleave_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(frame_interleave_list);
    delete frame_interleave_list;
}
HANDLE_EXCEPTIONS_NO_RETURN(frame_interleave_list)

uint32_t ob_device_frame_interleave_list_get_count(ob_device_frame_interleave_list *frame_interleave_list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(frame_interleave_list);
    return static_cast<uint32_t>(frame_interleave_list->frameInterleaveList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, frame_interleave_list)

const char *ob_device_frame_interleave_list_get_name(ob_device_frame_interleave_list *frame_interleave_list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(frame_interleave_list);
    VALIDATE_UNSIGNED_INDEX(index, frame_interleave_list->frameInterleaveList.size());
    return frame_interleave_list->frameInterleaveList.at(index).c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, frame_interleave_list, index)

bool ob_device_frame_interleave_list_has_frame_interleave(ob_device_frame_interleave_list *frame_interleave_list, const char *frame_interleave_name,
                                                          ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(frame_interleave_list);
    VALIDATE_NOT_NULL(frame_interleave_name);
    for(auto &name: frame_interleave_list->frameInterleaveList) {
        if(name == frame_interleave_name) {
            return true;
        }
    }
    return false;
}
HANDLE_EXCEPTIONS_AND_RETURN(false, frame_interleave_list, frame_interleave_name)

#ifdef __cplusplus
}
#endif
