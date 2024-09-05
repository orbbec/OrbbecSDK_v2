# target options
option(OB_BUILD_EXAMPLES "Build SDK examples" ON)
option(OB_BUILD_TESTS "Build tests" OFF)
option(OB_BUILD_TOOLS "Build tools" OFF)
option(OB_BUILD_DOCS "Build api document and install doc" ON)

# platform options
option(OB_BUILD_ANDROID "Build Android " OFF)

# platform abstract layer options
option(OB_BUILD_USB_PAL "Enable this to support USB/UVC/HID communication" ON)
<<<<<<< HEAD
option(OB_BUILD_NET_PAL "Enable this to support network/GVCP/RTSP/RTP communication" ON)
=======
option(OB_BUILD_NET_PAL "Enable this to support network/GVCP/RTSP communication" ON)
option(OB_BUILD_GMSL_PAL "Enable this to support GMSL communication" ON)
>>>>>>> develop

# install options
option(OB_INSTALL_EXAMPLES_SOURCE "Install SDK examples source files" ON)
option(OB_INSTALL_FILTER_DEV_HEADERS "Install HEADER files for filter development" OFF)

if(OB_BUILD_USB_PAL)
    add_definitions(-DBUILD_USB_PAL)
endif()

if(OB_BUILD_NET_PAL)
    add_definitions(-DBUILD_NET_PAL)
endif()

if(OB_BUILD_GMSL_PAL)
    if(NOT OB_BUILD_USB_PAL)
        message(FATAL_ERROR "GMSL PAL requires USB PAL to be enabled")
    endif()
    add_definitions(-DBUILD_GMSL_PAL)
endif()
