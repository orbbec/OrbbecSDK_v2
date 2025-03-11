cmake_minimum_required(VERSION 3.5)

# if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#     find_library(LIB_Packet Packet.lib PATHS ${CMAKE_CURRENT_LIST_DIR}/src/Lib/x64)
#     find_library(LIB_wpcap wpcap.lib PATHS ${CMAKE_CURRENT_LIST_DIR}/src/Lib/x64)
# elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
#     find_library(LIB_Packet Packet.lib PATHS ${CMAKE_CURRENT_LIST_DIR}/src/Lib/x32)
#     find_library(LIB_wpcap wpcap.lib PATHS ${CMAKE_CURRENT_LIST_DIR}/src/Lib/x32)
# endif()

add_library(npcaplib INTERFACE)

target_include_directories(npcaplib INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/include)

# target_link_libraries(npcaplib INTERFACE ${LIB_Packet} ${LIB_wpcap})

add_library(npcaplib::npcaplib ALIAS npcaplib)