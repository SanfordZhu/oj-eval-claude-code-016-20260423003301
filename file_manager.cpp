#include "file_manager.h"
#include <cstring>
#include <iostream>

FileManager::FileManager(const std::string& fname) : filename(fname) {
    if (!fileExists()) {
        createFile();
    }
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        // Try creating with trunc flag
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
    }
}

FileManager::~FileManager() {
    if (file.is_open()) {
        file.close();
    }
}

void FileManager::createFile() {
    std::ofstream newFile(filename, std::ios::binary);
    if (!newFile.is_open()) {
        throw std::runtime_error("Failed to create file: " + filename);
    }

    // Write initial header
    FileHeader header;
    newFile.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));

    newFile.close();
}

bool FileManager::fileExists() {
    std::ifstream test(filename);
    bool exists = test.good();
    test.close();
    return exists;
}

FileHeader FileManager::readHeader() {
    // Note: Assumes caller already holds lock if needed
    FileHeader header;
    file.seekg(0);
    file.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
    if (!file.good()) {
        throw std::runtime_error("Failed to read header");
    }
    return header;
}

void FileManager::writeHeader(const FileHeader& header) {
    // Note: Assumes caller already holds lock if needed
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));
    if (!file.good()) {
        throw std::runtime_error("Failed to write header");
    }
}

void FileManager::readBlock(int offset, char* buffer) {
    std::lock_guard<std::mutex> lock(mutex);
    file.seekg(HEADER_SIZE + offset * BLOCK_SIZE);
    file.read(buffer, BLOCK_SIZE);
    if (!file.good()) {
        throw std::runtime_error("Failed to read block at offset " + std::to_string(offset));
    }
}

void FileManager::writeBlock(int offset, const char* buffer) {
    std::lock_guard<std::mutex> lock(mutex);
    file.seekp(HEADER_SIZE + offset * BLOCK_SIZE);
    file.write(buffer, BLOCK_SIZE);
    if (!file.good()) {
        throw std::runtime_error("Failed to write block at offset " + std::to_string(offset));
    }
}

int FileManager::allocateBlock() {
    std::lock_guard<std::mutex> lock(mutex);
    FileHeader header = readHeader();

    int offset;
    if (header.free_list_head != -1) {
        // Use block from free list
        offset = header.free_list_head;

        // Read the next free block pointer
        char buffer[BLOCK_SIZE];
        readBlock(offset, buffer);
        int next_free = *reinterpret_cast<int*>(buffer);

        header.free_list_head = next_free;
    } else {
        // Allocate new block
        offset = header.total_blocks++;
    }

    writeHeader(header);
    return offset;
}

void FileManager::freeBlock(int offset) {
    std::lock_guard<std::mutex> lock(mutex);
    FileHeader header = readHeader();

    // Add to free list
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    *reinterpret_cast<int*>(buffer) = header.free_list_head;
    writeBlock(offset, buffer);

    header.free_list_head = offset;
    writeHeader(header);
}

void FileManager::flush() {
    std::lock_guard<std::mutex> lock(mutex);
    file.flush();
}