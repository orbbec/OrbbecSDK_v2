cmake_minimum_required(VERSION 3.13)

# ============================================================
# ob_destructive_test — 危险操作测试 (手动触发, 独立二进制)
# OB_HW_DESTRUCTIVE_TEST_FILTER is defined by the parent CMakeLists.txt
# (tests/hardware) and is visible here without redefinition.

add_executable(ob_destructive_test
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_02_discovery_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_03_device_list_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_04_device_info_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_05_access_mode_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_06_sensor_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_07_stream_profile_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_08_pipeline_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_09_config_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_10_frame_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_11_metadata_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_12_frame_factory_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_13_filter_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_14_property_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_15_depth_mode_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_16_preset_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_17_interleave_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_18_record_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_19_multi_sync_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_20_coordinate_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_21_firmware_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_24_error_hw_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TC_CPP_25_data_struct_hw_test.cpp
    destructive_test_main.cpp
)

target_link_libraries(ob_destructive_test PRIVATE ob_test_common)

# Ensure runtime extension DLLs are copied to the output dir even under a
# narrow `cmake --build --target ob_destructive_test` (see ob_unit_test).
add_dependencies(ob_destructive_test extensions)

add_custom_command(TARGET ob_destructive_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../../test_config.json"
        "${OB_RESOURCES_OUTPUT_DIRECTORY}/test_config.json"
)

include(GoogleTest)
# Label ob_destructive_test with a single "destructive" label only.
#
# gtest_discover_tests drops the second value when PROPERTIES LABELS is a
# semicolon-separated list like "hw;destructive": the generated
# set_tests_properties() call only retains the first value, so every test
# ended up labelled "hw" alone. That caused two problems:
#   1. `ctest -L destructive` matched zero tests -> the dedicated destructive
#      job printed "No tests were found" yet exited 0 (silent green pass).
#   2. hw-smoke's `-LE destructive` could not exclude these dangerous tests
#      (they lacked the "destructive" label), so firmware-flash / force-IP /
#      reboot cases leaked into the smoke run and destabilised the device.
# A single label avoids the multi-value bug entirely.
gtest_discover_tests(ob_destructive_test
    TEST_FILTER ${OB_HW_DESTRUCTIVE_TEST_FILTER}
    PROPERTIES LABELS "destructive" WORKING_DIRECTORY "${OB_RESOURCES_OUTPUT_DIRECTORY}"
)
set_target_properties(ob_destructive_test PROPERTIES FOLDER "tests")
if(MSVC)
    set_target_properties(ob_destructive_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

install(TARGETS ob_destructive_test RUNTIME DESTINATION bin)
