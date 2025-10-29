# Copyright (c) Orbbec Inc. All Rights Reserved.
# Licensed under the MIT License.

message(STATUS "Setting Linux configurations")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -pedantic -D_DEFAULT_SOURCE -fvisibility=hidden")
set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -fPIC -pedantic -Wno-missing-field-initializers -fpermissive -fvisibility=hidden"
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-switch -Wno-multichar -Wsequence-point -Wformat -Wformat-security")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags -Wl,-Bsymbolic")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(OB_ENABLE_SANITIZER)
    set(SANITIZER_FLAGS "-fsanitize=address -static-libasan ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

if(CMAKE_CROSSCOMPILING)
    if( CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" )
        set(OB_BUILD_LINUX_ARM64 ON)
        message(STATUS "OB_BUILD_LINUX_ARM64 ON")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7l" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "armv6")
        set(OB_BUILD_LINUX_ARM32 ON)
        message(STATUS "OB_BUILD_LINUX_ARM32 ON. CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")
    else()
        message(SEND_ERROR "OrbbecSDK: unknown system processor for cross compiling!!!")
    endif()
else()
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
endif()

execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine OUTPUT_VARIABLE MACHINE)

set(OB_BUILD_LINUX_ARM OFF)

if(OB_BUILD_LINUX_ARM64)
    if(${MACHINE} MATCHES "aarch64.*-linux-gnu")
        add_definitions(-DOS_ARM)
        add_definitions(-DOS_ARM64)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mstrict-align -ftree-vectorize")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mstrict-align -ftree-vectorize")

        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--allow-shlib-undefined")
        
        # NEON is enabled by default;
        # add a custom user-defined macro instead of the compiler's __ARM_NEON__
        message("Linux arm64 set neon")
        add_definitions(-D__NEON__)

        set(OB_BUILD_LINUX_ARM ON)
        set(OB_CURRENT_OS "linux_arm64")
        message(STATUS "OB_CURRENT_OS: ${OB_CURRENT_OS}")
    else()
        message(SEND_ERROR "OrbbecSDK: check aarch64-linux-gnu not found!!!")
    endif()
endif()

if(OB_BUILD_LINUX_ARM32)
    if(${MACHINE} MATCHES "arm.*-linux-gnueabihf")
        # check if NEON is supported
        # 1. Backup the original flags
        set(_old_flags "${CMAKE_CXX_FLAGS}")
        # 2. Add a temporary flag for testing that won't affect the main build
        set(CMAKE_CXX_FLAGS "-mfpu=neon ${CMAKE_CXX_FLAGS}")
        check_cxx_source_compiles("
        #if defined(__GNUC__) && defined(__arm__) && __ARM_ARCH >= 7
        #include <arm_neon.h>
        int main() { uint8x16_t v = vdupq_n_u8(0); return 0; }
        #else
        #error \"Unsupported target. Must be ARMv7-A with NEON for 32-bit.\"
        #endif" HAS_NEON)
        # 3. Restore the original flags
        set(CMAKE_CXX_FLAGS "${_old_flags}")
        # 4. enable NEON
        if(HAS_NEON)
            message(STATUS "The current compiler supports NEON")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon")
            # add a custom user-defined macro instead of the compiler's __ARM_NEON__
            message("Linux arm32 set neon")
            add_definitions(-D__NEON__)
        else()
            message(STATUS "The current compiler doesn't support NEON")
        endif()

        add_definitions(-DOS_ARM)
        add_definitions(-DOS_ARM32)
        set(CMAKE_C_FLAGS
            "${CMAKE_C_FLAGS} -Wl,--allow-shlib-undefined -mfloat-abi=hard -ftree-vectorize -latomic -std=c99"
        )
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -Wl,--allow-shlib-undefined -mfloat-abi=hard -ftree-vectorize -latomic"
        )

        set(OB_BUILD_LINUX_ARM ON)
        set(OB_CURRENT_OS "linux_arm32")
        message(STATUS "OB_CURRENT_OS: ${OB_CURRENT_OS}")
    else()
        message(SEND_ERROR "OrbbecSDK: check arm-linux-gnueabihf not found!!!")
    endif()
endif()

if(NOT OB_BUILD_LINUX_ARM)
    # When the compiler (e.g., gcc or g++) is given the flag -mssse3,
    # it automatically defines the macro __SSSE3__=1.
    message("Linux set sse3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -mssse3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -mssse3")
endif()

add_definitions(-DOS_LINUX)
set(OB_BUILD_LINUX ON)

# target rpath settings
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:${CMAKE_INSTALL_RPATH}")
