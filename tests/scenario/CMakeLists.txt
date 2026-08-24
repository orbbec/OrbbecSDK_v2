cmake_minimum_required(VERSION 3.13)

add_executable(ob_scenario_test
    scenario_test_main.cpp
    lifecycle_test.cpp
)

target_link_libraries(ob_scenario_test PRIVATE ob_test_common)

# Ensure runtime extension DLLs are copied to the output dir even under a
# narrow `cmake --build --target ob_scenario_test` (see ob_unit_test).
add_dependencies(ob_scenario_test extensions)

add_custom_command(TARGET ob_scenario_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../test_config.json"
        "${OB_RESOURCES_OUTPUT_DIRECTORY}/test_config.json"
)

include(GoogleTest)
gtest_discover_tests(ob_scenario_test
    PROPERTIES LABELS "scenario" WORKING_DIRECTORY "${OB_RESOURCES_OUTPUT_DIRECTORY}"
)

set_target_properties(ob_scenario_test PROPERTIES FOLDER "tests")
if(MSVC)
    set_target_properties(ob_scenario_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()
