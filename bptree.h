#ifndef BPTREE_H
#define BPTREE_H

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <cstring>

const int KEY_SIZE = 64;
const int MAX_KEYS = 30;  // Reduced to fit in 4KB block
const int MIN_KEYS = MAX_KEYS / 2;

struct KeyValue {
    char key[KEY_SIZE];
    int value;

    KeyValue() : value(0) {
        key[0] = '\0';
    }

    KeyValue(const std::string& k, int v) : value(v) {
        strncpy(key, k.c_str(), KEY_SIZE - 1);
        key[KEY_SIZE - 1] = '\0';
    }

    bool operator<(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp == 0) return value < other.value;
        return cmp < 0;
    }

    bool operator==(const KeyValue& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }

    bool operator>(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp == 0) return value > other.value;
        return cmp > 0;
    }
};

struct Node {
    bool is_leaf;
    int num_keys;
    KeyValue keys[MAX_KEYS];
    int children[MAX_KEYS + 1];  // For internal nodes: child node offsets
    int next_leaf;  // For leaf nodes: next leaf offset

    Node() : is_leaf(true), num_keys(0), next_leaf(-1) {
        for (int i = 0; i <= MAX_KEYS; i++) {
            children[i] = -1;
        }
    }
};

class BPTree {
private:
    std::string filename;
    int root_offset;
    int free_list_head;

    // File operations
    int allocateNode();
    void freeNode(int offset);
    Node readNode(int offset);
    void writeNode(int offset, const Node& node);

    // Tree operations
    void insertInLeaf(Node& leaf, const KeyValue& kv);
    void insertInParent(int leaf_offset, const KeyValue& new_key, int new_child);
    void splitLeaf(int leaf_offset);
    void splitInternal(int internal_offset);
    void deleteFromLeaf(Node& leaf, const KeyValue& kv);
    void deleteEntry(int node_offset, const KeyValue& kv);
    void redistributeLeaves(int leaf_offset, int sibling_offset, int parent_offset, int index);
    void redistributeInternal(int node_offset, int sibling_offset, int parent_offset, int index);
    void coalesceLeaves(int leaf_offset, int sibling_offset, int parent_offset, int index);
    void coalesceInternal(int node_offset, int sibling_offset, int parent_offset, int index);

    // Search operations
    int findLeaf(const std::string& key);
    std::vector<int> findAllValues(const std::string& key);

public:
    BPTree(const std::string& fname);
    ~BPTree();

    void insert(const std::string& key, int value);
    void remove(const std::string& key, int value);
    std::vector<int> find(const std::string& key);

    void printTree();
    void flush();  // Write header to disk
};

#endif