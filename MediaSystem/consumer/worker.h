#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>

struct UploadItem {
    std::string temp_path;
    std::string filename;
    std::string producer_id;
    std::string checksum;
    int64_t filesize;
};

using NotifyFn = std::function<void(const UploadItem&, const std::string& preview_url, const std::string& final_url)>;

class WorkerPool {
public:
    WorkerPool(size_t workers, const std::string& storage_dir, const std::string& preview_dir, NotifyFn notify);
    ~WorkerPool();

    void start();
    void stop();
    void enqueue(UploadItem&& item);

    // expose the queue so the gRPC service can try_push into it
    class Impl;
    Impl* impl;
};
