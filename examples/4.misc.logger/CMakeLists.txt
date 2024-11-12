cmake_minimum_required(VERSION 3.5)
project(ob_logger)

add_executable(${PROJECT_NAME} logger.cpp)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
target_link_libraries(${PROJECT_NAME} ob::OrbbecSDK ob::examples::utils)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "examples")

install(TARGETS ob_logger RUNTIME DESTINATION bin)