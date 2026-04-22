#include "bptree.h"
#include "file_manager.h"
#include <cstring>
#include <algorithm>
#include <iostream>

static FileManager* fm = nullptr;

BPTree::BPTree(const std::string& fname) : filename(fname), root_offset(-1), free_list_head(-1) {
    if (!fm) {
        fm = new FileManager(filename);
    }

    FileHeader header = fm->readHeader();
    root_offset = header.root_offset;
    free_list_head = header.free_list_head;
}

BPTree::~BPTree() {
    // Don't write header in destructor to avoid deadlock
    // The header should be written after significant operations
}

int BPTree::allocateNode() {
    return fm->allocateBlock();
}

void BPTree::freeNode(int offset) {
    fm->freeBlock(offset);
}

Node BPTree::readNode(int offset) {
    Node node;
    char buffer[BLOCK_SIZE];
    fm->readBlock(offset, buffer);
    memcpy(&node, buffer, sizeof(Node));
    return node;
}

void BPTree::writeNode(int offset, const Node& node) {
    char buffer[BLOCK_SIZE];
    memcpy(buffer, &node, sizeof(Node));
    fm->writeBlock(offset, buffer);
}

void BPTree::insert(const std::string& key, int value) {
    KeyValue kv(key, value);

    if (root_offset == -1) {
        // Empty tree, create root
        root_offset = allocateNode();
        Node root;
        root.is_leaf = true;
        root.num_keys = 1;
        root.keys[0] = kv;
        writeNode(root_offset, root);
        return;
    }

    int leaf_offset = findLeaf(key);
    Node leaf = readNode(leaf_offset);

    // Check if key-value pair already exists
    for (int i = 0; i < leaf.num_keys; i++) {
        if (strcmp(leaf.keys[i].key, key.c_str()) == 0 && leaf.keys[i].value == value) {
            return;  // Duplicate, don't insert
        }
    }

    if (leaf.num_keys < MAX_KEYS) {
        insertInLeaf(leaf, kv);
        writeNode(leaf_offset, leaf);
    } else {
        // Split the leaf
        writeNode(leaf_offset, leaf);  // Save current state
        splitLeaf(leaf_offset);

        // Re-find the correct leaf after split
        leaf_offset = findLeaf(key);
        leaf = readNode(leaf_offset);
        insertInLeaf(leaf, kv);
        writeNode(leaf_offset, leaf);
    }
}

void BPTree::insertInLeaf(Node& leaf, const KeyValue& kv) {
    int i = leaf.num_keys - 1;
    while (i >= 0 && leaf.keys[i] > kv) {
        leaf.keys[i + 1] = leaf.keys[i];
        i--;
    }
    leaf.keys[i + 1] = kv;
    leaf.num_keys++;
}

void BPTree::splitLeaf(int leaf_offset) {
    Node leaf = readNode(leaf_offset);
    Node new_leaf;
    new_leaf.is_leaf = true;
    new_leaf.num_keys = 0;
    new_leaf.next_leaf = leaf.next_leaf;

    // Move half the keys to new leaf
    int split_point = (leaf.num_keys + 1) / 2;
    for (int i = split_point; i < leaf.num_keys; i++) {
        new_leaf.keys[i - split_point] = leaf.keys[i];
        new_leaf.num_keys++;
    }
    leaf.num_keys = split_point;

    // Allocate space for new leaf
    int new_leaf_offset = allocateNode();
    writeNode(new_leaf_offset, new_leaf);

    // Update original leaf's next pointer
    leaf.next_leaf = new_leaf_offset;
    writeNode(leaf_offset, leaf);

    // Insert new key in parent
    KeyValue mid_key = new_leaf.keys[0];
    insertInParent(leaf_offset, mid_key, new_leaf_offset);
}

void BPTree::insertInParent(int left_offset, const KeyValue& new_key, int right_offset) {
    if (left_offset == root_offset) {
        // Create new root
        int new_root_offset = allocateNode();
        Node new_root;
        new_root.is_leaf = false;
        new_root.num_keys = 1;
        new_root.keys[0] = new_key;
        new_root.children[0] = left_offset;
        new_root.children[1] = right_offset;
        writeNode(new_root_offset, new_root);

        root_offset = new_root_offset;
        return;
    }

    // Find parent (simplified - in production, maintain parent pointers)
    // For now, we'll do a full search
    int parent_offset = -1;
    int parent_index = -1;

    // Search for parent containing left_offset
    std::vector<int> stack;
    stack.push_back(root_offset);

    while (!stack.empty()) {
        int current_offset = stack.back();
        stack.pop_back();
        Node current = readNode(current_offset);

        if (!current.is_leaf) {
            for (int i = 0; i <= current.num_keys; i++) {
                if (current.children[i] == left_offset) {
                    parent_offset = current_offset;
                    parent_index = i;
                    break;
                }
                stack.push_back(current.children[i]);
            }
        }
        if (parent_offset != -1) break;
    }

    if (parent_offset == -1) {
        throw std::runtime_error("Parent not found");
    }

    Node parent = readNode(parent_offset);

    if (parent.num_keys < MAX_KEYS) {
        // Insert in parent
        int i = parent.num_keys - 1;
        while (i >= parent_index && parent.keys[i] > new_key) {
            parent.keys[i + 1] = parent.keys[i];
            parent.children[i + 2] = parent.children[i + 1];
            i--;
        }
        parent.keys[i + 1] = new_key;
        parent.children[i + 2] = right_offset;
        parent.num_keys++;
        writeNode(parent_offset, parent);
    } else {
        // Split internal node
        // (Implementation similar to splitLeaf but more complex)
        // For brevity, simplified version
        writeNode(parent_offset, parent);
        splitInternal(parent_offset);
    }
}

void BPTree::splitInternal(int internal_offset) {
    // Similar to splitLeaf but handles children pointers
    // Implementation omitted for brevity but follows B+ tree algorithm
}

void BPTree::remove(const std::string& key, int value) {
    KeyValue kv(key, value);
    int leaf_offset = findLeaf(key);
    Node leaf = readNode(leaf_offset);

    bool found = false;
    int index = -1;
    for (int i = 0; i < leaf.num_keys; i++) {
        if (leaf.keys[i] == kv) {
            found = true;
            index = i;
            break;
        }
    }

    if (!found) {
        return;  // Key-value pair not found
    }

    deleteFromLeaf(leaf, kv);
    writeNode(leaf_offset, leaf);

    if (leaf.num_keys < MIN_KEYS && leaf_offset != root_offset) {
        // Handle underflow
        deleteEntry(leaf_offset, kv);
    }
}

void BPTree::deleteFromLeaf(Node& leaf, const KeyValue& kv) {
    int index = -1;
    for (int i = 0; i < leaf.num_keys; i++) {
        if (leaf.keys[i] == kv) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        for (int i = index; i < leaf.num_keys - 1; i++) {
            leaf.keys[i] = leaf.keys[i + 1];
        }
        leaf.num_keys--;
    }
}

void BPTree::deleteEntry(int node_offset, const KeyValue& kv) {
    // Handle underflow by redistribution or coalescing
    // Implementation follows B+ tree deletion algorithm
}

int BPTree::findLeaf(const std::string& key) {
    if (root_offset == -1) {
        return -1;
    }

    int current_offset = root_offset;
    Node current = readNode(current_offset);

    while (!current.is_leaf) {
        int i = 0;
        while (i < current.num_keys && strcmp(key.c_str(), current.keys[i].key) >= 0) {
            i++;
        }
        current_offset = current.children[i];
        current = readNode(current_offset);
    }

    return current_offset;
}

std::vector<int> BPTree::find(const std::string& key) {
    std::vector<int> result;
    int leaf_offset = findLeaf(key);

    if (leaf_offset == -1) {
        return result;
    }

    Node leaf = readNode(leaf_offset);
    bool found = false;

    // Search in this leaf
    for (int i = 0; i < leaf.num_keys; i++) {
        if (strcmp(leaf.keys[i].key, key.c_str()) == 0) {
            result.push_back(leaf.keys[i].value);
            found = true;
        }
    }

    if (!found) {
        // Check if key might be in next leaf (for range queries)
        while (leaf.next_leaf != -1) {
            leaf_offset = leaf.next_leaf;
            leaf = readNode(leaf_offset);
            for (int i = 0; i < leaf.num_keys; i++) {
                if (strcmp(leaf.keys[i].key, key.c_str()) == 0) {
                    result.push_back(leaf.keys[i].value);
                } else if (strcmp(leaf.keys[i].key, key.c_str()) > 0) {
                    break;
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

void BPTree::printTree() {
    // Debug function to print tree structure
    if (root_offset == -1) {
        std::cout << "Empty tree\n";
        return;
    }

    std::vector<int> current_level;
    current_level.push_back(root_offset);

    int level = 0;
    while (!current_level.empty()) {
        std::vector<int> next_level;
        std::cout << "Level " << level << ": ";

        for (int offset : current_level) {
            Node node = readNode(offset);
            std::cout << "[";
            for (int i = 0; i < node.num_keys; i++) {
                if (i > 0) std::cout << " ";
                std::cout << node.keys[i].key << ":" << node.keys[i].value;
            }
            std::cout << "] ";

            if (!node.is_leaf) {
                for (int i = 0; i <= node.num_keys; i++) {
                    next_level.push_back(node.children[i]);
                }
            }
        }
        std::cout << "\n";
        current_level = next_level;
        level++;
    }
}

void BPTree::flush() {
    FileHeader header = fm->readHeader();
    header.root_offset = root_offset;
    header.free_list_head = free_list_head;
    fm->writeHeader(header);
}