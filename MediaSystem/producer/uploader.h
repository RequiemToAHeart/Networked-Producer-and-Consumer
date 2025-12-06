#pragma once

#include <grpcpp/grpcpp.h>
#include "media.grpc.pb.h"
#include <memory>
#include <string>

class Uploader {
public:
    explicit Uploader(const std::string& server_address);

    bool upload_file(const std::string& filepath, const std::string& producer_id);

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<media::MediaUpload::Stub> stub_;

};
