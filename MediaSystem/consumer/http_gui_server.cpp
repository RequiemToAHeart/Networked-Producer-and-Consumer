#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <filesystem>

// We'll use a very small header-only HTTP lib — please download httplib.h to consumer/ and include it.
// Example: https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
#include "httplib.h" // <--- ensure this file is present in consumer/ directory

class SimpleHttpServer {
public:
    SimpleHttpServer(int port, const std::string& static_dir, const std::string& upload_dir, const std::string& preview_dir)
    : port_(port), static_dir_(static_dir), upload_dir_(upload_dir), preview_dir_(preview_dir), server_thread_running_(false) {}

    void start() {
        server_thread_running_ = true;
        server_thread_ = std::thread([this]{
            httplib::Server svr;
            // serve static
            svr.Get("/", [this](const httplib::Request& req, httplib::Response& res){
                std::ifstream ifs(static_dir_ + "/index.html");
                std::stringstream ss; ss << ifs.rdbuf();
                res.set_content(ss.str(), "text/html");
            });
            svr.Get("/main.js", [this](const httplib::Request& req, httplib::Response& res){
                std::ifstream ifs(static_dir_ + "/main.js");
                std::stringstream ss; ss << ifs.rdbuf();
                res.set_content(ss.str(), "application/javascript");
            });
            svr.Get("/uploads/(.*)", [this](const httplib::Request& req, httplib::Response& res){
                std::string file = req.matches[1];
                std::string path = upload_dir_ + "/" + file;
                if (std::filesystem::exists(path)) {
                    res.set_content_provider(
                        std::filesystem::file_size(path),
                        "video/mp4",
                        [path](size_t offset, size_t length, httplib::DataSink &sink) {
                            std::ifstream ifs(path, std::ios::binary);
                            ifs.seekg(offset);
                            std::vector<char> buf(length);
                            ifs.read(buf.data(), length);
                            sink.write(buf.data(), ifs.gcount());
                            return true;
                        });
                } else {
                    res.status = 404;
                    res.set_content("Not found", "text/plain");
                }
            });
            svr.Get("/previews/(.*)", [this](const httplib::Request& req, httplib::Response& res){
                std::string file = req.matches[1];
                std::string path = preview_dir_ + "/" + file;
                if (std::filesystem::exists(path)) {
                    res.set_content_provider(
                        std::filesystem::file_size(path),
                        "video/mp4",
                        [path](size_t offset, size_t length, httplib::DataSink &sink) {
                            std::ifstream ifs(path, std::ios::binary);
                            ifs.seekg(offset);
                            std::vector<char> buf(length);
                            ifs.read(buf.data(), length);
                            sink.write(buf.data(), ifs.gcount());
                            return true;
                        });
                } else {
                    res.status = 404;
                    res.set_content("Not found", "text/plain");
                }
            });

            // SSE endpoint - we will keep connections and push events to them
            svr.Get("/events", [this](const httplib::Request& req, httplib::Response& res){
                res.set_header("Content-Type", "text/event-stream");
                res.set_header("Cache-Control", "no-cache");
                res.set_header("Connection", "keep-alive");
                // hold connection open and periodically send keepalive; in this simplified version we do not keep per-client state here
                // This code will block the worker thread; for production use a proper async SSE implementation.
                res.set_chunked_content_provider("text/event-stream", [this](size_t offset, httplib::DataSink &sink){
                    // send queued events
                    while (server_thread_running_) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        // we don't have per-client queue in this minimal server; to send real events the server should push to every client.
                        // We'll rely on notify callback to write events via a shared sink (complex in single-file demo).
                        if (!server_thread_running_) break;
                    }
                    return true;
                });
            });

            svr.listen("0.0.0.0", port_);
        });
    }

    void stop() {
        server_thread_running_ = false;
        // stop not implemented for brevity
    }

private:
    int port_;
    std::string static_dir_;
    std::string upload_dir_;
    std::string preview_dir_;
    std::thread server_thread_;
    std::atomic<bool> server_thread_running_;
};
