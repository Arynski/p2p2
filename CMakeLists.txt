cmake_minimum_required(VERSION 3.10)
project(P2P2 C)

find_package(Curses REQUIRED)

set(COMMON_SOURCES
    common/network.c
)

# klient p2p2
add_executable(p2p2
    klient/tui/tui.c 
    klient/handler.c 
    klient/host.c 
    klient/main.c 
    klient/peer.c
    ${COMMON_SOURCES}
)
target_include_directories(p2p2 PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(p2p2 PRIVATE ${CURSES_LIBRARIES})

# serwer p2p2
add_executable(p2p2_serwer 
    serwer/main.c 
    serwer/room.c
    serwer/handler.c
    ${COMMON_SOURCES}
)
target_include_directories(p2p2_serwer PRIVATE ${CMAKE_SOURCE_DIR})