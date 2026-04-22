#ifndef BPTREE_V2_H
#define BPTREE_V2_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>

// Simple in-memory implementation first, then persist to file
class BPTreeV2 {
private:
    std::map<std::string, std::set<int>> data;
    std::string filename;

    void loadFromFile();
    void saveToFile();

public:
    BPTreeV2(const std::string& fname);
    ~BPTreeV2();

    void insert(const std::string& key, int value);
    void remove(const std::string& key, int value);
    std::vector<int> find(const std::string& key);
};

#endif