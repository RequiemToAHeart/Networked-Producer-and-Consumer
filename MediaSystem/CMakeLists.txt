cmake_minimum_required(VERSION 3.15)
project(MediaSystem CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# -------------------------
#   PROTO & GRPC GENERATION
# -------------------------
set(PROTO_FILE ${CMAKE_SOURCE_DIR}/proto/media.proto)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate protobuf sources (.pb.cc / .pb.h)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# Generate gRPC sources (.grpc.pb.cc / .grpc.pb.h)
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

# -------------------------
#   Build a shared library
# -------------------------
add_library(proto_generated STATIC
    ${PROTO_SRCS} ${PROTO_HDRS}
    ${GRPC_SRC} ${GRPC_HDR}
)

target_include_directories(proto_generated PUBLIC
    ${GENERATED_DIR}
    ${CMAKE_BINARY_DIR}    
)


target_link_libraries(proto_generated PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

# -------------------------
#   Subdirectories
# -------------------------
add_subdirectory(producer)
add_subdirectory(consumer)
