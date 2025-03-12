cmake_minimum_required(VERSION 3.5)

add_library(npcaplib INTERFACE)

target_include_directories(npcaplib INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src/include)

add_library(npcaplib::npcaplib ALIAS npcaplib)