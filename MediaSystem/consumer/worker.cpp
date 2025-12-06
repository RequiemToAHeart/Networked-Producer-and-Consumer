#include "worker.h"
#include "bounded_queue.h"
#include "sha256.h"
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sqlite3.h>
#include <fstream>

struct WorkerPool::Impl {
    BoundedQueue<UploadItem> queue;
    std::vector<std::thread> threads;
    std::atomic<bool> running;
    std::string storage_dir;
    std::string preview_dir;
    NotifyFn notify;
    sqlite3* db = nullptr;

    Impl(size_t cap, const std::string& sdir, const std::string& pdir, NotifyFn n)
    : queue(cap), running(false), storage_dir(sdir), preview_dir(pdir), notify(n), db(nullptr) {
        std::filesystem::create_directories(storage_dir);
        std::filesystem::create_directories(preview_dir);
        std::string dbfile = storage_dir + "/metadata.db";
        if (sqlite3_open(dbfile.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Failed to open sqlite db: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            db = nullptr;
        } else {
            const char* create_sql = "CREATE TABLE IF NOT EXISTS uploads (id INTEGER PRIMARY KEY, filename TEXT, checksum TEXT UNIQUE, path TEXT, preview TEXT, uploaded_at DATETIME DEFAULT CURRENT_TIMESTAMP);";
            char* err = nullptr;
            sqlite3_exec(db, create_sql, nullptr, nullptr, &err);
            if (err) { std::cerr << "sqlite create table err: " << err << std::endl; sqlite3_free(err); }
        }
    }

    ~Impl() {
        if (db) sqlite3_close(db);
    }
};

static void process_item(WorkerPool::Impl* impl, UploadItem item) {
    try {
        std::string dest = impl->storage_dir + "/" + item.filename;
        if (std::filesystem::exists(dest)) {
            std::string alt = item.filename + "." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            dest = impl->storage_dir + "/" + alt;
        }
        std::filesystem::rename(item.temp_path, dest);

        std::string preview = impl->preview_dir + "/" + std::filesystem::path(dest).filename().string() + ".preview.mp4";
        std::string cmd = "ffmpeg -y -hide_banner -loglevel error -i \""+dest+"\" -ss 0 -t 10 -c:v libx264 -preset veryfast -crf 28 -c:a aac -b:a 64k \""+preview+"\"";
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "ffmpeg preview generation failed rc=" << rc << std::endl;
        }

        if (impl->db) {
            const char* insert_sql = "INSERT OR IGNORE INTO uploads(filename, checksum, path, preview) VALUES(?,?,?,?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(impl->db, insert_sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, item.filename.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, item.checksum.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, dest.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 4, preview.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        std::string preview_url = std::string("/previews/") + std::filesystem::path(preview).filename().string();
        std::string final_url = std::string("/uploads/") + std::filesystem::path(dest).filename().string();

        impl->notify(item, preview_url, final_url);
    } catch (const std::exception& ex) {
        std::cerr << "Exception in process_item: " << ex.what() << std::endl;
    }
}

WorkerPool::WorkerPool(size_t workers, const std::string& storage_dir, const std::string& preview_dir, NotifyFn notify) {
    impl = new Impl(workers * 4, storage_dir, preview_dir, notify);
    (void)workers;
}

WorkerPool::~WorkerPool() {
    stop();
    delete impl;
}

void WorkerPool::start() {
    if (impl->running.exchange(true)) return;
    size_t num = std::thread::hardware_concurrency();
    if (num == 0) num = 2;
    for (size_t i=0;i<num;i++) {
        impl->threads.emplace_back([this]{
            while (impl->running) {
                UploadItem item = impl->queue.pop_blocking();
                process_item(impl, std::move(item));
            }
        });
    }
}

void WorkerPool::stop() {
    impl->running = false;
    for (auto &t : impl->threads) {
        if (t.joinable()) t.detach();
    }
    impl->threads.clear();
}

void WorkerPool::enqueue(UploadItem&& item) {
    impl->queue.try_push(std::move(item));
}