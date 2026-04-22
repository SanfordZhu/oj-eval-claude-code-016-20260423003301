#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <fstream>
#include <mutex>

const int BLOCK_SIZE = 4096;
const int HEADER_SIZE = 1024;

struct FileHeader {
    int root_offset;
    int free_list_head;
    int total_blocks;
    int reserved[250];  // Padding to 1KB

    FileHeader() : root_offset(-1), free_list_head(-1), total_blocks(0) {
        for (int i = 0; i < 250; i++) {
            reserved[i] = 0;
        }
    }
};

class FileManager {
private:
    std::string filename;
    std::fstream file;
    mutable std::mutex mutex;

public:
    FileManager(const std::string& fname);
    ~FileManager();

    void createFile();
    bool fileExists();

    FileHeader readHeader();
    void writeHeader(const FileHeader& header);

    void readBlock(int offset, char* buffer);
    void writeBlock(int offset, const char* buffer);

    int allocateBlock();
    void freeBlock(int offset);

    void flush();
};

#endif