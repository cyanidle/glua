project(glua C CXX)

include(CPM.cmake)

option(GLUA_TESTS "Build glua tests" OFF)

CPMAddPackage("gh:cyanidle/describe@1.3")

add_library(glua INTERFACE)
target_compile_options(glua INTERFACE .)
target_link_libraries(glua INTERFACE describe)

add_library(glua::glua ALIAS glua)

if (GLUA_TESTS)
    add_subdirectory(tests)
endif()