cmake_minimum_required(VERSION 3.5)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltDeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltPropertyAccessor.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltPropertyAccessor.cpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltAlgParamManager.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoBoltAlgParamManager.cpp
        )

add_subdirectory(rawphase)