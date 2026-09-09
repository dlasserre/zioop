include(FetchContent)

set(FETCHCONTENT_BASE_DIR
        ${CMAKE_SOURCE_DIR}/.deps
        CACHE PATH "" FORCE
)

if(ZIOOP_BUILD_GUI)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        5.5
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE
    )

    FetchContent_MakeAvailable(raylib)

    if(TARGET raylib)
        set_property(TARGET raylib PROPERTY FOLDER "deps")

        if(MSVC)
            target_compile_options(raylib PRIVATE /W0)
        endif()
    endif()

endif()