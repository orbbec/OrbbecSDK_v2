# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

add_custom_target(extensions ALL)

if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}")
    add_custom_command(
        TARGET extensions
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}" "${OB_RESOURCES_OUTPUT_DIRECTORY}/extensions/filters")
endif()

if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}")
    add_custom_command(
        TARGET extensions
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}" "${OB_RESOURCES_OUTPUT_DIRECTORY}/extensions/frameprocessor")
endif()

if(NOT APPLE AND (NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}"))
    add_custom_command(
        TARGET extensions
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}" "${OB_RESOURCES_OUTPUT_DIRECTORY}/extensions/depthengine")
endif()

if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}")
    add_custom_command(
        TARGET extensions
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}" "${OB_RESOURCES_OUTPUT_DIRECTORY}/extensions/firmwareupdater")
endif()

set_target_properties(extensions PROPERTIES FOLDER "modules")

if(OB_IS_MAIN_PROJECT)

    if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}")
        install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}/" DESTINATION "${OB_EXTENSIONS_INSTALL_RPEFIX}/extensions/filters")
    endif()

    if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}")
        install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}/" DESTINATION "${OB_EXTENSIONS_INSTALL_RPEFIX}/extensions/frameprocessor")
    endif()

if(NOT APPLE AND (NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}"))
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}/" DESTINATION "${OB_EXTENSIONS_INSTALL_RPEFIX}/extensions/depthengine")
endif()

    if(NOT OB_BUILD_LINUX_RISCV64 OR IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}")
        install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}/" DESTINATION "${OB_EXTENSIONS_INSTALL_RPEFIX}/extensions/firmwareupdater")
    endif()

endif()
