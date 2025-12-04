add_executable(consumer
    consumer_main.cpp
    grpc_service.cpp
    worker.cpp
    http_gui_server.cpp
    ${GENERATED_PROTO_SRC}
    ${GENERATED_PROTO_HDR}
)

target_include_directories(consumer PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(consumer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    httplib::httplib
)
