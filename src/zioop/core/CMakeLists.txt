set(ZIOOP_CORE_SOURCES
        Cell.cpp
        Matrix.cpp
        Agent.cpp
        Resource.cpp
)

file(GLOB_RECURSE ZIOOP_CORE_HEADERS CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/includes/zioop/core/*.hpp
)

add_library(
        zioop_core
        STATIC
        ${ZIOOP_CORE_SOURCES}
        ${ZIOOP_CORE_HEADERS}
)

target_include_directories(
        zioop_core
        PUBLIC
        ${PROJECT_SOURCE_DIR}/includes
)

target_link_libraries(
        zioop_core
        PRIVATE
        zioop_flags
)