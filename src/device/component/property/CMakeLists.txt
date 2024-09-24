
cmake_minimum_required(VERSION 3.5)

target_sources(
    ${OB_TARGET_DEVICE}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/VendorPropertyAccessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/VendorPropertyAccessor.hpp
    ${CMAKE_CURRENT_LIST_DIR}/PropertyServer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PropertyServer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/CommonPropertyAccessors.cpp
    ${CMAKE_CURRENT_LIST_DIR}/CommonPropertyAccessors.hpp
    ${CMAKE_CURRENT_LIST_DIR}/FilterPropertyAccessors.cpp
    ${CMAKE_CURRENT_LIST_DIR}/FilterPropertyAccessors.hpp
    ${CMAKE_CURRENT_LIST_DIR}/InternalProperty.hpp
    ${CMAKE_CURRENT_LIST_DIR}/PrivateFilterPropertyAccessors.hpp
    ${CMAKE_CURRENT_LIST_DIR}/PrivateFilterPropertyAccessors.cpp
)

if(OB_BUILD_USB_PAL)
    target_sources(
        ${OB_TARGET_DEVICE} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/UvcPropertyAccessor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/UvcPropertyAccessor.hpp
    )
endif()