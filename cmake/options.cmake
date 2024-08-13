# target options
option(OB_BUILD_EXAMPLES "Build SDK examples" ON)
option(OB_BUILD_TESTS "Build tests" OFF)
option(OB_BUILD_TOOLS "Build tools" OFF)
option(OB_BUILD_DOCS "Build api document and install doc" ON)

# platform options
option(OB_BUILD_ANDROID "Build Android " OFF)

# platform abstract layer options
option(OB_BUILD_USB_PAL "Enable this to support USB/UVC/HID communication" ON)
option(OB_BUILD_NET_PAL "Enable this to support network/GVCP/RTSP/RTP communication" ON)

# install options
option(OB_INSTALL_EXAMPLES_SOURCE "Install SDK examples source files" ON)
option(OB_INSTALL_FILTER_DEV_HEADERS "Install HEADER files for filter development" OFF)
