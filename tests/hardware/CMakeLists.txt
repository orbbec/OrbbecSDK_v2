cmake_minimum_required(VERSION 3.13)

# ============================================================================
# Destructive test case names — the SINGLE source of truth.
#
# These five tests (force-IP, hotplug reboot, sensor-after-reboot, firmware
# update, depth-preset update) share the same source files as the rest of the
# hw suite but must never run inside the smoke binary. Two gtest filters are
# derived from this one list so the names can never drift out of sync:
#
#   OB_HW_DESTRUCTIVE_TEST_FILTER     "A:B:C"     run ONLY these (destructive binary)
#   OB_HW_NON_DESTRUCTIVE_TEST_FILTER "*-A:B:C"   run all EXCEPT these (smoke binary)
#
# The gtest filter syntax splits on the FIRST '-' only: the left side is the
# positive pattern ('*' = everything) and the right side is the negative
# pattern (colon-separated exclusions). A single '*-' prefix therefore
# excludes every name that follows — no need to repeat it per name.
#
# Keep this list in sync with kDestructiveTestFilter in
# tests/hardware/hw_test_main_common.hpp.
set(OB_HW_DESTRUCTIVE_TESTS
    TC_CPP_02_Discovery_HW.TC_CPP_02_04_force_ip
    TC_CPP_02_Discovery_HW.TC_CPP_02_05_hotplug_reboot
    TC_CPP_06_Sensor.TC_CPP_06_09_sensor_after_reboot
    TC_CPP_21_Firmware.TC_CPP_21_07_firmware_update
    TC_CPP_21_Firmware.TC_CPP_21_08_update_depth_presets
)

# Join the list with ':' to form gtest filter strings.
string(REPLACE ";" ":" OB_HW_DESTRUCTIVE_TEST_FILTER "${OB_HW_DESTRUCTIVE_TESTS}")
string(REPLACE ";" ":" _OB_HW_DEST_EXCLUDE "${OB_HW_DESTRUCTIVE_TESTS}")
set(OB_HW_NON_DESTRUCTIVE_TEST_FILTER "*-${_OB_HW_DEST_EXCLUDE}")
unset(_OB_HW_DEST_EXCLUDE)


set(HW_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_02_discovery_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_03_device_list_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_04_device_info_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_05_access_mode_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_06_sensor_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_07_stream_profile_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_08_pipeline_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_09_config_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_10_frame_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_11_metadata_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_12_frame_factory_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_13_filter_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_14_property_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_15_depth_mode_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_16_preset_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_17_interleave_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_18_record_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_19_multi_sync_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_20_coordinate_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_21_firmware_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_24_error_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TC_CPP_25_data_struct_hw_test.cpp
)

add_executable(ob_hw_test
    ${HW_TEST_SOURCES}
    hw_test_main.cpp
)

target_link_libraries(ob_hw_test PRIVATE ob_test_common)

# Ensure runtime extension DLLs are copied to the output dir even under a
# narrow `cmake --build --target ob_hw_test` (see ob_unit_test).
add_dependencies(ob_hw_test extensions)

add_custom_command(TARGET ob_hw_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../test_config.json"
        "${OB_RESOURCES_OUTPUT_DIRECTORY}/test_config.json"
)

include(GoogleTest)
gtest_discover_tests(ob_hw_test
    TEST_FILTER ${OB_HW_NON_DESTRUCTIVE_TEST_FILTER}
    PROPERTIES LABELS "hw" WORKING_DIRECTORY "${OB_RESOURCES_OUTPUT_DIRECTORY}"
)

set_target_properties(ob_hw_test PROPERTIES FOLDER "tests")
if(MSVC)
    set_target_properties(ob_hw_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ob_hw_test RUNTIME DESTINATION bin)

add_subdirectory(destructive)
