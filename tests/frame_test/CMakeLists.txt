cmake_minimum_required(VERSION 3.5)

add_executable(frame_test frame_test.cpp)
target_link_libraries(frame_test PRIVATE ob::OrbbecSDK)
set_target_properties(frame_test PROPERTIES FOLDER "tests")
