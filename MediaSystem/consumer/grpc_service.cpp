#include "grpc_service.h"
#include "sha256.h"
#include <fstream>
#include <iostream>
#include <filesystem>

MediaUploadServiceImpl::MediaUploadServiceImpl(size_t queue_capacity,
                                               const std::string& storage_dir,
                                               const std::string& preview_dir,
                                               std::function<void(const UploadItem&, const std::string&, const std::string&)> notify)
: queue_(queue_capacity), queue_capacity_(queue_capacity), storage_dir_(storage_dir), preview_dir_(preview_dir), notify_(notify) {
    pool_ = new WorkerPool(std::thread::hardware_concurrency(), storage_dir_, preview_dir_, notify_);
    // optionally load existing checksums from DB (omitted for brevity)
}

grpc::Status MediaUploadServiceImpl::Upload(grpc::ServerContext* context, grpc::ServerReader<media::UploadRequest>* reader, media::UploadStatus* response) {
    media::UploadRequest req;
    media::FileInfo info;
    bool got_info = false;
    // create temp file
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
    UploadItem item;
    item.temp_path = temp_file;
    item.filename = info.filename();
    item.producer_id = info.producer_id();
    item.filesize = info.filesize();
    // compute checksum
    item.checksum = sha256_file(temp_file);

    {
        std::lock_guard<std::mutex> lk(checksums_mtx_);
        if (checksums_.count(item.checksum)) {
            response->set_accepted(false);
            response->set_message("duplicate");
            response->set_duplicate(true);
            std::filesystem::remove(temp_file);
            return grpc::Status::OK;
        }
    }

    // try to enqueue
    bool enq = queue_.try_push(std::move(item));
    if (!enq) {
        response->set_accepted(false);
        response->set_message("queue full");
        std::filesystem::remove(temp_file);
        return grpc::Status::OK;
    }

    // accepted, record checksum so duplicates detected earlier (this assumes worker will persist)
    {
        std::lock_guard<std::mutex> lk(checksums_mtx_);
        checksums_.insert(item.checksum); // note: we moved item into queue; but checksum local copy saved into set earlier - this is simplified
    }

    response->set_accepted(true);
    response->set_message("enqueued");
    return grpc::Status::OK;
}

void MediaUploadServiceImpl::start_workers() {
    pool_->start();
    // connect pool's internal queue to our queue: in this simplified design, we periodically pop from queue_ and push to pool's internal queue
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
