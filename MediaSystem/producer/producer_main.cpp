#include <grpcpp/grpcpp.h>
#include "media.grpc.pb.h"
#include "media.pb.h"

#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

using media::MediaUpload;
using media::UploadRequest;
using media::FileInfo;
using media::Chunk;
using media::UploadStatus;

void upload_file(const std::string& server_addr, const std::string& filepath, const std::string& producer_id) {
    auto channel = grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials());
    std::unique_ptr<MediaUpload::Stub> stub = MediaUpload::NewStub(channel);

    grpc::ClientContext ctx;
    UploadStatus status;
    std::unique_ptr<grpc::ClientWriter<UploadRequest>> writer(stub->Upload(&ctx, &status));

    // send FileInfo
    UploadRequest req;
    FileInfo* info = req.mutable_info();
    info->set_filename(std::filesystem::path(filepath).filename().string());
    info->set_producer_id(producer_id);
    info->set_filesize(std::filesystem::file_size(filepath));
    info->set_mime("video/mp4");
    writer->Write(req);

    std::ifstream ifs(filepath, std::ios::binary);
    const size_t CHUNK = 64*1024;
    std::vector<char> buf(CHUNK);
    while (ifs) {
        ifs.read(buf.data(), buf.size());
        std::streamsize n = ifs.gcount();
        if (n <= 0) break;
        UploadRequest chunkReq;
        Chunk* ch = chunkReq.mutable_chunk();
        ch->set_data(buf.data(), n);
        // optionally ch->set_offset(...)
        writer->Write(chunkReq);
    }

    writer->WritesDone();
    grpc::Status s = writer->Finish();
    if (!s.ok()) {
        std::cerr << "gRPC upload error: " << s.error_message() << std::endl;
    } else {
        if (status.accepted()) {
            std::cout << "Uploaded " << filepath << " -> accepted: " << status.message() << std::endl;
        } else {
            std::cout << "Uploaded " << filepath << " -> rejected: " << status.message() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: producer <server:port> <producer_id> <input_folder>" << std::endl;
        return 1;
    }
    std::string server = argv[1];
    std::string pid = argv[2];
    std::string folder = argv[3];

    for (auto& p : std::filesystem::directory_iterator(folder)) {
        if (!p.is_regular_file()) continue;
        std::string path = p.path().string();
        std::cout << "Uploading " << path << std::endl;
        upload_file(server, path, pid);
    }

    return 0;
}
