#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "grpc_service.h"
#include "httplib.h"
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc, char** argv) {
    size_t queue_capacity = 8;
    std::string storage_dir = "./uploads";
    std::string preview_dir = "./previews";
    int http_port = 8080;
    int grpc_port = 50051;

    std::filesystem::create_directories(storage_dir);
    std::filesystem::create_directories(preview_dir);

    auto notify = [&](const UploadItem& item, const std::string& preview_url, const std::string& final_url){
        std::string meta = storage_dir + "/.uploads.jsonl";
        std::ofstream ofs(meta, std::ios::app);
        ofs << "{ \"filename\":\"" << item.filename << "\", \"preview\":\"" << preview_url << "\", \"url\":\"" << final_url << "\" }\n";
    };

    MediaUploadServiceImpl service(queue_capacity, storage_dir, preview_dir, notify);

    service.start_workers();

    std::string server_address = "0.0.0.0:" + std::to_string(grpc_port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "gRPC server listening on " << server_address << std::endl;

    httplib::Server svr;
    
    svr.Get("/", [](const httplib::Request&, httplib::Response& res){
        std::ifstream ifs("../../../web/index.html");
        if (!ifs.is_open()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }
        std::stringstream ss; ss << ifs.rdbuf();
        res.set_content(ss.str(), "text/html");
    });

    svr.Get("/main.js", [](const httplib::Request&, httplib::Response& res){
        std::ifstream ifs("../../../web/main.js");
        if (!ifs.is_open()) {
            res.status = 404;
            res.set_content("main.js not found", "text/plain");
            return;
        }
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
    
    svr.Get("/api/stats", [&service](const httplib::Request&, httplib::Response& res){
        std::string json = "{\"duplicates\":" + std::to_string(service.get_duplicate_count()) + "}";
        res.set_content(json, "application/json");
    });

    svr.Post("/api/compress", [&storage_dir](const httplib::Request& req, httplib::Response& res){
        std::string body = req.body;
        size_t pos = body.find("\"filename\":\"");
        if (pos == std::string::npos) {
            res.set_content("{\"success\":false,\"message\":\"missing filename\"}", "application/json");
            return;
        }
        pos += 12;
        size_t end = body.find("\"", pos);
        std::string filename = body.substr(pos, end - pos);
        
        std::string original_path = storage_dir + "/" + filename;
        if (!std::filesystem::exists(original_path)) {
            res.set_content("{\"success\":false,\"message\":\"file not found\"}", "application/json");
            return;
        }
        
        std::string compressed_path = storage_dir + "/compressed_" + filename;
        std::string cmd = "ffmpeg -y -hide_banner -loglevel error -i \"" + original_path + "\" -c:v libx264 -preset medium -crf 23 -c:a aac -b:a 128k \"" + compressed_path + "\"";
        
        int rc = std::system(cmd.c_str());
        
        if (rc == 0 && std::filesystem::exists(compressed_path)) {
            size_t original_size = std::filesystem::file_size(original_path);
            size_t compressed_size = std::filesystem::file_size(compressed_path);
            double ratio = (100.0 * compressed_size / original_size);
            
            std::cout << "Compressed: " << filename 
                      << " (original: " << original_size 
                      << " bytes, compressed: " << compressed_size 
                      << " bytes, ratio: " << ratio << "%)" << std::endl;
            
            res.set_content("{\"success\":true,\"original_size\":" + std::to_string(original_size) + 
                          ",\"compressed_size\":" + std::to_string(compressed_size) + 
                          ",\"ratio\":" + std::to_string(ratio) + "}", "application/json");
        } else {
            res.set_content("{\"success\":false,\"message\":\"compression failed\"}", "application/json");
        }
    });

    svr.set_mount_point("/uploads", storage_dir.c_str());
    svr.set_mount_point("/previews", preview_dir.c_str());

    std::thread http([&](){
        std::cout << "HTTP server at http://0.0.0.0:" << http_port << std::endl;
        svr.listen("0.0.0.0", http_port);
    });

    server->Wait();
    svr.stop();
    http.join();
    return 0;
}