#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

const int BLOCK_SIZE = 4096;
const int MAX_KEY_SIZE = 64;
const int M = 100; // B+ tree order

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

    bool operator<=(const Key& other) const {
        return *this < other || *this == other;
    }
};

struct Node {
    bool isLeaf;
    int keyCount;
    Key keys[M];
    int children[M + 1]; // file positions for internal nodes
    int next; // next leaf node position (only for leaf nodes)

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
        int pos = nodeCount++;
        writeMetadata();
        return pos;
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

    void splitChild(int parentPos, int childIndex, Node& parent, Node& child) {
        Node newNode;
        newNode.isLeaf = child.isLeaf;

        int mid = M / 2;
        newNode.keyCount = M - mid;

        for (int i = 0; i < newNode.keyCount; i++) {
            newNode.keys[i] = child.keys[mid + i];
        }

        if (!child.isLeaf) {
            for (int i = 0; i <= newNode.keyCount; i++) {
                newNode.children[i] = child.children[mid + i];
            }
        } else {
            newNode.next = child.next;
            child.next = allocateNode();
            writeNode(child.next, newNode);
        }

        child.keyCount = mid;

        int newNodePos = (child.isLeaf ? child.next : allocateNode());
        if (!child.isLeaf) {
            writeNode(newNodePos, newNode);
        }

        for (int i = parent.keyCount; i > childIndex; i--) {
            parent.children[i + 1] = parent.children[i];
        }
        parent.children[childIndex + 1] = newNodePos;

        for (int i = parent.keyCount - 1; i >= childIndex; i--) {
            parent.keys[i + 1] = parent.keys[i];
        }
        parent.keys[childIndex] = (child.isLeaf ? child.keys[mid] : newNode.keys[0]);
        parent.keyCount++;
    }

    void insertNonFull(int nodePos, const Key& key) {
        Node node;
        readNode(nodePos, node);

        if (node.isLeaf) {
            int i = node.keyCount - 1;
            while (i >= 0 && key < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = key;
            node.keyCount++;
            writeNode(nodePos, node);
        } else {
            int i = node.keyCount - 1;
            while (i >= 0 && key < node.keys[i]) {
                i--;
            }
            i++;

            Node child;
            readNode(node.children[i], child);

            if (child.keyCount == M) {
                splitChild(nodePos, i, node, child);
                writeNode(node.children[i], child);
                writeNode(nodePos, node);

                if (node.keys[i] < key || node.keys[i] == key) {
                    i++;
                }
            }

            insertNonFull(node.children[i], key);
        }
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

        if (root.keyCount == M) {
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.keyCount = 0;
            int newRootPos = allocateNode();
            newRoot.children[0] = rootPos;

            splitChild(newRootPos, 0, newRoot, root);
            writeNode(rootPos, root);
            writeNode(newRootPos, newRoot);

            rootPos = newRootPos;
            writeMetadata();

            insertNonFull(rootPos, key);
        } else {
            insertNonFull(rootPos, key);
        }
    }

    std::vector<int> find(const char* index) {
        std::vector<int> result;
        Node node;
        readNode(rootPos, node);

        while (!node.isLeaf) {
            int i = 0;
            Key searchKey(index, 0);
            while (i < node.keyCount && node.keys[i].index[0] != 0) {
                int cmp = strcmp(searchKey.index, node.keys[i].index);
                if (cmp < 0) break;
                i++;
            }
            if (i < node.keyCount + 1 && node.children[i] != -1) {
                readNode(node.children[i], node);
            } else {
                return result;
            }
        }

        while (true) {
            for (int i = 0; i < node.keyCount; i++) {
                if (strcmp(node.keys[i].index, index) == 0) {
                    result.push_back(node.keys[i].value);
                }
            }

            if (node.next != -1) {
                Node nextNode;
                readNode(node.next, nextNode);
                if (nextNode.keyCount > 0 && strcmp(nextNode.keys[0].index, index) <= 0) {
                    node = nextNode;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    void remove(const char* index, int value) {
        // Simplified delete - just mark as deleted conceptually
        // For a full implementation, we would need complex rebalancing
        // This is a basic implementation that searches and removes from leaf

        Key key(index, value);
        Node node;
        readNode(rootPos, node);

        while (!node.isLeaf) {
            int i = 0;
            while (i < node.keyCount && node.keys[i].index[0] != 0) {
                int cmp = strcmp(key.index, node.keys[i].index);
                if (cmp < 0) break;
                i++;
            }
            if (i < node.keyCount + 1 && node.children[i] != -1) {
                readNode(node.children[i], node);
            } else {
                return;
            }
        }

        int nodePos = rootPos;
        // Find the actual node position (we need to track it during traversal)
        // Simplified: Linear search through all leaf nodes
        for (int pos = 0; pos < nodeCount; pos++) {
            readNode(pos, node);
            if (!node.isLeaf) continue;

            for (int i = 0; i < node.keyCount; i++) {
                if (node.keys[i] == key) {
                    for (int j = i; j < node.keyCount - 1; j++) {
                        node.keys[j] = node.keys[j + 1];
                    }
                    node.keyCount--;
                    writeNode(pos, node);
                    return;
                }
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
