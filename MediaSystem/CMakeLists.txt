cmake_minimum_required(VERSION 3.15)
project(MediaSystem CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# ------------------------------------------
#      Generate protobuf and gRPC sources
# ------------------------------------------
set(PROTO_FILE ${CMAKE_SOURCE_DIR}/proto/media.proto)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# gRPC generation
set(GRPC_SRC ${GENERATED_DIR}/media.grpc.pb.cc)
set(GRPC_HDR ${GENERATED_DIR}/media.grpc.pb.h)

add_custom_command(
    OUTPUT ${GRPC_SRC} ${GRPC_HDR}
    COMMAND protobuf::protoc
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        --proto_path=${CMAKE_SOURCE_DIR}/proto
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
)

# Collect all generated files
set(GENERATED_PROTO_SRC
    ${PROTO_SRCS}
    ${GRPC_SRC}
)

set(GENERATED_PROTO_HDR
    ${PROTO_HDRS}
    ${GRPC_HDR}
)

# Export these variables for subdirectories
set(GENERATED_PROTO_SRC ${GENERATED_PROTO_SRC} PARENT_SCOPE)
set(GENERATED_PROTO_HDR ${GENERATED_PROTO_HDR} PARENT_SCOPE)

include_directories(${GENERATED_DIR})

add_subdirectory(producer)
add_subdirectory(consumer)
