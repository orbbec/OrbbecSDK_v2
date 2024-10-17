# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.5)

add_custom_target(extensions ALL)

add_custom_command(
    TARGET extensions
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/extensions/filters")

add_custom_command(
    TARGET extensions
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/extensions/frameprocessor")

if(NOT APPLE)
    add_custom_command(
        TARGET extensions
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/extensions/depthengine")
endif()

add_custom_command(
    TARGET extensions
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/extensions/firmwareupdater")

set_target_properties(extensions PROPERTIES FOLDER "modules")

if(OB_IS_MAIN_PROJECT)
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}/" DESTINATION "bin/extensions/filters")
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/filters/${OB_CURRENT_OS}/" DESTINATION "lib/extensions/filters")

    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}/" DESTINATION "bin/extensions/frameprocessor")
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/frameprocessor/${OB_CURRENT_OS}/" DESTINATION "lib/extensions/frameprocessor")

if(NOT APPLE)
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}/" DESTINATION "bin/extensions/depthengine")
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/depthengine/${OB_CURRENT_OS}/" DESTINATION "lib/extensions/depthengine")
endif()

    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}/" DESTINATION "bin/extensions/firmwareupdater")
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/firmwareupdater/${OB_CURRENT_OS}/" DESTINATION "lib/extensions/firmwareupdater")
endif()
