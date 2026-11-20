# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

set(app_requires)
if(MATTER_ENABLED)
    list(APPEND app_requires matter_dependencies)
endif()

idf_component_register(SRCS ${app_sources} REQUIRES ${app_requires})
