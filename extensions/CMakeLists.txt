# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.10)

add_custom_target(extensions ALL)

function(ob_package_extension extension_name)
    set(extension_source "${CMAKE_CURRENT_LIST_DIR}/${extension_name}/${OB_CURRENT_OS}")
    if(EXISTS "${extension_source}")
        add_custom_command(
            TARGET extensions
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${extension_source}"
                "${OB_RESOURCES_OUTPUT_DIRECTORY}/extensions/${extension_name}")

        if(OB_IS_MAIN_PROJECT)
            install(DIRECTORY "${extension_source}/"
                    DESTINATION "${OB_EXTENSIONS_INSTALL_RPEFIX}/extensions/${extension_name}")
        endif()
        message(STATUS "Extension '${extension_name}': enabled for ${OB_CURRENT_OS}")
    else()
        message(STATUS "Extension '${extension_name}': no binary for ${OB_CURRENT_OS}; skipped")
    endif()
endfunction()

if(OB_BUILD_WITH_EXTENSIONS)
    ob_package_extension(filters)
    ob_package_extension(frameprocessor)
    ob_package_extension(firmwareupdater)
    if(NOT APPLE)
        ob_package_extension(depthengine)
    endif()
else()
    message(STATUS "Proprietary SDK extensions are disabled")
endif()

if(OB_BUILD_LINUX_LOONGARCH64)
    message(STATUS
        "LoongArch64 open-source build: private filters, frame processor, firmware updater, "
        "and Femto Bolt depth engine are unavailable")
endif()

set_target_properties(extensions PROPERTIES FOLDER "modules")

# The extension binaries are intentionally optional. In particular, Orbbec does
# not currently publish LoongArch64 builds. Keep the packaging logic based on
# directory existence so future official binaries can be picked up without a
# new architecture-specific branch.

