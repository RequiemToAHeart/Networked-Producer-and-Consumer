add_executable(consumer
    consumer_main.cpp
    grpc_service.cpp
    worker.cpp
    http_gui_server.cpp
)

target_link_libraries(consumer PRIVATE
    proto_generated
    nlohmann_json::nlohmann_json
    httplib::httplib
)
