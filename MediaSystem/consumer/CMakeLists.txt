add_executable(consumer
    consumer_main.cpp
    grpc_service.cpp
    worker.cpp
    http_gui_server.cpp
)

find_package(unofficial-sqlite3 CONFIG REQUIRED)

target_link_libraries(consumer
    PRIVATE
        proto_generated
        gRPC::grpc++
        httplib::httplib
        unofficial::sqlite3::sqlite3
)

target_include_directories(consumer PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)
