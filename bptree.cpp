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

void BPTree::redistributeLeaves(int leaf_offset, int sibling_offset, int parent_offset, int index) {
    Node leaf = readNode(leaf_offset);
    Node sibling = readNode(sibling_offset);
    Node parent = readNode(parent_offset);

    if (leaf_offset < sibling_offset) {
        // Move one key from sibling to leaf
        for (int i = leaf.num_keys; i > 0; i--) {
            leaf.keys[i] = leaf.keys[i-1];
        }
        leaf.keys[0] = sibling.keys[0];
        leaf.num_keys++;

        // Shift sibling keys
        for (int i = 0; i < sibling.num_keys - 1; i++) {
            sibling.keys[i] = sibling.keys[i+1];
        }
        sibling.num_keys--;

        // Update parent key
        parent.keys[index] = sibling.keys[0];
    } else {
        // Move one key from leaf to sibling
        sibling.keys[sibling.num_keys] = leaf.keys[leaf.num_keys - 1];
        sibling.num_keys++;
        leaf.num_keys--;

        // Update parent key
        parent.keys[index] = leaf.keys[leaf.num_keys - 1];
    }

    writeNode(leaf_offset, leaf);
    writeNode(sibling_offset, sibling);
    writeNode(parent_offset, parent);
}

void BPTree::redistributeInternal(int node_offset, int sibling_offset, int parent_offset, int index) {
    // Redistribute keys and children between internal node and sibling
    Node node = readNode(node_offset);
    Node sibling = readNode(sibling_offset);
    Node parent = readNode(parent_offset);

    if (node_offset < sibling_offset) {
        // Move one key from sibling to node
        for (int i = node.num_keys; i > 0; i--) {
            node.keys[i] = node.keys[i-1];
            node.children[i+1] = node.children[i];
        }
        node.children[1] = node.children[0];

        node.keys[0] = parent.keys[index];
        node.children[0] = sibling.children[sibling.num_keys];
        node.num_keys++;

        parent.keys[index] = sibling.keys[sibling.num_keys - 1];
        sibling.num_keys--;
    } else {
        // Move one key from node to sibling
        sibling.keys[sibling.num_keys] = parent.keys[index];
        sibling.children[sibling.num_keys + 1] = node.children[node.num_keys];
        sibling.num_keys++;

        parent.keys[index] = node.keys[node.num_keys - 1];
        node.num_keys--;
    }

    writeNode(node_offset, node);
    writeNode(sibling_offset, sibling);
    writeNode(parent_offset, parent);
}

void BPTree::coalesceLeaves(int left_offset, int right_offset, int parent_offset, int index) {
    Node left = readNode(left_offset);
    Node right = readNode(right_offset);
    Node parent = readNode(parent_offset);

    // Move all keys from right to left
    for (int i = 0; i < right.num_keys; i++) {
        left.keys[left.num_keys + i] = right.keys[i];
    }
    left.num_keys += right.num_keys;
    left.next_leaf = right.next_leaf;

    // Remove right node
    freeNode(right_offset);

    // Remove key from parent
    for (int i = index; i < parent.num_keys - 1; i++) {
        parent.keys[i] = parent.keys[i+1];
        parent.children[i+1] = parent.children[i+2];
    }
    parent.num_keys--;

    writeNode(left_offset, left);
    writeNode(parent_offset, parent);

    // Handle parent underflow
    if (parent.num_keys < MIN_KEYS && parent_offset != root_offset) {
        deleteEntry(parent_offset, parent.keys[0]);
    }
}

void BPTree::coalesceInternal(int node_offset, int sibling_offset, int parent_offset, int index) {
    // Merge internal node and sibling
    Node node = readNode(node_offset);
    Node sibling = readNode(sibling_offset);
    Node parent = readNode(parent_offset);

    // Pull down key from parent
    if (node_offset < sibling_offset) {
        node.keys[node.num_keys] = parent.keys[index];
        node.children[node.num_keys + 1] = sibling.children[0];
        node.num_keys++;

        // Move all keys and children from sibling to node
        for (int i = 0; i < sibling.num_keys; i++) {
            node.keys[node.num_keys + i] = sibling.keys[i];
            node.children[node.num_keys + i + 1] = sibling.children[i + 1];
        }
        node.num_keys += sibling.num_keys;

        writeNode(node_offset, node);
    } else {
        // Pull down key from parent
        for (int i = sibling.num_keys; i > 0; i--) {
            sibling.keys[i] = sibling.keys[i-1];
            sibling.children[i+1] = sibling.children[i];
        }
        sibling.children[1] = sibling.children[0];
        sibling.keys[0] = parent.keys[index];
        sibling.num_keys++;

        // Move all keys and children from node to sibling
        for (int i = 0; i < node.num_keys; i++) {
            sibling.keys[sibling.num_keys + i] = node.keys[i];
            sibling.children[sibling.num_keys + i + 1] = node.children[i + 1];
        }
        sibling.num_keys += node.num_keys;

        writeNode(sibling_offset, sibling);
    }

    // Free the empty node
    freeNode(node_offset < sibling_offset ? sibling_offset : node_offset);

    // Remove key from parent
    for (int i = index; i < parent.num_keys - 1; i++) {
        parent.keys[i] = parent.keys[i+1];
        parent.children[i+1] = parent.children[i+2];
    }
    parent.num_keys--;

    writeNode(parent_offset, parent);

    // Handle parent underflow
    if (parent.num_keys < MIN_KEYS && parent_offset != root_offset) {
        deleteEntry(parent_offset, parent.keys[0]);
    }
}

void BPTree::splitInternal(int internal_offset) {
    Node internal = readNode(internal_offset);
    Node new_internal;
    new_internal.is_leaf = false;
    new_internal.num_keys = 0;

    // Split keys and children
    int split_point = (internal.num_keys + 1) / 2;
    KeyValue mid_key = internal.keys[split_point - 1];

    // Move upper half to new node
    for (int i = split_point; i < internal.num_keys; i++) {
        new_internal.keys[i - split_point] = internal.keys[i];
        new_internal.children[i - split_point] = internal.children[i];
        new_internal.num_keys++;
    }
    new_internal.children[new_internal.num_keys] = internal.children[internal.num_keys];

    internal.num_keys = split_point - 1;

    // Allocate space for new internal node
    int new_internal_offset = allocateNode();
    writeNode(new_internal_offset, new_internal);
    writeNode(internal_offset, internal);

    // Insert mid_key in parent
    insertInParent(internal_offset, mid_key, new_internal_offset);
}

void BPTree::remove(const std::string& key, int value) {
    KeyValue kv(key, value);
    int leaf_offset = findLeaf(key);
    if (leaf_offset == -1) return;

    Node leaf = readNode(leaf_offset);

    bool found = false;
    for (int i = 0; i < leaf.num_keys; i++) {
        if (leaf.keys[i] == kv) {
            found = true;
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
    Node node = readNode(node_offset);

    if (node.is_leaf) {
        if (node.num_keys >= MIN_KEYS || node_offset == root_offset) {
            // No underflow or is root
            return;
        }

        // Find parent and siblings
        int parent_offset = -1;
        int left_sibling = -1;
        int right_sibling = -1;
        int node_index = -1;

        // Search for parent (simplified - in production, maintain parent pointers)
        std::vector<std::pair<int, int>> stack;  // node_offset, child_index
        stack.push_back({root_offset, 0});

        while (!stack.empty()) {
            auto current = stack.back();
            stack.pop_back();
            Node current_node = readNode(current.first);

            if (!current_node.is_leaf) {
                for (int i = 0; i <= current_node.num_keys; i++) {
                    if (current_node.children[i] == node_offset) {
                        parent_offset = current.first;
                        node_index = i;
                        if (i > 0) left_sibling = current_node.children[i-1];
                        if (i < current_node.num_keys) right_sibling = current_node.children[i+1];
                        break;
                    }
                    stack.push_back({current_node.children[i], i});
                }
            }
            if (parent_offset != -1) break;
        }

        if (parent_offset == -1) return;

        // Try redistribution first
        if (left_sibling != -1) {
            Node left = readNode(left_sibling);
            if (left.num_keys > MIN_KEYS) {
                redistributeLeaves(node_offset, left_sibling, parent_offset, node_index);
                return;
            }
        }

        if (right_sibling != -1) {
            Node right = readNode(right_sibling);
            if (right.num_keys > MIN_KEYS) {
                redistributeLeaves(right_sibling, node_offset, parent_offset, node_index + 1);
                return;
            }
        }

        // Coalescing if redistribution not possible
        if (left_sibling != -1) {
            coalesceLeaves(left_sibling, node_offset, parent_offset, node_index);
        } else if (right_sibling != -1) {
            coalesceLeaves(node_offset, right_sibling, parent_offset, node_index);
        }
    }
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