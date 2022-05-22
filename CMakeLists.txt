cmake_minimum_required(VERSION 3.10)
project(ubus)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(3rdparty)

add_subdirectory(nimportequoi)

# add_subdirectory(transport)

file(GLOB_RECURSE src_list ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

add_library(ubus SHARED ${src_list})

target_include_directories(ubus
    PUBLIC
        include
)

target_link_libraries(ubus
    PUBLIC
        common
        modern-json
)

add_executable(ubus-master main.cpp)

target_link_libraries(ubus-master
    PUBLIC
        ubus
)

add_executable(test-ubus-runtime test/test_ubus_runtime.cpp)

target_link_libraries(test-ubus-runtime
    PUBLIC
        ubus
)



# add_subdirectory(test)