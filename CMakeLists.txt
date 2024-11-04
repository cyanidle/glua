cmake_minimum_required(VERSION 3.16)
project(glua C CXX)

include(CPM.cmake)

option(GLUA_TESTS "Build glua tests" OFF)

CPMAddPackage("gh:cyanidle/describe@2.0")

add_library(glua INTERFACE)
target_include_directories(glua INTERFACE include)
target_link_libraries(glua INTERFACE describe)

add_library(glua::glua ALIAS glua)

if (GLUA_TESTS)
    add_subdirectory(tests)
endif()
