# ===========================================================================
# GUI executable
# ===========================================================================
if(ZIOOP_BUILD_GUI)
    add_executable(
            zioop_gui
            gui_main.cpp
    )

    target_link_libraries(
            zioop_gui
            PRIVATE
            zioop_gui_lib
            zioop_network
            zioop_core
            zioop_flags
    )

    if(WIN32)
        set_target_properties(
                zioop_gui
                PROPERTIES
                WIN32_EXECUTABLE $<CONFIG:Release>
        )
    endif()

    set_target_properties(
            zioop_gui
            PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# ===========================================================================
# SERVER executable
# ===========================================================================

add_executable(
        zioop_server
        server_main.cpp
)

target_link_libraries(
        zioop_server
        PRIVATE
        nlohmann_json::nlohmann_json
        zioop_core
        zioop_network
        zioop_flags
)

# ===========================================================================
# AI executable
# ===========================================================================

add_executable(
        zioop_ai
        ai_main.cpp
)

target_link_libraries(
        zioop_ai
        PRIVATE
        zioop_core
        zioop_network
        zioop_flags
)

# ===========================================================================
# BOT executable
# ===========================================================================

add_executable(
        zioop_bot
        bot_main.cpp
)

target_link_libraries(
        zioop_bot
        PRIVATE
        zioop_network
        zioop_flags
)