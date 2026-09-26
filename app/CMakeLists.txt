# ===========================================================================
# GUI executable
# ===========================================================================
if (ZIOOP_BUILD_GUI)
    add_executable(
            zioop_gui
            gui_main.cpp
    )

    target_link_libraries(
            zioop_gui
            PRIVATE
            zioop_gui_lib
            zioop_flags
    )

    add_custom_command(TARGET zioop_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:zioop_gui>/assets
    )

    if (WIN32)
        set_target_properties(
                zioop_gui
                PROPERTIES
                WIN32_EXECUTABLE $<CONFIG:Release>
        )
    endif ()

    set_target_properties(
            zioop_gui
            PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif ()

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
        zioop_ai_lib
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
        zioop_flags
)
