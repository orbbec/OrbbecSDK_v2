# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Windows configurations")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OB_CURRENT_OS "win_x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(OB_CURRENT_OS "win_x86")
endif()

message(${CMAKE_SYSTEM_VERSION})

# Check for Windows Version ##
if(${CMAKE_SYSTEM_VERSION} EQUAL 6.1) # Windows 7
    message(FATAL_ERROR "CMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION} too lower!")
endif()

if(MSVC)
    # build with multiple cores
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /bigobj /wd4819")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMTD")
    set(LBS_TRY_USE_AVX true)

    SET(OBSENSOR_DEPENTENCIES_INCLUDE_DIR
    ${DepthEngine_INCLUDE_DIRS}
       )
    add_definitions(-D_UNICODE -DUNICODE -D_CRT_SECURE_NO_WARNINGS)
endif()
set(OB_BUILD_WIN32 ON)



