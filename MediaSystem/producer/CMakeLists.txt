add_executable(producer
    producer_main.cpp
    uploader.cpp
)

target_link_libraries(producer PRIVATE
    proto_generated
)
