cmake_minimum_required(VERSION 3.20)

project(zioop
        VERSION 0.1.0
        DESCRIPTION "Une arene C++ ou des modeles de langage recoltent, s'appellent et se battent"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZIOOP_BUILD_GUI "Construire le client GUI" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

include(cmake/deps.cmake)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

add_library(zioop_flags INTERFACE)

if(MSVC)
    target_compile_options(zioop_flags INTERFACE
            /W4
            /permissive-
            /utf-8
            /Zc:preprocessor
            /MP
            /diagnostics:column
    )

    target_compile_definitions(
            zioop_flags
            INTERFACE
            _CRT_SECURE_NO_WARNINGS
    )
else()
    target_compile_options(
            zioop_flags
            INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ===========================================================================
# CORE
# ===========================================================================

add_subdirectory(src/zioop/core)

# ===========================================================================
# NETWORK
# ===========================================================================

add_subdirectory(src/zioop/network)

# ===========================================================================
# GUI
# ===========================================================================

add_subdirectory(src/zioop/gui)

# ===========================================================================
# Binaries
# ===========================================================================

add_subdirectory(app)

# ===========================================================================
# MSVC
# ===========================================================================

if(MSVC)

    if(ZIOOP_BUILD_GUI)
        set_property(
                DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                PROPERTY VS_STARTUP_PROJECT zioop_gui
        )
    else()
        set_property(
                DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                PROPERTY VS_STARTUP_PROJECT zioop_server
        )
    endif()

endif()