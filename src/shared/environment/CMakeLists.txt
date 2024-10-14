# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

include(${OB_3RDPARTY_DIR}/cmrc/src/cmake_resource.cmake)

cmrc_add_resource_library(
    env_config_rc
    NAMESPACE
    ob
    WHENCE
    ./
    PREFIX
    config
    ./OrbbecSDKConfig.xml
)

add_library(ob::env_config_rc ALIAS env_config_rc)
set_target_properties(env_config_rc PROPERTIES FOLDER "dependencies")

configure_file(${CMAKE_CURRENT_LIST_DIR}/Version.hpp.in ${CMAKE_CURRENT_LIST_DIR}/Version.hpp)

if(OB_IS_MAIN_PROJECT)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/OrbbecSDKConfig.xml DESTINATION shared)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/OrbbecSDKConfig.md DESTINATION shared)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/OrbbecSDKConfig.xml DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/OrbbecSDKConfig.md DESTINATION bin)
endif()
