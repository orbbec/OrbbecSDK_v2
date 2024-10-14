# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

if(OB_IS_MAIN_PROJECT)
    if(UNIX)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/env_setup/99-obsensor-libusb.rules DESTINATION shared)
        install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/env_setup/install_udev_rules.sh DESTINATION shared)
        install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/env_setup/setup.sh DESTINATION ./)
    elseif(WIN32)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/env_setup/obsensor_metadata_win10.md DESTINATION shared)
        install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/env_setup/obsensor_metadata_win10.ps1 DESTINATION shared)
    endif()
endif()
