# API Changes at v2.x.x

This file lists the interface names that are incompatible between the latest SDK and the version v1.x.y. The changes were concentrated in the 'device' and 'Pipeline' modules, with some C and C++ interfaces being deprecated or modified due to internal usage changes, tweaked functionality, or devices no longer being supported.

## device Module

### Reason for Incompatibility: These interfaces were originally used for research and production purposes, and external users cannot/do not need to use them. Related functions will be made into an upper-layer module based on the general communication interface of the SDK for research and production use in the future

#### C Interfaces

- **ob_device_set_structured_data_ext**
- **ob_device_get_structured_data_ext**
- **ob_device_set_raw_data**
- **ob_device_get_raw_data**
- **ob_device_get_protocol_version**
- **ob_device_get_cmd_version**
- **ob_device_write_ahb**
- **ob_device_read_ahb**
- **ob_device_write_i2c**
- **ob_device_read_i2c**
- **ob_device_write_flash**
- **ob_device_read_flash**

#### C++ Interfaces

- **writeAHB**
- **readAHB**
- **writeI2C**
- **readI2C**
- **writeFlash**
- **readFlash**
- **writeCustomerData**
- **readCustomerData**
- **setRawData**
- **getRawData**
- **setStructuredDataExt**
- **getStructuredDataExt**
- **getProtocolVersion**
- **getCmdVersion**
- **sendFile**
- **activateAuthorization**
- **loadLicense**
- **loadLicenseFromData**

### Reason for Incompatibility: These interfaces have been marked as deprecated in the version v1.x.y and are no longer supported or replaced by other functions

#### C Interfaces

- **ob_device_set_sync_config**
- **ob_device_info_usb_type**

#### C++ Interfaces

- **getSyncConfig**
- **setSyncConfig**

### Reason for Incompatibility: SDK no longer supports related devices (Femto, Robotcat, etc.)

#### C Interfaces

- **ob_device_send_file_to_destination**
- **ob_device_activate_authorization**
- **ob_device_write_authorization_code**

#### C++ Interfaces

- **sendFile**
- **activateAuthorization**
- **writeAuthorizationCode**

### Reason for Incompatibility: The filter function has been adjusted, and after the filter is created, it will be configured and used by the application layer. These interfaces are no longer needed

#### C Interfaces

- **ob_device_load_depth_filter_config**
- **ob_device_reset_default_depth_filter_config**

#### C++ Interfaces

- **loadDepthFilterConfig**
- **resetDefaultDepthFilterConfig**

## Pipeline Module

### Reason for Incompatibility: The recording and playback functionality is currently not supported and will need to be redesigned in future versions

#### C Interfaces

- **ob_create_pipeline_with_playback_file**
- **ob_pipeline_get_playback**
- **ob_pipeline_start_record**
- **ob_pipeline_stop_record**

#### C++ Interfaces

- **Pipeline(const char \*filename);**
- **getPlayback**
- **startRecord**
- **stopRecord**

### Reason for Incompatibility: These are non-essential functions previously used for debugging and are not formal product features. The interface design is also considered unreasonable

#### C Interfaces

- **ob_get_d2c_valid_area**
- **ob_get_d2c_range_valid_area**

#### C++ Interfaces

- **getD2CValidArea**
