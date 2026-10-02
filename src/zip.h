#pragma once
#include "common.h"

// Minimal ZIP reader (stored + deflate) used for .wsz skins.
struct ZipEntry {
    std::string name;  // full path inside the archive (as stored)
    std::vector<uint8_t> data;
};

bool ZipReadAll(const std::vector<uint8_t>& archive, std::vector<ZipEntry>& out);
bool Inflate(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out, size_t expected);
