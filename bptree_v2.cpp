#include "bptree_v2.h"
#include <algorithm>
#include <iostream>

BPTreeV2::BPTreeV2(const std::string& fname) : filename(fname) {
    loadFromFile();
}

BPTreeV2::~BPTreeV2() {
    saveToFile();
}

void BPTreeV2::loadFromFile() {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return;

    size_t num_keys;
    file.read(reinterpret_cast<char*>(&num_keys), sizeof(num_keys));

    for (size_t i = 0; i < num_keys; i++) {
        size_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        size_t num_values;
        file.read(reinterpret_cast<char*>(&num_values), sizeof(num_values));

        for (size_t j = 0; j < num_values; j++) {
            int value;
            file.read(reinterpret_cast<char*>(&value), sizeof(value));
            data[key].insert(value);
        }
    }

    file.close();
}

void BPTreeV2::saveToFile() {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return;

    size_t num_keys = data.size();
    file.write(reinterpret_cast<const char*>(&num_keys), sizeof(num_keys));

    for (const auto& [key, values] : data) {
        size_t key_len = key.length();
        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.c_str(), key_len);

        size_t num_values = values.size();
        file.write(reinterpret_cast<const char*>(&num_values), sizeof(num_values));

        for (int value : values) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
    }

    file.close();
}

void BPTreeV2::insert(const std::string& key, int value) {
    data[key].insert(value);
}

void BPTreeV2::remove(const std::string& key, int value) {
    auto it = data.find(key);
    if (it != data.end()) {
        it->second.erase(value);
        if (it->second.empty()) {
            data.erase(it);
        }
    }
}

std::vector<int> BPTreeV2::find(const std::string& key) {
    std::vector<int> result;
    auto it = data.find(key);
    if (it != data.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}