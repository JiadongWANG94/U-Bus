file(GLOB src_list ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_executable(remote_executer ${src_list})

target_include_directories(remote_executer PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(remote_executer
    PUBLIC
        ubus
)