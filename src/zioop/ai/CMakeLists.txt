if(ZIOOP_BUILD_AI)

    set(ZIOOP_AI_SOURCES
            Bot.cpp
            Claude.cpp
            Test.cpp
    )

    file(GLOB_RECURSE ZIOOP_AI_HEADERS CONFIGURE_DEPENDS
            ${PROJECT_SOURCE_DIR}/includes/zioop/ai/*.hpp
    )

    add_library(
            zioop_ai_lib
            STATIC
            ${ZIOOP_AI_SOURCES}
            ${ZIOOP_AI_HEADERS}
    )

    target_include_directories(
            zioop_ai_lib
            PUBLIC
            ${PROJECT_SOURCE_DIR}/includes
    )

    target_link_libraries(
            zioop_ai_lib
            PUBLIC
            zioop_protocol
            nlohmann_json::nlohmann_json
            PRIVATE
            zioop_flags
    )

    if(WIN32)
        target_link_libraries(zioop_ai_lib PRIVATE ws2_32)
    endif()
endif()
