# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Linux configurations")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -Wl,-Bsymbolic -fPIC -pedantic -D_DEFAULT_SOURCE -fvisibility=hidden")
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS}  -Wl,-Bsymbolic -fPIC -pedantic -Wno-missing-field-initializers -fpermissive -fvisibility=hidden"
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-switch -Wno-multichar -Wsequence-point -Wformat -Wformat-security")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(OB_ENABLE_SANITIZER)
    set(SANITIZER_FLAGS "-fsanitize=address -static-libasan ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

execute_process(COMMAND uname -r OUTPUT_VARIABLE UNAME_RESULT OUTPUT_STRIP_TRAILING_WHITESPACE)
message(-- " Kernel version: " ${UNAME_RESULT})
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${UNAME_RESULT})
message(STATUS "linux version ${LINUX_KERNEL_VERSION}")

set(OB_CURRENT_OS "linux_x86_64")

execute_process(COMMAND uname -m OUTPUT_VARIABLE MACHINES)
execute_process(COMMAND getconf LONG_BIT OUTPUT_VARIABLE MACHINES_BIT)
if((${MACHINES} MATCHES "x86_64") AND (${MACHINES_BIT} MATCHES "64"))
    set(OB_CURRENT_OS "linux_x86_64")
elseif(${MACHINES} MATCHES "arm")
    set(OB_BUILD_LINUX_ARM32 ON)
elseif((${MACHINES} MATCHES "aarch64") AND (${MACHINES_BIT} MATCHES "64"))
    set(OB_BUILD_LINUX_ARM64 ON)
elseif((${MACHINES} MATCHES "aarch64") AND (${MACHINES_BIT} MATCHES "32"))
    set(OB_BUILD_LINUX_ARM32 ON)
endif()

execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine OUTPUT_VARIABLE MACHINE)

if(OB_BUILD_LINUX_ARM64)
    if(${MACHINE} MATCHES "aarch64-linux-gnu")
        add_definitions(-DOS_ARM)
        add_definitions(-DOS_ARM64)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -Wl,--allow-shlib-undefined -mstrict-align -ftree-vectorize")

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined -mstrict-align -ftree-vectorize")

        set(OB_CURRENT_OS "linux_arm64")
    else()
        message(SEND_ERROR "Obsensor: check aarch64-linux-gnu not found!!!")
    endif(${MACHINE} MATCHES "aarch64-linux-gnu")
endif()

if(OB_BUILD_LINUX_ARM32)
    if(${MACHINE} MATCHES "arm-linux-gnueabihf")
        add_definitions(-DOS_ARM)
        add_definitions(-DOS_ARM32)
        set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS}  -Wl,--allow-shlib-undefined -mfpu=neon -mfloat-abi=hard -ftree-vectorize -latomic -std=c99"
        )
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined -mfpu=neon -mfloat-abi=hard -ftree-vectorize -latomic"
        )
        set(OB_CURRENT_OS "linux_arm32")
    else()
        message(SEND_ERROR "Obsensor: check arm-linux-gnueabihf not found!!!")
    endif()
endif()

if(OB_BUILD_LINUX_ARM64 OR OB_BUILD_LINUX_ARM32)
    message("linux set neon")
    add_definitions(-D__NEON__)
else()
    message("linux set sse3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse3")
endif()

add_definitions(-DOS_LINUX)
set(OB_BUILD_LINUX ON)

# target rpath settings
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN:../lib:${CMAKE_INSTALL_RPATH}")

