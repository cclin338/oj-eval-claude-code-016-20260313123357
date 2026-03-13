#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

const int MAX_KEY_SIZE = 64;
const int ORDER = 100; // B+ tree order - max keys per node

struct Key {
    char index[MAX_KEY_SIZE + 1];
    int value;

    Key() : value(0) {
        memset(index, 0, sizeof(index));
    }

    Key(const char* idx, int val) : value(val) {
        memset(index, 0, sizeof(index));
        strncpy(index, idx, MAX_KEY_SIZE);
    }

    bool operator<(const Key& other) const {
        int cmp = strcmp(index, other.index);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Key& other) const {
        return strcmp(index, other.index) == 0 && value == other.value;
    }
};

struct Node {
    bool isLeaf;
    int keyCount;
    Key keys[ORDER];
    int children[ORDER + 1];
    int next; // for leaf nodes

    Node() : isLeaf(true), keyCount(0), next(-1) {
        memset(children, -1, sizeof(children));
    }
};

class BPlusTree {
private:
    std::fstream file;
    int rootPos;
    int nodeCount;
    const char* filename = "bptree.dat";

    int allocateNode() {
        return nodeCount++;
    }

    void readNode(int pos, Node& node) {
        file.seekg(sizeof(int) * 2 + pos * sizeof(Node));
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
    }

    void writeNode(int pos, const Node& node) {
        file.seekp(sizeof(int) * 2 + pos * sizeof(Node));
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }

    void writeMetadata() {
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(&rootPos), sizeof(int));
        file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(int));
        file.flush();
    }

    void readMetadata() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&rootPos), sizeof(int));
        file.read(reinterpret_cast<char*>(&nodeCount), sizeof(int));
    }

    int findChild(const Node& node, const Key& key) {
        int i = 0;
        while (i < node.keyCount && !(key < node.keys[i])) {
            i++;
        }
        return i;
    }

    void insertInLeaf(Node& leaf, const Key& key) {
        int i = leaf.keyCount - 1;
        while (i >= 0 && key < leaf.keys[i]) {
            leaf.keys[i + 1] = leaf.keys[i];
            i--;
        }
        leaf.keys[i + 1] = key;
        leaf.keyCount++;
    }

    void splitNode(int parentPos, int childIndex, int childPos) {
        Node parent, child;
        readNode(parentPos, parent);
        readNode(childPos, child);

        int mid = ORDER / 2;
        Node newNode;
        newNode.isLeaf = child.isLeaf;
        newNode.keyCount = ORDER - mid;

        // Copy keys
        for (int i = 0; i < newNode.keyCount; i++) {
            newNode.keys[i] = child.keys[mid + i];
        }

        if (!child.isLeaf) {
            for (int i = 0; i <= newNode.keyCount; i++) {
                newNode.children[i] = child.children[mid + i];
            }
        } else {
            newNode.next = child.next;
        }

        child.keyCount = mid;

        int newNodePos = allocateNode();
        if (child.isLeaf) {
            child.next = newNodePos;
        }

        // Insert new key in parent
        for (int i = parent.keyCount; i > childIndex; i--) {
            parent.children[i + 1] = parent.children[i];
        }
        parent.children[childIndex + 1] = newNodePos;

        for (int i = parent.keyCount - 1; i >= childIndex; i--) {
            parent.keys[i + 1] = parent.keys[i];
        }
        parent.keys[childIndex] = newNode.keys[0];
        parent.keyCount++;

        writeNode(childPos, child);
        writeNode(newNodePos, newNode);
        writeNode(parentPos, parent);
        writeMetadata();
    }

    void insertNonFull(int nodePos, const Key& key) {
        Node node;
        readNode(nodePos, node);

        if (node.isLeaf) {
            insertInLeaf(node, key);
            writeNode(nodePos, node);
        } else {
            int i = findChild(node, key);

            Node child;
            readNode(node.children[i], child);

            if (child.keyCount == ORDER) {
                splitNode(nodePos, i, node.children[i]);
                readNode(nodePos, node);

                if (!(key < node.keys[i])) {
                    i++;
                }
            }

            insertNonFull(node.children[i], key);
        }
    }

    int findLeafNode(const char* index) {
        Node node;
        int pos = rootPos;
        readNode(pos, node);

        while (!node.isLeaf) {
            Key searchKey(index, -2147483648);
            int i = findChild(node, searchKey);
            pos = node.children[i];
            readNode(pos, node);
        }

        return pos;
    }

    bool removeFromLeaf(int leafPos, const Key& key) {
        Node leaf;
        readNode(leafPos, leaf);

        int i = 0;
        while (i < leaf.keyCount && !(leaf.keys[i] == key)) {
            i++;
        }

        if (i >= leaf.keyCount) {
            return false; // Key not found
        }

        for (int j = i; j < leaf.keyCount - 1; j++) {
            leaf.keys[j] = leaf.keys[j + 1];
        }
        leaf.keyCount--;
        writeNode(leafPos, leaf);
        return true;
    }

public:
    BPlusTree() {
        std::ifstream testFile(filename);
        bool exists = testFile.good();
        testFile.close();

        if (exists) {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
            readMetadata();
        } else {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
            rootPos = 0;
            nodeCount = 1;
            Node root;
            root.isLeaf = true;
            writeNode(0, root);
            writeMetadata();
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* index, int value) {
        Key key(index, value);
        Node root;
        readNode(rootPos, root);

        if (root.keyCount == ORDER) {
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.keyCount = 0;
            newRoot.children[0] = rootPos;

            int newRootPos = allocateNode();
            rootPos = newRootPos;
            writeNode(newRootPos, newRoot);
            writeMetadata();

            splitNode(newRootPos, 0, newRoot.children[0]);
        }

        insertNonFull(rootPos, key);
    }

    std::vector<int> find(const char* index) {
        std::vector<int> result;

        // Navigate to leftmost leaf that could contain this index
        Key searchKey(index, -2147483648);
        Node node;
        int pos = rootPos;
        readNode(pos, node);

        // Navigate down the tree
        while (!node.isLeaf) {
            int i = findChild(node, searchKey);
            pos = node.children[i];
            readNode(pos, node);
        }

        // Now scan forward through leaf nodes
        // Continue scanning until we find a key > target
        int maxLeaves = 100; // Limit to prevent infinite loops
        int scanned = 0;
        while (pos != -1 && scanned < maxLeaves) {
            readNode(pos, node);
            scanned++;

            bool foundInThisLeaf = false;
            bool pastTarget = false;

            for (int i = 0; i < node.keyCount; i++) {
                int cmp = strcmp(node.keys[i].index, index);
                if (cmp == 0) {
                    result.push_back(node.keys[i].value);
                    foundInThisLeaf = true;
                } else if (cmp > 0) {
                    pastTarget = true;
                    break;
                }
            }

            //Stop if we passed the target AND we've found at least one result
            if (pastTarget && !result.empty()) {
                break;
            }

            // Also stop if we haven't found anything in this leaf
            // and the first key is already > target
            if (!foundInThisLeaf && node.keyCount > 0 && !result.empty()) {
                if (strcmp(node.keys[0].index, index) > 0) {
                    break;
                }
            }

            pos = node.next;
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    void remove(const char* index, int value) {
        Key key(index, value);
        int leafPos = findLeafNode(index);

        // Search through leaf nodes
        Node leaf;
        while (leafPos != -1) {
            readNode(leafPos, leaf);

            if (removeFromLeaf(leafPos, key)) {
                return;
            }

            // Check if we should continue to next leaf
            // Only continue if the last key in this leaf has the same index
            if (leaf.keyCount > 0 && strcmp(leaf.keys[leaf.keyCount - 1].index, index) == 0) {
                // Might be in next leaf with same index
                leafPos = leaf.next;
            } else {
                // No matching index in this leaf, and last key != target index
                // So we won't find it in later leaves either
                break;
            }
        }
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    BPlusTree tree;

    int n;
    std::cin >> n;

    for (int i = 0; i < n; i++) {
        std::string cmd;
        std::cin >> cmd;

        if (cmd == "insert") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            std::cin >> index >> value;
            tree.insert(index, value);
        } else if (cmd == "delete") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            std::cin >> index >> value;
            tree.remove(index, value);
        } else if (cmd == "find") {
            char index[MAX_KEY_SIZE + 1];
            std::cin >> index;
            std::vector<int> result = tree.find(index);

            if (result.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << result[j];
                }
                std::cout << "\n";
            }
        }
    }

    return 0;
}
