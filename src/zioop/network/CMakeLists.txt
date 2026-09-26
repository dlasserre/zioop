add_library(zioop_protocol STATIC Protocol.cpp ${PROJECT_SOURCE_DIR}/includes/zioop/network/Protocol.hpp)

target_include_directories(zioop_protocol PUBLIC ${PROJECT_SOURCE_DIR}/includes)
target_link_libraries(zioop_protocol PUBLIC nlohmann_json::nlohmann_json PRIVATE zioop_flags)

if(WIN32)
    target_link_libraries(zioop_protocol PRIVATE ws2_32)
endif()

set(ZIOOP_NETWORK_SOURCES
        Server.cpp
        Connection.cpp
        Client.cpp
        Serialization.cpp
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
        PUBLIC
        zioop_protocol
        nlohmann_json::nlohmann_json
)

# Windows sockets
if(WIN32)
    target_link_libraries(zioop_network PRIVATE ws2_32)
endif()
