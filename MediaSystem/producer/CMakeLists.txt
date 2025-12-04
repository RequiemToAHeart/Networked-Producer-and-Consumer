add_executable(producer
    producer_main.cpp
    uploader.cpp
    ${GENERATED_PROTO_SRC}
    ${GENERATED_PROTO_HDR}
)

target_include_directories(producer PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(producer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)
