set(ZIOOP_NETWORK_SOURCES
        Server.cpp
        Client.cpp
)

file(GLOB_RECURSE ZIOOP_NETWORK_HEADERS CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/includes/zioop/network/*.hpp
)

add_library(
        zioop_network
        STATIC
        ${ZIOOP_NETWORK_SOURCES}
        ${ZIOOP_NETWORK_HEADERS}
)

target_include_directories(
        zioop_network
        PUBLIC
        ${PROJECT_SOURCE_DIR}/includes
)

target_link_libraries(
        zioop_network
        PRIVATE
        zioop_flags
)

# Windows sockets
if(WIN32)
    target_link_libraries(zioop_network PRIVATE ws2_32)
endif()