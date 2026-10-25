if(ZIOOP_BUILD_GUI)

    set(ZIOOP_GUI_SOURCES
            MatrixConnection.cpp
            Agent.cpp
            Renderer.cpp
            Terrain.cpp
    )

    file(GLOB_RECURSE ZIOOP_GUI_HEADERS CONFIGURE_DEPENDS
            ${PROJECT_SOURCE_DIR}/includes/zioop/gui/*.hpp
    )

    add_library(
            zioop_gui_lib
            STATIC
            ${ZIOOP_GUI_SOURCES}
            ${ZIOOP_GUI_HEADERS}
    )

    target_include_directories(
            zioop_gui_lib
            PUBLIC
            ${PROJECT_SOURCE_DIR}/includes
    )

    target_link_libraries(
            zioop_gui_lib
            PUBLIC
            raylib
            nlohmann_json::nlohmann_json
            PRIVATE
            zioop_protocol
            zioop_flags
    )

    if(WIN32)
        target_link_libraries(zioop_gui_lib PRIVATE ws2_32)
    endif()
endif()
