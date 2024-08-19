cmake_minimum_required(VERSION 3.5)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE  ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaDevice.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaDevice.cpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaDeviceInfo.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaDeviceInfo.cpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaTempPropertyAccessor.hpp
             ${CMAKE_CURRENT_LIST_DIR}/FemtoMegaTempPropertyAccessor.cpp
        )
