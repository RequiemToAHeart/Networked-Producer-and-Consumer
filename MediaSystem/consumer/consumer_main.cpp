#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "grpc_service.h"
#include "httplib.h"

int main(int argc, char** argv) {
    // Basic args
    size_t queue_capacity = 8;
    std::string storage_dir = "./uploads";
    std::string preview_dir = "./previews";
    int http_port = 8080;
    int grpc_port = 50051;

    std::filesystem::create_directories(storage_dir);
    std::filesystem::create_directories(preview_dir);

    // notify callback: in simple design we write a metadata file or append to a JSON that the web client polls
    auto notify = [&](const UploadItem& item, const std::string& preview_url, const std::string& final_url){
        // append to a small JSON lines file consumed by /api/list
        std::string meta = storage_dir + "/.uploads.jsonl";
        std::ofstream ofs(meta, std::ios::app);
        ofs << "{ \"filename\":\"" << item.filename << "\", \"preview\":\"" << preview_url << "\", \"url\":\"" << final_url << "\" }\n";
    };

    MediaUploadServiceImpl service(queue_capacity, storage_dir, preview_dir, notify);

    // start worker thread that forwards items to worker pool
    service.start_workers();

    // Start gRPC server
    std::string server_address = "0.0.0.0:" + std::to_string(grpc_port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "gRPC server listening on " << server_address << std::endl;

    // Start simple HTTP server for GUI (static files in ../web)
    httplib::Server svr;
    // Serve index.html and JS
    svr.Get("/", [](const httplib::Request&, httplib::Response& res){
        std::ifstream ifs("../web/index.html");
        std::stringstream ss; ss << ifs.rdbuf();
        res.set_content(ss.str(), "text/html");
    });
    svr.Get("/main.js", [](const httplib::Request&, httplib::Response& res){
        std::ifstream ifs("../web/main.js");
        std::stringstream ss; ss << ifs.rdbuf();
        res.set_content(ss.str(), "application/javascript");
    });

    svr.Get("/api/list", [&storage_dir](const httplib::Request&, httplib::Response& res){
        std::string meta = storage_dir + "/.uploads.jsonl";
        std::ifstream ifs(meta);
        std::string line;
        std::vector<std::string> rows;
        while (std::getline(ifs, line)) {
            rows.push_back(line);
        }
        std::string out = "[";
        for (size_t i=0;i<rows.size();++i) {
            out += rows[i];
            if (i+1<rows.size()) out += ",";
        }
        out += "]";
        res.set_content(out, "application/json");
    });

    // serve uploads and previews
    svr.set_mount_point("/uploads", storage_dir.c_str());
    svr.set_mount_point("/previews", preview_dir.c_str());

    std::thread http([&](){
        std::cout << "HTTP server at http://0.0.0.0:" << http_port << std::endl;
        svr.listen("0.0.0.0", http_port);
    });

    // block until gRPC server stops (in real deployment handle signals)
    server->Wait();
    svr.stop();
    http.join();
    return 0;
}
