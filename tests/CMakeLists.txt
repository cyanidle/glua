add_executable(glua-test main.cpp)
target_link_libraries(glua-test PRIVATE glua)

find_package(Lua)
target_link_libraries(glua-test PRIVATE ${LUA_LIBRARIES})
target_include_directories(glua-test PRIVATE ${LUA_INCLUDE_DIR})
