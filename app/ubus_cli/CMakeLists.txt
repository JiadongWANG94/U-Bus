

file(GLOB src_list ${CMAKE_CURRENT_LIST_DIR}/*.cpp )

add_executable(ubus_cli ${src_list})

target_include_directories(ubus_cli PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ubus_cli
    PUBLIC
        CLI11
        ubus
)