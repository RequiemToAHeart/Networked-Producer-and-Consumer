#pragma once
#include "media.grpc.pb.h"
#include "media.pb.h"
#include "worker.h"
#include "bounded_queue.h"
#include <grpcpp/grpcpp.h>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <atomic>

class MediaUploadServiceImpl final : public media::MediaUpload::Service {
public:
    MediaUploadServiceImpl(size_t queue_capacity,
                          const std::string& storage_dir,
                          const std::string& preview_dir,
                          std::function<void(const UploadItem&, const std::string&, const std::string&)> notify);

    grpc::Status Upload(grpc::ServerContext* context, grpc::ServerReader<media::UploadRequest>* reader, media::UploadStatus* response) override;

    void start_workers();
    void stop_workers();
    
    size_t get_duplicate_count() const { return duplicate_count_.load(); }

private:
    void load_checksums();
    void save_checksum(const std::string& checksum);
    
    BoundedQueue<UploadItem> queue_;
    std::unordered_set<std::string> checksums_;
    std::mutex checksums_mtx_;
    WorkerPool* pool_;
    size_t queue_capacity_;
    std::string storage_dir_;
    std::string preview_dir_;
    std::string checksums_file_;
    std::function<void(const UploadItem&, const std::string&, const std::string&)> notify_;
    std::atomic<size_t> duplicate_count_{0};
};