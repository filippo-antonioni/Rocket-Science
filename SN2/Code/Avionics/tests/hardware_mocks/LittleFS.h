#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#define FILE_WRITE "w"
#define FILE_READ "r"
struct File {
 std::shared_ptr<std::vector<uint8_t>> bytes;
 explicit operator bool() const;size_t write(const uint8_t*,size_t);size_t read(uint8_t*,size_t);
 void flush();void close();int available();const char* name();size_t size();
 bool isDirectory();File openNextFile();
};
struct FsMock {bool begin(bool);void end();bool format();size_t totalBytes();size_t usedBytes();
 bool exists(const char*);File open(const char*,const char* = FILE_READ);bool remove(const char*);
};extern FsMock LittleFS;
