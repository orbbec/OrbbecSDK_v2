# How to Upgrade OpenNI Protocol Devices to UVC Protocol and API Usage Guide

## 1. Overview

The OpenNI protocol cameras use the proprietary OpenNI protocol for data transmission and require the OpenNI2 SDK for development. UVC protocol cameras represent an upgrade for certain existing OpenNI protocol devices, requiring a corresponding firmware upgraded. These upgraded cameras now support data transmission via the UVC protocol. Products that have been upgraded include the Astra Mini Pro, Astra Mini S Pro, and in the future, the Gemini E, Gemini UW, Gemini EW, DaBai Max, DaBai Max Pro, Dabai DW, Dabai DCW, Dabai DCW2, and Dabai DW2. These devices require the open-source [Orbbec SDK v2](https://github.com/orbbec/OrbbecSDK_v2) for development,The supported devices and firmware version numbers can also be found in the documentation at this link.

As the APIs of OpenNI2 SDK and Orbbec SDK v2 differ significantly, this document maps commonly used OpenNI interfaces to their Orbbec SDK v2 equivalents.If you need to switch from the OpenNI2 SDK to the Orbbec SDK v2, please refer to the [Orbbec SDK V2 C++ API User Guide](https://orbbec.github.io/docs/OrbbecSDKv2_API_User_Guide/) for the operational procedures.

## 2. Advantages of Upgrading to UVC Protocol

Switching from the OpenNI protocol to the UVC protocol offers significant advantages in three key areas: compatibility, ease of use, and efficiency.

1. Driver Dependency: Enhanced Compatibility and Usability
    - OpenNI Protocol: On platforms such as Windows, a dedicated sensor driver (Orbbec SensorDriver_v4.x.x.x.exe) must be installed. Plug-and-play is not supported, and the deployment process is complex.
    - UVC Protocol: As a standard USB Video Class protocol, UVC is natively supported by operating systems (Windows, macOS, Linux) without the need for additional drivers. This provides true plug-and-play functionality, significantly lowering the barrier for end users.
2. Data Handling: Simplified Development and Integration
    - OpenNI Protocol: Depth (Depth), infrared (IR), and other data streams are transmitted in packets via a proprietary protocol. After reception, the SDK must perform frame assembly and unpacking according to proprietary specifications.
    - UVC Protocol: Depth (Depth), infrared (IR), and other data streams are transmitted using the standard UVC protocol. Frame assembly is handled by the OS kernel’s UVC driver, and the SDK receives complete image data without the need for further assembly.
3. System Overhead: Optimized Resource Efficiency
    - OpenNI Protocol: Data frame assembly, checksum verification, and unpacking at the software level consume additional CPU resources.
    - UVC Protocol: Frame assembly is managed by the hardware (camera chip) or the kernel-level UVC driver, eliminating CPU overhead at the application layer, which improves data processing efficiency.

## 3. Upgrade OpenNI Protocol Device to UVC Protocol Device

Upgrading a device from the OpenNI protocol to the UVC protocol only requires a firmware update. Specifically, use the firmware upgrade interface provided by the OpenNI2 SDK to replace the current firmware with the target UVC protocol firmware. For the complete upgrade procedure, please refer to the "Firmware Upgrade Function" instructions in Section 4.39 of this document.

**Important Notes During the Upgrade Process:**

**i.Firmware Distinction:** 
The version numbering schemes for OpenNI protocol firmware (format: 1.x.xx) and UVC protocol firmware (format: 2.x.xx) are different. Ensure you select the correct firmware file corresponding to your target protocol before starting the upgrade.

**ii. Post-Upgrade Action on Windows:** 
On Windows, after completing the upgrade via the OpenNI2 SDK, you must first uninstall the “Orbbec SensorDriver” in Device Manager and then reconnect the device for the host to correctly recognize it as a new UVC device. This process is not user-friendly, and after a reboot, it is inconvenient to read the firmware version to verify whether the upgrade was successful. Therefore, it is recommended to perform the upgrade on Linux.

**iii. SDK Switch:** After the device has been upgraded to the UVC protocol, the original OpenNI2 SDK is no longer applicable. You must switch to the Orbbec SDK v2, which is specifically designed for the UVC protocol (Download: https://github.com/orbbec/OrbbecSDK_v2).

## 4. Mapping of Common OpenNI2 Interfaces to Orbbec SDK v2


### 4.1 SDK Initialization

**OpenNI2 SDK API :**
```cpp
Status rc = OpenNI::initialize()
```

**OrbbecSDK v2 API :**
```cpp
ob::Context ctx;
```

### 4.2 Device List Enumeration

**OpenNI2 SDK API :**
```cpp
Array<DeviceInfo> deviceInfoList;
OpenNI::enumerateDevices(&deviceInfoList);
```

**OrbbecSDK v2 API :**
```cpp
// Enumerate all connected devices through the Context.
std::shared_ptr<DeviceList> devices = ctx.queryDeviceList();
```

### 4.3 Open Device

**OpenNI2 SDK API :**
```cpp
Status rc = OpenNI::initialize();

// Method 1
// Open the first device
Device device;
Status rc = device.open(ANY_DEVICE);

// Method 2
// Open device by uri
Array<DeviceInfo> deviceInfoList;
OpenNI::enumerateDevices(&deviceInfoList);

Get the device at index 0 and open
Device device;
Status rc = device.open(deviceInfoList[0].getUri());
```

**OrbbecSDK v2 API :**
```cpp
// Create a Context
ob::Context ctx;

// Method 1
// Enumerate all connected devices through the Context.
std::shared_ptr<DeviceList> devices = ctx.queryDeviceList();
// Get the device at index 0.
auto device  = devices->getDevice(0);

// Method 2
// Enumerate all connected devices through the Context.
std::shared_ptr<DeviceList> devices = ctx.queryDeviceList();
// Get the device by serial number.
auto device = devices->getDeviceBySN("AE4M73D0040");

// Method 3
// Enumerate all connected devices through the Context.
std::shared_ptr<DeviceList> devices = ctx.queryDeviceList();
// Get the device by uid.
auto device = devices->getDeviceByUid("NDSG3958LKHY45");

// Method 4
ob::Pipeline pipe;
// Get the device by pipeline.
auto device = pipe.getDevice();
```

### 4.4 Get Depth Stream

**OpenNI2 SDK API :**
```cpp
// 1. Initialize SDK
Status rc = OpenNI::initialize();
if (rc != STATUS_OK) {
	printf("Initialize failed\n%s\n", OpenNI::getExtendedError());
	return 1;
}

// 2. Open device
// Method 1
// Open the first device
Device device;
rc = device.open(ANY_DEVICE);
if (rc != STATUS_OK) {
   printf("Couldn't open device\n%s\n", OpenNI::getExtendedError());
   return 2;
}

// 3. Create depth
VideoStream depth;
if (device.getSensorInfo(SENSOR_DEPTH) != NULL) {
	rc = depth.create(device, SENSOR_DEPTH);
	if (rc != STATUS_OK) {
		printf("Couldn't create depth stream\n%s\n", OpenNI::getExtendedError());
		return 3;
	}
}

//4. Set videomode
const Array<VideoMode>& videoModes = device.getSensorInfo(SENSOR_DEPTH)->getSupportedVideoModes();

VideoMode modeSet;
for (int i = 0; i < videomodes.getSize(); i++){
  VideoMode mode = videomodes[i];
  if (mode.getResolutionX() == 640 && mode.getResolutionY() == 400 && mode.getFps() == 30){
		modeSet = mode;
		break;
	}
}

depth.setVideoMode(modeSet);

//5. Start depth
rc = depth.start();
if (rc != STATUS_OK) {
	printf("Couldn't start the depth stream\n%s\n", OpenNI::getExtendedError());
	return 4;
}

//6. Read frame
int changedStreamDummy;
VideoFrameRef frame;
VideoStream* pStream = &depth;
while(true) {
    rc = OpenNI::waitForAnyStream(&pStream, 1, &changedStreamDummy, SAMPLE_READ_WAIT_TIMEOUT);
  if (rc != STATUS_OK) {
    printf("Wait failed! (timeout is %d ms)\n%s\n", SAMPLE_READ_WAIT_TIMEOUT, 
    OpenNI::getExtendedError());
    continue;
  }

  rc = depth.readFrame(&frame);
  if (rc != STATUS_OK) {
    printf("Read failed!\n%s\n", OpenNI::getExtendedError());
    continue;
  }

  // Get depth data
  DepthPixel* pDepth = (DepthPixel*)frame.getData();
}

// 7.stop stream
depth.stop();
depth.destroy();
// 8. Close device
device.close();
// 9. Shutdown sdk
OpenNI::shutdown();
```

**OrbbecSDK v2 API :**
```cpp
// 1.Create a pipeline with default device. or create a pipeline with device
// auto pipe = std::make_shared<ob::Pipeline>(device);
ob::Pipeline pipe;

// 2.Create config.
std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

// 3.Enable video stream. You can modify the parameters based on your usage requirements, stream format can be either 11-bit(OB_FORMAT_Y11) or 12-bit(OB_FORMAT_Y12).
config->enableVideoStream(OB_STREAM_DEPTH, 640, 400, 30, OB_FORMAT_Y11);

// 4.Start the pipeline with config.
pipe.start(config);

while(true) {
    // 5.Wait for up to 100ms for a frameset in blocking mode.
    auto frameSet = pipe.waitForFrameset(100);
    if(frameSet == nullptr) {
        continue;
    }
  
    // Get the depth frame.
    auto depthFrameRaw = frameSet->getFrame(OB_FRAME_DEPTH);
    if(depthFrameRaw) {
      // Get the depth Frame form depthFrame,process depth data.
      auto depthFrame = depthFrameRaw->as<ob::DepthFrame>();
      uint32_t        width  = depthFrame->getWidth();
      uint32_t        height = depthFrame->getHeight();
      const uint16_t *data   = reinterpret_cast<const uint16_t *>(depthFrame->getData());
    }
}

// 6.Stop the pipeline
pipe.stop();
```

### 4.5 Get IR Stream

**OpenNI2 SDK API :**
```cpp
// 1. Initialize SDK
Status rc = OpenNI::initialize();
if (rc != STATUS_OK) {
	printf("Initialize failed\n%s\n", OpenNI::getExtendedError());
	return 1;
}

// 2. Open device
// Open the first device
Device device;
rc = device.open(ANY_DEVICE);
if (rc != STATUS_OK) {
   printf("Couldn't open device\n%s\n", OpenNI::getExtendedError());
   return 2;
}

// 3. Create ir stream
VideoStream ir;
if (device.getSensorInfo(SENSOR_IR) != NULL) {
	rc = ir.create(device, SENSOR_IR);
	if (rc != STATUS_OK) {
		printf("Couldn't create ir stream\n%s\n", OpenNI::getExtendedError());
		return 3;
	}
}

// 4. Set videomode
const Array<VideoMode>& videoModes = device.getSensorInfo(SENSOR_IR)->getSupportedVideoModes();

VideoMode modeSet;
for (int i = 0; i < videomodes.getSize(); i++){
  VideoMode mode = videomodes[i];
  if (mode.getResolutionX() == 640 && mode.getResolutionY() == 400 && mode.getFps() == 30){
		modeSet = mode;
		break;
	}
}
ir.setVideoMode(modeSet);

// 5. Start stream
rc = ir.start();
if (rc != STATUS_OK) {
	printf("Couldn't start the ir stream\n%s\n", OpenNI::getExtendedError());
	return 4;
}

// 6. Read frame
int changedStreamDummy;
VideoFrameRef frame;
VideoStream* pStream = &ir;
while(true) {
    rc = OpenNI::waitForAnyStream(&pStream, 1, &changedStreamDummy, SAMPLE_READ_WAIT_TIMEOUT);
  if (rc != STATUS_OK) {
    printf("Wait failed! (timeout is %d ms)\n%s\n", SAMPLE_READ_WAIT_TIMEOUT, 
    OpenNI::getExtendedError());
    continue;
  }

  rc = ir.readFrame(&frame);
  if (rc != STATUS_OK) {
    printf("Read failed!\n%s\n", OpenNI::getExtendedError());
    continue;
  }

  // Get IR data
  Grayscale16Pixel* pIR = (Grayscale16Pixel*)frame.getData();
}

// 7. Stop frame
ir.stop();
ir.destroy();
// 8. Close device
device.close();
// 9. Shutdown SDK
OpenNI::shutdown();
```

**OrbbecSDK v2 API :**
```cpp
// 1.Create a pipeline with default device. or create a pipeline with device
// auto pipe = std::make_shared<ob::Pipeline>(device);
ob::Pipeline pipe;

// 2.Create config.
std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

// 3.Enable video stream. You can modify the parameters based on your usage requirements
config->enableVideoStream(OB_STREAM_IR, 640, 400, 30, OB_FORMAT_Y10);

// 4.Start the pipeline with config.
pipe.start(config);

while(true) {
    // 5.Wait for up to 100ms for a frameset in blocking mode.
    auto frameSet = pipe.waitForFrameset(100);
    if(frameSet == nullptr) {
        continue;
    }
  
    // Get the ir frame.
    auto irFrameRaw = frameSet->getFrame(OB_FRAME_IR);
    if(irFrameRaw) {
      // Get the ir Frame form IRFrame,process ir data.
      auto irFrame = irFrameRaw->as<ob::IRFrame>();
      uint32_t        width  = irFrame->getWidth();
      uint32_t        height = irFrame->getHeight();
      const uint16_t *data   = reinterpret_cast<const uint16_t *>(irFrame->getData());
    }
}

// 6.Stop the pipeline
pipe.stop();
```

### 4.6 Get Color Stream

**OpenNI2 SDK API :**
```cpp
// 1. Initialize SDK
Status rc = OpenNI::initialize();
if (rc != STATUS_OK) {
	printf("Initialize failed\n%s\n", OpenNI::getExtendedError());
	return 1;
}

// 2. Open device
// Method 1
// Open the first device
Device device;
rc = device.open(ANY_DEVICE);
if (rc != STATUS_OK) {
   printf("Couldn't open device\n%s\n", OpenNI::getExtendedError());
   return 2;
}

// 3. Create color stream
VideoStream color;
if (device.getSensorInfo(SENSOR_COLOR) != NULL) {
	rc = color.create(device, SENSOR_COLOR);
	if (rc != STATUS_OK) {
		printf("Couldn't create color stream\n%s\n", OpenNI::getExtendedError());
		return 3;
	}
}

// 4. Set videomode
const Array<VideoMode>& videoModes = device.getSensorInfo(SENSOR_COLOR)->getSupportedVideoModes();

VideoMode modeSet;
for (int i = 0; i < videomodes.getSize(); i++){
  VideoMode mode = videomodes[i];
  if (mode.getResolutionX() == 640 && mode.getResolutionY() == 400 && mode.getFps() == 30){
		modeSet = mode;
		break;
	}
}
color.setVideoMode(modeSet);

// 5. Start stream
rc = color.start();
if (rc != STATUS_OK) {
	printf("Couldn't start the color stream\n%s\n", OpenNI::getExtendedError());
	return 4;
}

// 6. Read frame
int changedStreamDummy;
VideoFrameRef frame;
VideoStream* pStream = &color;
while(true) {
    rc = OpenNI::waitForAnyStream(&pStream, 1, &changedStreamDummy, SAMPLE_READ_WAIT_TIMEOUT);
  if (rc != STATUS_OK) {
    printf("Wait failed! (timeout is %d ms)\n%s\n", SAMPLE_READ_WAIT_TIMEOUT, 
    OpenNI::getExtendedError());
    continue;
  }

  rc = color.readFrame(&frame);
  if (rc != STATUS_OK) {
    printf("Read failed!\n%s\n", OpenNI::getExtendedError());
    continue;
  }

  // Get color data
  RGB888Pixel* pColor = (RGB888Pixel*)frame.getData();
}

// 7. Stop frame
color.stop();
color.destroy();
// 8. Close device
device.close();
// 9. Shutdown SDK
OpenNI::shutdown();
```

**OrbbecSDK v2 API :**
```cpp
// 1.Create a pipeline with default device. or create a pipeline with device
// auto pipe = std::make_shared<ob::Pipeline>(device);
ob::Pipeline pipe;

// 2.Create config.
std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

// 3.Enable video stream. You can modify the parameters based on your usage requirements
config->enableVideoStream(OB_STREAM_COLOR, 640, 400, 30, OB_FORMAT_MJPG);

// 4.Start the pipeline with config.
pipe.start(config);

while(true) {
    // 5.Wait for up to 100ms for a frameset in blocking mode.
    auto frameSet = pipe.waitForFrameset(100);
    if(frameSet == nullptr) {
        continue;
    }
  
    // Get the color frame.
    auto colorFrameRaw = frameSet->getFrame(OB_FRAME_COLOR);
    if(colorFrameRaw) {
      // Get the color frame, process color data.
      auto colorFrame = colorFrameRaw->as<ob::ColorFrame>();
      uint32_t width  = colorFrame->getWidth();
      uint32_t height = colorFrame->getHeight();
      auto *data      = colorFrame->getData();
    }
}

// 6.Stop the pipeline
pipe.stop();
```

### 4.7 Stop Stream

**OpenNI2 SDK API :**
```cpp
Status rc = videoStream.stop();
```

**OrbbecSDK v2 API :**
```cpp
pipe.stop();
```

### 4.8 Destroy Stream

**OpenNI2 SDK API :**
```cpp
videoStream.destroy();
```

**OrbbecSDK v2 API :**
```cpp
pipe.stop();
```

### 4.9 Close Device

**OpenNI2 SDK API :**
```cpp
Device.close();
```

**OrbbecSDK v2 API :**

No corresponding interface is provided. Data streams are managed via `ob::Pipeline`, and can be stopped simply by calling `pipe.stop()`.

### 4.10 Destroy SDK

**OpenNI2 SDK API :**
```cpp
OpenNI::shutdown();
```

**OrbbecSDK v2 API :**

Create a global variable or smart pointer for `ob::Context`，explicit destruction is not required.

### 4.11 Device Disconnection Status Callback

**OpenNI2 SDK API :**
```cpp
class OpenNIDeviceListener : public OpenNI::DeviceConnectedListener,
	public OpenNI::DeviceDisconnectedListener,
	public OpenNI::DeviceStateChangedListener
{
public:
	virtual void onDeviceStateChanged(const DeviceInfo* pInfo, DeviceState state)
	{

	}

	virtual void onDeviceConnected(const DeviceInfo* pInfo){
		printf("Device \"%s\" connected\n", pInfo->getUri());
		if (obDevieList.size() != 0)
		{
			
		}
	}

	virtual void onDeviceDisconnected(const DeviceInfo* pInfo){
		printf("Device \"%s\" disconnected\n", pInfo->getUri());
		if (obDevieList.size() != 0)
		{
			
		}
	}
};

OpenNIDeviceListener* devicePrinter = NULL;
Status rc = OpenNI::initialize();
if (rc != STATUS_OK){
	printf("Initialize failed\n%s\n", OpenNI::getExtendedError());
	return 1;
}

devicePrinter = new OpenNIDeviceListener();
OpenNI::addDeviceConnectedListener(devicePrinter);
OpenNI::addDeviceDisconnectedListener(devicePrinter);
OpenNI::addDeviceStateChangedListener(devicePrinter);
```

**OrbbecSDK v2 API :**
```cpp
// create context
ob::Context ctx;

// register device callback
ctx.setDeviceChangedCallback([](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList> deviceList) {
    if（deviceList->deviceCount() > 0）{

    }

    if（removedList->deviceCount() > 0）{

    }
});
```

### 4.12 Get Device Information

**OpenNI2 SDK API :**
```cpp
const DeviceInfo& deviceInfo = device.getDeviceInfo();
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get the device from pipeline.
std::shared_ptr<ob::Device> device = pipe.getDevice();
// Get deviceInfo
auto deviceInfo = device->getDeviceInfo();
```

### 4.13 Enable Hardware D2C

**OpenNI2 SDK API :**
```cpp
typedef enum
{
    //Close haedware D2C
	IMAGE_REGISTRATION_OFF				= 0,
    //Open haedware D2C
	IMAGE_REGISTRATION_DEPTH_TO_COLOR	= 1,
} ImageRegistrationMode;
// API
Status openni::Device::setImageRegistrationMode(ImageRegistrationMode mode)；
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable hardware D2C
device->setBoolProperty(OB_PROP_DEPTH_ALIGN_HARDWARE_BOOL, true);
```

### 4.14 Get Serial Number

**OpenNI2 SDK API :**
```cpp
char serNumber[12]={0};
int dataSize=sizeof(serNumber);
openni::Status rc = Device.getProperty(OBEXTENSION_ID_SERIALNUMBER, (uint8_t *)&serNumber,&dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get deviceInfo
auto deviceInfo = device->getDeviceInfo();
// Get serial number
auto sn = deviceInfo->getSerialNumber();
```

### 4.15 Get Camera Calibration Parameters

**OpenNI2 SDK API :**
```cpp
// 1.Calibration parameter structure:
typedef struct OBCameraParams
{
      float l_intr_p[4];
      float r_intr_p[4];
      float r2l_r[9];
      float r2l_t[3];
      float l_k[5];
      float r_k[5];
}OBCameraParams;
// 2.API, Applicable to Astra Mini Pro and Astra Mini S Pro.
OBCameraParams cameraParam;
int dataSize = sizeof(cameraParam);
memset(&cameraParam, 0, sizeof(cameraParam));
openni::Status rc = Device.getProperty(openni::OBEXTENSION_ID_CAM_PARAMS, (uint8_t *)&cameraParam, &dataSize);

// 3.API,Applicable to others.
OBCameraParamList cameraParamList;
memset(&cameraParamList, 0, sizeof(cameraParamList));
int dataSize = sizeof(cameraParamList);

const uint8_t  maxSize = 4;
OBCameraParam cameraParams[maxSize];
cameraParamList.pCameraParams = (OBCameraParam*)cameraParams;

openni::Status rc = Device.getProperty(openni::OBEXTENSION_ID_CAM_PARAMS, (uint8_t *)&cameraParamList, &dataSize);
if (rc != openni::STATUS_OK) {
	printf("Error: %s\n", openni::OpenNI::getExtendedError());
}
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get calibration camera param list
auto CameraParamList = device->getCalibrationCameraParamList();
auto count           = CameraParamList->count();
// Get cameraParam
for(int i = 0; i < count; i++) {
    OBCameraParam cameraParam = CameraParamList->getCameraParam(0);
}
```

### 4.16 Get IR Gain

**OpenNI2 SDK API :**
```cpp
int gain = 0;
int dataSize = 4;
openni::Status rc = Device.getProperty(OBEXTENSION_ID_IR_GAIN, (uint8_t*)&gain, &dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get ir gain
int32_t gainValue = device->getIntProperty(OB_PROP_IR_GAIN_INT);
```

### 4.17 Set IR Gain

**OpenNI2 SDK API :**
```cpp
int gain = 1000;
int dataSize = 4;
rc = Device.setProperty(OBEXTENSION_ID_IR_GAIN, (uint8_t*)&gain, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Set ir gain.
device->setIntProperty(OB_PROP_IR_GAIN_INT, gain);
```

### 4.18 Get IR Exposure

**OpenNI2 SDK API :**
```cpp
int exposure = 0;
int dataSize = 4;
openni::Status rc = Device.getProperty(OBEXTENSION_ID_IR_EXP, (uint8_t*)&exposure, &dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get ir exposure
int32_t gainValue = device->getIntProperty(OB_PROP_IR_EXPOSURE_INT);
```

### 4.19 Set IR Exposure

**OpenNI2 SDK API :**
```cpp
int exposure = 500;
int dataSize = 4;
rc = Device.setProperty(OBEXTENSION_ID_IR_EXP, (uint8_t*)&exposure, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Set exposure
device->setIntProperty(OB_PROP_IR_EXPOSURE_INT, 1000); 
```

### 4.20 Set Color AE

**OpenNI2 SDK API :**
```cpp
//Through CameraSettings, some devices like Astra mini pro, Astra mini s pro, and color ae adjustment use the OpenNI protocol for transmission.
CameraSettings* cameraSetting = colorStream.getCameraSettings();
// Set true or false to enable ae
cameraSetting->setAutoExposureEnabled(true);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Set true or false to enable ae
device->setBoolProperty(OB_PROP_COLOR_AUTO_EXPOSURE_BOOL, true);
```

### 4.21 Set Color Gain

**OpenNI2 SDK API :**
```cpp
//Through CameraSettings, some devices like Astra mini pro, Astra mini s pro, and color ae adjustment use the OpenNI protocol for transmission.
CameraSettings* cameraSetting = colorStream.getCameraSettings();
//Set color gain. gain: the specific gain value
cameraSetting->setGain(gain);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
//Set color gain. gain: the specific gain value
device->setIntProperty(OB_PROP_COLOR_GAIN_INT, gain);
```


### 4.22 Set Color Exposure

**OpenNI2 SDK API :**
```cpp
//Through CameraSettings, some devices like Astra mini pro, Astra mini s pro, and color ae adjustment use the OpenNI protocol for transmission.
CameraSettings* cameraSetting = colorStream.getCameraSettings();
//set exposure
cameraSetting->setExposure(1000);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
//set exposure
device->setIntProperty(OB_PROP_COLOR_EXPOSURE_INT, 1000); 
```

### 4.23 Enable Laser

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
//1: represents turning on the laser; 0: represents turning off the laser
int laser_en = 1; 
openni::Status rc = Device.setProperty(XN_MODULE_PROPERTY_EMITTER_STATE, (uint8_t*)&laser_en,dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable laser, true or false
device->setBoolProperty(OB_PROP_LASER_BOOL, true);
```

### 4.24 Get Laser Status

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
int laser_en = 0;
rc = Device.getProperty(XN_MODULE_PROPERTY_EMITTER_STATE_V1, (uint8_t*)&laser_en, &dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get laser status
bool laserStatus = device->getBoolProperty(OB_PROP_LASER_BOOL);
```

### 4.25 Enable Flood

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
//1: represents turning on the floodlight; 0: represents turning off the floodlight
int flood_en = 1; 
openni::Status rc =Device.setProperty(XN_MODULE_PROPERTY_IRFLOOD_STATE, (uint8_t *)&flood_en, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable flood
device->setBoolProperty(OB_PROP_FLOOD_BOOL, true);
```

### 4.26 Get Flood Status

**OpenNI2 SDK API :**
```cpp
int flood_en = 0;
int dataSize = 4;
openni::Status rc= Device.getProperty(XN_MODULE_PROPERTY_IRFLOOD_STATE, (uint8_t *)&flood_en, &dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get flood status
bool laserStatus = device->getBoolProperty(OB_PROP_FLOOD_BOOL);
```

### 4.27 Enable LDP

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
// 0: Indicates turning off LDP, 1: Indicates turning on LDP.
int ldp_enable = 1; 
openni::Status rc = Device.setProperty(XN_MODULE_PROPERTY_LDP_ENABLE,(uint8_t*)&ldp_enable,dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable ldp
device->setBoolProperty(OB_PROP_LDP_BOOL, true);
```

### 4.28 Get LDP Protection Status

**OpenNI2 SDK API :**
```cpp
XnUInt32 nValue = 0;
openni::Status rc;
rc = Device.getProperty(XN_MODULE_PROPERTY_LDP_STATUS, &nValue);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
bool status = device->getBoolProperty(OB_PROP_LDP_STATUS_BOOL);
```

### 4.29 Set Depth Minimum and Maximum Values

**OpenNI2 SDK API :**
```cpp
// The purpose of setting the minimum and maximum values for Depth is to filter out Depth points that are outside of this range. This needs to be set between the create and start functions of OpenNI. The reference code for setting this is similar to setting the Depth resolution.
// Set Depth minimum values：
int dataSize = 4;
// Set 340mm
int nMinDepth = 340; 
openni::Status rc = depth.setProperty(XN_STREAM_PROPERTY_MIN_DEPTH, (uint8_t *)&nMinDepth, dataSize);

// Set Depth maximum values：
int dataSize = 4;
// //Set 1500mm 
int nMaxDepth =1500 ; 
openni::Status rc =depth.setProperty(XN_STREAM_PROPERTY_MAX_DEPTH, (uint8_t *)&nMaxDepth, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// By creating config to configure which streams to enable or disable for the pipeline, here the depth stream will be enabled.
std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

// This is the default depth streamprofile that is enabled. If you want to modify it, you can do so in the configuration file.
config->enableVideoStream(OB_STREAM_DEPTH);

// Start the pipeline with config.
pipe.start(config);

//Create ThresholdFilter to set depth minimum and maximum values：
auto thresholdFilter = std::make_shared<ob::ThresholdFilter>();
thresholdFilter->setValueRange(340, 1500);

while(true) {
  // Wait for up to 100ms for a frameset in blocking mode.
  auto frameSet = pipe.waitForFrameset(100);
  if(frameSet == nullptr) {
      continue;
  }

  // Get the depth frame raw from frameset.
  auto depthFrameRaw = frameSet->getFrame(OB_FRAME_DEPTH);
  if(!depthFrameRaw) {
      continue;
  }

  // After processing by the thresholdFilter, return the new depth frame with the maximum and minimum values.
  auto newDepthFrame = thresholdFilter->process(depthFrameRaw);
}

// Stop the pipeline
pipe.stop();
```

### 4.30 Switch Left/Right IR

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
// 0: represents left IR; 1: represents right IR
int switch_ir = 1或0; 
openni::Status rc = Device.setProperty(XN_MODULE_PROPERTY_SWITCH_IR, (uint8_t*)&switch_ir,dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// 0 : IR stream from IR Left sensor; 1 : IR stream from IR Right sensor;
device->setIntProperty(OB_PROP_IR_CHANNEL_DATA_SOURCE_INT, 0);
```

### 4.31 Set Depth Mirror

**OpenNI2 SDK API :**
```cpp
//Set depth mirror or non-mirror
Depth.setMirroringEnabled(true); 
//Set depth non-mirror
Depth.setMirroringEnabled(false); 
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable depth mirror
device->setBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL, true);
// Enable depth non-mirror
device->setBoolProperty(OB_PROP_DEPTH_MIRROR_BOOL, false);
```

### 4.32 Set IR Mirror

**OpenNI2 SDK API :**
```cpp
//Set ir mirror or non-mirror
IR.setMirroringEnabled(true); 
//Set ir non-mirror
IR.setMirroringEnabled(false); 
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable ir mirror
device->setBoolProperty(OB_PROP_IR_MIRROR_BOOL, true);
// Enable ir non-mirror
device->setBoolProperty(OB_PROP_IR_MIRROR_BOOL, false);
```

### 4.33 Set Color Mirror

**OpenNI2 SDK API :**
```cpp
//Set color mirror or non-mirror
color.setMirroringEnabled(true); 
//Set color non-mirror
color.setMirroringEnabled(false); 
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable color mirror
device->setBoolProperty(OB_PROP_COLOR_MIRROR_BOOL, true);
// Enable color non-mirror
device->setBoolProperty(OB_PROP_COLOR_MIRROR_BOOL, false);
```

### 4.34 Get Device PID

**OpenNI2 SDK API :**
```cpp
device.getDeviceInfo().getUSBPorductId();  
```

**OrbbecSDK v2 API :**
```cpp
auto pid = device->getDeviceInfo()->getPid();
```

### 4.35 Get Device Temperature

**OpenNI2 SDK API :**
```cpp
XnDouble nValue = 0;
openni::Status rc;
rc = Device.getProperty(XN_MODULE_PROPERTY_RT_IR_TEMP, &nValue);
rc = Device.getProperty(XN_MODULE_PROPERTY_RT_RIGHT_IR_TEMP, &nValue);
rc = Device.getProperty(XN_MODULE_PROPERTY_RT_LDMP_TEMP, &nValue);
rc = Device.getProperty(XN_MODULE_PROPERTY_RT_RIGHT_LDMP_TEMP, &nValue);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Get device temperature
OBDeviceTemperature deviceTemp;
uint32_t size = sizeof(OBDeviceTemperature);
device->getStructuredData(OB_STRUCT_DEVICE_TEMPERATURE, (uint8_t *)&deviceTemp, &size);
```

### 4.36 Set HoleFilter

**OpenNI2 SDK API :**
```cpp
//You need to set it between the create and start functions of OpenNI. The reference code for setting it is similar to setting the Depth resolution.

//Hole filling filter: 0 means disabling the filter function, and when enabled, the values can be 1, 2, 3, or 4 (HoleFilter represents the window filter size: 1 is 3x3, 2 is 5x5, 3 is 7x7, 4 is 9x9). It is not necessary to set it unless required.

int dataSize = 4;
int nHoleFilter = 3;
openni::Status	rc = depth.setProperty(XN_STREAM_PROPERTY_HOLE_FILTER, (uint8_t *)&nHoleFilter, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Enable holefilter true or false
device->setBoolProperty(OB_PROP_DEPTH_HOLEFILTER_BOOL, true);
```

### 4.37 Set Remove Filter MaxDiff

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
int max_diff = 16;
openni::Status	rc;
rc = rcdepth.setProperty(XN_STREAM_PROPERTY_DEPTH_MAX_DIFF, (uint8_t *)&max_diff, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto dev = pipe.getDevice();
dev->setIntProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, 16);
```

### 4.38 Set Remove Filter MaxSpeckleSize

**OpenNI2 SDK API :**
```cpp
int dataSize = 4;
int max_speckle_size = 480;
openni::Status	rc;
rc = depth.setProperty(XN_STREAM_PROPERTY_DEPTH_MAX_SPECKLE_SIZE, (uint8_t *)&max_speckle_size, dataSize);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
device->setIntProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_SPECKLE_SIZE_INT, 480);
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto dev = pipe.getDevice();
dev->setIntProperty(OB_PROP_DEPTH_NOISE_REMOVAL_FILTER_MAX_DIFF_INT, 16);
```

### 4.39 Firmware Upgrade

**OpenNI2 SDK API :**
```cpp
// 1. Initialize SDK
Status rc = OpenNI::initialize();
if (rc != STATUS_OK) {
	printf("Initialize failed\n%s\n", OpenNI::getExtendedError());
	return 1;
}

// 2. Open device
// Method 1
// Open the first device
Device device;
rc = device.open(ANY_DEVICE);
if (rc != STATUS_OK) {
   printf("Couldn't open device\n%s\n", OpenNI::getExtendedError());
   return 2;
}

// 3. Update firmware
uint8_t *buff = firmwareData;
uint32_t size = firmwareSize;
openni::Status rc=device.setProperty(openni::OBEXTENSION_ID_UPDATE_FIRMWARE, buff, size);

// 4.Reboot device
int nValue = 0;
device.setProperty(XN_MODULE_PROPERTY_SOFT_RESET, (int)nValue);

// 5. close device
device.close();

//6. Shutdown sdk
openni::OpenNI::shutdown();
```

**OrbbecSDK v2 API :**
```cpp
// 1.Create a context to access the connected devices
std::shared_ptr<ob::Context> context = std::make_shared<ob::Context>();
// 2.Get connected devices from the context
std::shared_ptr<ob::DeviceList> deviceList = context->queryDeviceList();
// 3.Get the first device.
auto device = deviceList->getDevice(0);
// 4.Set async to false to synchronously block and wait for the device firmware upgrade to complete.
devices->updateFirmware(firmwarePath, firmwareUpdateCallback, false);

// 5.Firmware upgrade status callback
void firmwareUpdateCallback(OBFwUpdateState state, const char *message, uint8_t percent) {
    switch(state) {
    case STAT_VERIFY_SUCCESS:
        std::cout << "Image file verification success" << std::endl;
        break;
    case STAT_FILE_TRANSFER:
        std::cout << "File transfer in progress" << std::endl;
        break;
    case STAT_DONE:
        std::cout << "Update completed" << std::endl;
        break;
    case STAT_IN_PROGRESS:
        std::cout << "Upgrade in progress" << std::endl;
        break;
    case STAT_START:
        std::cout << "Starting the upgrade" << std::endl;
        break;
    case STAT_VERIFY_IMAGE:
        std::cout << "Verifying image file" << std::endl;
        break;
    default:
        std::cout << "Unknown status or error" << std::endl;
        break;
    }
    std::cout << "Message : " << message << std::endl << std::flush;
}

// 6.Reboot the device after a successful upgrade.
device->reboot();
```
Notes：
If the device enters Bootloader mode, upgrading via Orbbec SDK v2 is not supported; it can only be upgraded using the OpenNI SDK.

### 4.40 Read and Write User Data

**OpenNI2 SDK API :**
```cpp
// Write customer data, length cannot exceed 32.
OBThirdCustomerSN thirdSN;
memset(&thirdSN, 0, sizeof(OBThirdCustomerSN));
std::string sn = "SMNDJHFIGU90O";
int mSnLen = (int)sn.size();
memcpy(thirdSN.SerialNumber, sn.c_str(), mSnLen);
thirdSN.size = mSnLen;

openni::Status rc = Device.setProperty(XN_MODULE_PROPERTY_DEVICE_SN, thirdSN);
if (rc != XN_STATUS_OK){
	printf("Error: %s\n", openni::OpenNI::getExtendedError());
}

// Read customer data
OBThirdCustomerSN thirdSN;
memset(&thirdSN, 0, sizeof(OBThirdCustomerSN));

openni::Status rc = Device.getProperty(XN_MODULE_PROPERTY_DEVICE_SN, &thirdSN);
if (rc != XN_STATUS_OK) {
	printf("Error: %s\n", openni::OpenNI::getExtendedError());
}
```

**OrbbecSDK v2 API :**
```cpp
// Write customer data, length cannot exceed 32.
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
uint8_t writeData[] = { "SMNDJHFIGU90O" };
uint32_t dataLen     = static_cast<uint32_t>(sizeof(writeData));
device->writeCustomerData(writeData, dataLen);

// Read customer data.
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
uint8_t readData[32];
uint32_t dataLen = 0;
device->readCustomerData(readData, &dataLen);
```

### 4.41 Device Reboot

**OpenNI2 SDK API :**
```cpp
XnUInt32 nValue = 0;
openni::Status rc = Device.setProperty(XN_MODULE_PROPERTY_SOFT_RESET, (XnUInt32)nValue);
if (rc != openni::STATUS_OK) {
	printf("Error: %s\n", openni::OpenNI::getExtendedError());
}
```

**OrbbecSDK v2 API :**
```cpp
// Create a pipeline with default device.
ob::Pipeline pipe;
// Get device
auto device = pipe.getDevice();
// Reboot device
device->reboot();
```