# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)

add_executable(ob_unit_test
    TC_CPP_01_context_test.cpp
    TC_CPP_02_discovery_test.cpp
    TC_CPP_10_frame_test.cpp
    TC_CPP_11_metadata_test.cpp
    TC_CPP_12_frame_factory_test.cpp
    TC_CPP_13_filter_test.cpp
    TC_CPP_18_playback_test.cpp
    TC_CPP_20_coordinate_test.cpp
    TC_CPP_22_version_test.cpp
    TC_CPP_23_logger_test.cpp
    TC_CPP_24_error_test.cpp
    unit_test_main.cpp
)

target_link_libraries(ob_unit_test PRIVATE ob_test_common)

# Playback/filter tests load extension DLLs (frameprocessor, filters, ...) from
# <resources>/extensions at runtime. Declare the dependency so a narrow
# `cmake --build --target ob_unit_test` (used by the hw jobs) still copies the
# extensions into the binary output directory.
add_dependencies(ob_unit_test extensions)

add_custom_command(TARGET ob_unit_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../test_config.json"
        "${OB_RESOURCES_OUTPUT_DIRECTORY}/test_config.json"
)

include(GoogleTest)
gtest_discover_tests(ob_unit_test
    PROPERTIES LABELS "unit" WORKING_DIRECTORY "${OB_RESOURCES_OUTPUT_DIRECTORY}"
    DISCOVERY_TIMEOUT 30
)

set_target_properties(ob_unit_test PROPERTIES FOLDER "tests")
if(MSVC)
    set_target_properties(ob_unit_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ob_unit_test RUNTIME DESTINATION bin)
