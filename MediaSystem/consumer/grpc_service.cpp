#include "grpc_service.h"
#include "sha256.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "bounded_queue.h"
#include "worker.h"
#include "media.grpc.pb.h"

MediaUploadServiceImpl::MediaUploadServiceImpl(size_t queue_capacity,
                                               const std::string& storage_dir,
                                               const std::string& preview_dir,
                                               std::function<void(const UploadItem&, const std::string&, const std::string&)> notify)
: queue_(queue_capacity), queue_capacity_(queue_capacity), storage_dir_(storage_dir), preview_dir_(preview_dir), notify_(notify) {
    pool_ = new WorkerPool(std::thread::hardware_concurrency(), storage_dir_, preview_dir_, notify_);
    checksums_file_ = storage_dir_ + "/.checksums.txt";
    load_checksums();
}

void MediaUploadServiceImpl::load_checksums() {
    std::ifstream ifs(checksums_file_);
    if (!ifs.is_open()) {
        std::cout << "No existing checksums file found, starting fresh" << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) {
            checksums_.insert(line);
        }
    }
    std::cout << "Loaded " << checksums_.size() << " checksums from file" << std::endl;
}

void MediaUploadServiceImpl::save_checksum(const std::string& checksum) {
    std::ofstream ofs(checksums_file_, std::ios::app);
    if (ofs.is_open()) {
        ofs << checksum << "\n";
        ofs.flush();
    }
}

grpc::Status MediaUploadServiceImpl::Upload(grpc::ServerContext* context, grpc::ServerReader<media::UploadRequest>* reader, media::UploadStatus* response) {
    media::UploadRequest req;
    media::FileInfo info;
    bool got_info = false;
    std::string tmpdir = std::filesystem::temp_directory_path().string();
    std::string temp_file = tmpdir + "/upload_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "_" + std::to_string(std::time(nullptr));
    
    std::ofstream ofs(temp_file, std::ios::binary);
    if (!ofs) {
        response->set_accepted(false);
        response->set_message("server error: cannot open temp file");
        return grpc::Status::OK;
    }
    
    while (reader->Read(&req)) {
        if (req.has_info()) {
            info = req.info();
            got_info = true;
        } else if (req.has_chunk()) {
            const std::string& d = req.chunk().data();
            ofs.write(d.data(), d.size());
        }
    }
    ofs.close();
    
    if (!got_info) {
        response->set_accepted(false);
        response->set_message("missing file info");
        std::filesystem::remove(temp_file);
        return grpc::Status::OK;
    }
    
    std::string checksum = sha256_file(temp_file);
    
    if (checksum.empty()) {
        std::cout << "ERROR: Failed to compute checksum for " << temp_file << std::endl;
        response->set_accepted(false);
        response->set_message("server error: checksum failed");
        std::filesystem::remove(temp_file);
        return grpc::Status::OK;
    }
    
    {
        std::lock_guard<std::mutex> lk(checksums_mtx_);
        if (checksums_.count(checksum)) {
            response->set_accepted(false);
            response->set_message("duplicate");
            response->set_duplicate(true);
            duplicate_count_++;
            std::cout << "Duplicate detected: " << info.filename() << " (hash: " << checksum.substr(0, 16) << "...)" << std::endl;
            std::filesystem::remove(temp_file);
            return grpc::Status::OK;
        }
    }
    
    UploadItem item;
    item.temp_path = temp_file;
    item.filename = info.filename();
    item.producer_id = info.producer_id();
    item.filesize = info.filesize();
    item.checksum = checksum;

    bool enq = queue_.try_push(std::move(item));
    if (!enq) {
        response->set_accepted(false);
        response->set_message("queue full");
        std::filesystem::remove(temp_file);
        return grpc::Status::OK;
    }

    {
        std::lock_guard<std::mutex> lk(checksums_mtx_);
        checksums_.insert(checksum);
        save_checksum(checksum);
    }

    response->set_accepted(true);
    response->set_message("enqueued");
    std::cout << "Uploaded: " << info.filename() << " (hash: " << checksum.substr(0, 16) << "...)" << std::endl;
    return grpc::Status::OK;
}

void MediaUploadServiceImpl::start_workers() {
    pool_->start();
    std::thread([this]{
        while (true) {
            UploadItem it = queue_.pop_blocking();
            pool_->enqueue(std::move(it));
        }
    }).detach();
}

void MediaUploadServiceImpl::stop_workers() {
    pool_->stop();
}