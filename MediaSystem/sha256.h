#pragma once
#include <string>
#include <openssl/sha.h>
#include <fstream>
#include <iomanip>
#include <sstream>

inline std::string sha256_file(const std::string& path) {
    unsigned char buffer[8192];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::ifstream ifs(path, std::ios::binary);
    while (ifs.good()) {
        ifs.read((char*)buffer, sizeof(buffer));
        std::streamsize s = ifs.gcount();
        if (s > 0) SHA256_Update(&ctx, buffer, s);
    }
    SHA256_Final(hash, &ctx);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i=0;i<SHA256_DIGEST_LENGTH;i++) ss << std::setw(2) << (int)hash[i];
    return ss.str();
}
