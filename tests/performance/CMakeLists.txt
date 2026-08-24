cmake_minimum_required(VERSION 3.13)

add_executable(ob_perf_test
    frame_drop_test.cpp
    thread_safety_test.cpp
    perf_test_main.cpp
)

target_link_libraries(ob_perf_test PRIVATE ob_test_common)

# Ensure runtime extension DLLs are copied to the output dir even under a
# narrow `cmake --build --target ob_perf_test` (see ob_unit_test).
add_dependencies(ob_perf_test extensions)

add_custom_command(TARGET ob_perf_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../test_config.json"
        "${OB_RESOURCES_OUTPUT_DIRECTORY}/test_config.json"
)

if(WIN32)
    target_link_libraries(ob_perf_test PRIVATE psapi)
endif()

include(GoogleTest)
gtest_discover_tests(ob_perf_test
    PROPERTIES LABELS "perf" WORKING_DIRECTORY "${OB_RESOURCES_OUTPUT_DIRECTORY}"
)

set_target_properties(ob_perf_test PROPERTIES FOLDER "tests")
if(MSVC)
    set_target_properties(ob_perf_test PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()
