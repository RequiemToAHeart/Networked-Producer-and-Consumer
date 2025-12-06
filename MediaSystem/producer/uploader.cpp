#include "uploader.h"
#include "media.grpc.pb.h"
#include "media.pb.h"

#include <openssl/sha.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::string sha256_of_file(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Unable to open file for hashing: " + path);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::vector<unsigned char> buffer(8192);
    while (file.good())
    {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        SHA256_Update(&ctx, buffer.data(), file.gcount());
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    std::string hex;
    hex.reserve(64);
    static const char* digits = "0123456789abcdef";

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        hex.push_back(digits[(hash[i] >> 4) & 0xF]);
        hex.push_back(digits[hash[i] & 0xF]);
    }

    return hex;
}

Uploader::Uploader(const std::string& server)
{
    channel_ = grpc::CreateChannel(server, grpc::InsecureChannelCredentials());
    stub_ = media::MediaUpload::NewStub(channel_);

}

bool Uploader::upload_file(const std::string& filepath, const std::string& producer_id)
{
    if (!fs::exists(filepath))
    {
        std::cerr << "[Producer] File does not exist: " << filepath << "\n";
        return false;
    }

    auto filesize = fs::file_size(filepath);
    auto hash = sha256_of_file(filepath);

    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        std::cerr << "[Producer] Error opening file: " << filepath << "\n";
        return false;
    }

    grpc::ClientContext ctx;
    media::UploadStatus response;
    auto writer = stub_->Upload(&ctx, &response);

    media::UploadRequest req;
    media::FileInfo* info = req.mutable_info();

    info->set_filename(fs::path(filepath).filename().string());
    info->set_producer_id(producer_id);
    info->set_filesize((int64_t)filesize);
    info->set_mime("application/octet-stream"); 

    writer->Write(req);
    int64_t offset = 0;

    std::vector<char> buffer(64 * 1024);

    while (!file.eof())
    {
        file.read(buffer.data(), buffer.size());
        std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;

       media::UploadRequest req;
        media::Chunk* c = req.mutable_chunk();
        c->set_data(buffer.data(), (size_t)bytes_read);
        c->set_offset(offset);
        writer->Write(req);
        offset += bytes_read;

    }

    writer->WritesDone();
    grpc::Status status = writer->Finish();

    if (!status.ok())
    {
        std::cerr << "[Producer] Upload failed: " << status.error_message() << "\n";
        return false;
    }

    std::cout << "[Producer] Upload result: " << response.message() << "\n";
    return true;
}
