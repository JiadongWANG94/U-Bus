cmake_minimum_required(VERSION 3.10)
project(ubus)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

add_subdirectory(3rdparty)

add_subdirectory(nimportequoi)

# add_subdirectory(transport)

file(GLOB_RECURSE src_list ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

add_library(ubus STATIC ${src_list})

target_include_directories(ubus
    PUBLIC
        include
)

target_link_libraries(ubus
    PUBLIC
        common
        modern-json
        Threads::Threads
)

add_executable(ubus-master main.cpp)

target_link_libraries(ubus-master
    PUBLIC
        ubus
)

add_executable(test-ubus-publisher test/test_ubus_publisher.cpp)

target_link_libraries(test-ubus-publisher
    PUBLIC
        ubus
)

add_executable(test-ubus-subscriber test/test_ubus_subscriber.cpp)

target_link_libraries(test-ubus-subscriber
    PUBLIC
        ubus
)

target_include_directories(test-ubus-subscriber
    PUBLIC
        test
)

add_executable(test-ubus-subscriber-2 test/test_ubus_subscriber_2.cpp)

target_link_libraries(test-ubus-subscriber-2
    PUBLIC
        ubus
)

target_include_directories(test-ubus-subscriber-2
    PUBLIC
        test
)

add_executable(test-ubus-method-provider test/test_ubus_method_provider.cpp)

target_link_libraries(test-ubus-method-provider
    PUBLIC
        ubus
)
target_include_directories(test-ubus-method-provider
    PUBLIC
        test
)

add_executable(test-ubus-method-requester test/test_ubus_method_requester.cpp)

target_link_libraries(test-ubus-method-requester
    PUBLIC
        ubus
)
target_include_directories(test-ubus-method-requester
    PUBLIC
        test
)

add_executable(test-clock-client test/test_clock_client.cpp)

target_link_libraries(test-clock-client
    PUBLIC
        ubus
)
target_include_directories(test-clock-client
    PUBLIC
        test
)

add_executable(test-clock-server test/test_clock_server.cpp)

target_link_libraries(test-clock-server
    PUBLIC
        ubus
)
target_include_directories(test-clock-server
    PUBLIC
        test
)

# add_executable(test-memory-order test/test_memory_order.cpp)
# add_subdirectory(test)