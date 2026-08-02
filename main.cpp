#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

const int MAX_KEY_SIZE = 65;
const int ORDER = 100; // B+ tree order

struct KeyValue {
    char key[MAX_KEY_SIZE];
    int value;

    KeyValue() {
        memset(key, 0, MAX_KEY_SIZE);
        value = 0;
    }

    KeyValue(const char* k, int v) : value(v) {
        strncpy(key, k, MAX_KEY_SIZE - 1);
        key[MAX_KEY_SIZE - 1] = '\0';
    }

    bool operator<(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const KeyValue& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

class BPlusTree {
private:
    std::fstream file;
    std::string filename;

    struct Node {
        bool is_leaf;
        int num_keys;
        KeyValue keys[ORDER];
        long children[ORDER + 1]; // file positions for internal nodes
        long next_leaf; // for leaf nodes

        Node() : is_leaf(true), num_keys(0), next_leaf(-1) {
            for (int i = 0; i <= ORDER; i++) {
                children[i] = -1;
            }
        }
    };

    long root_pos;

    long allocate_node() {
        file.seekp(0, std::ios::end);
        return file.tellp();
    }

    void write_node(long pos, const Node& node) {
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }

    Node read_node(long pos) {
        Node node;
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        return node;
    }

    void split_child(Node& parent, int index, long child_pos) {
        Node child = read_node(child_pos);
        Node new_node;
        new_node.is_leaf = child.is_leaf;

        int mid = ORDER / 2;
        new_node.num_keys = ORDER - mid;

        for (int i = 0; i < new_node.num_keys; i++) {
            new_node.keys[i] = child.keys[mid + i];
        }

        if (!child.is_leaf) {
            for (int i = 0; i <= new_node.num_keys; i++) {
                new_node.children[i] = child.children[mid + i];
            }
        } else {
            new_node.next_leaf = child.next_leaf;
            child.next_leaf = allocate_node();
        }

        child.num_keys = mid;

        long new_pos = child.next_leaf;
        if (child.is_leaf) {
            write_node(new_pos, new_node);
        } else {
            new_pos = allocate_node();
            write_node(new_pos, new_node);
        }
        write_node(child_pos, child);

        for (int i = parent.num_keys; i > index; i--) {
            parent.children[i + 1] = parent.children[i];
            parent.keys[i] = parent.keys[i - 1];
        }

        parent.children[index + 1] = new_pos;
        parent.keys[index] = new_node.keys[0];
        parent.num_keys++;
    }

    void insert_non_full(long pos, const KeyValue& kv) {
        Node node = read_node(pos);

        if (node.is_leaf) {
            int i = node.num_keys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = kv;
            node.num_keys++;
            write_node(pos, node);
        } else {
            int i = node.num_keys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                i--;
            }
            i++;

            Node child = read_node(node.children[i]);
            if (child.num_keys == ORDER) {
                split_child(node, i, node.children[i]);
                write_node(pos, node);
                node = read_node(pos);
                if (node.keys[i] < kv || kv < node.keys[i]) {
                    i++;
                }
            }
            insert_non_full(node.children[i], kv);
        }
    }

public:
    BPlusTree(const std::string& fname) : filename(fname), root_pos(0) {
        std::ifstream test(filename);
        bool exists = test.good();
        test.close();

        if (exists) {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        } else {
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
            Node root;
            write_node(0, root);
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* key, int value) {
        KeyValue kv(key, value);
        Node root = read_node(root_pos);

        if (root.num_keys == ORDER) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.num_keys = 0;
            new_root.children[0] = root_pos;

            long new_root_pos = allocate_node();
            write_node(new_root_pos, new_root);

            split_child(new_root, 0, root_pos);
            insert_non_full(new_root_pos, kv);

            root_pos = new_root_pos;
        } else {
            insert_non_full(root_pos, kv);
        }
    }

    std::vector<int> find(const char* key) {
        std::vector<int> result;
        Node node = read_node(root_pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.num_keys && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            if (i > 0 && strcmp(key, node.keys[i - 1].key) == 0) {
                i--;
            }
            node = read_node(node.children[i]);
        }

        for (int i = 0; i < node.num_keys; i++) {
            if (strcmp(node.keys[i].key, key) == 0) {
                result.push_back(node.keys[i].value);
            }
        }

        while (node.next_leaf != -1) {
            node = read_node(node.next_leaf);
            if (node.num_keys > 0 && strcmp(node.keys[0].key, key) == 0) {
                for (int i = 0; i < node.num_keys && strcmp(node.keys[i].key, key) == 0; i++) {
                    result.push_back(node.keys[i].value);
                }
            } else {
                break;
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    bool remove_from_leaf(long pos, const KeyValue& kv) {
        Node node = read_node(pos);

        if (!node.is_leaf) {
            return false;
        }

        int idx = -1;
        for (int i = 0; i < node.num_keys; i++) {
            if (node.keys[i] == kv) {
                idx = i;
                break;
            }
        }

        if (idx == -1) {
            return false;
        }

        for (int i = idx; i < node.num_keys - 1; i++) {
            node.keys[i] = node.keys[i + 1];
        }
        node.num_keys--;
        write_node(pos, node);
        return true;
    }

    void remove(const char* key, int value) {
        KeyValue kv(key, value);
        Node node = read_node(root_pos);

        // Navigate to the leaf node
        while (!node.is_leaf) {
            int i = 0;
            while (i < node.num_keys && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            if (i > 0 && strcmp(key, node.keys[i - 1].key) == 0) {
                i--;
            }
            node = read_node(node.children[i]);
        }

        // Find and remove from leaf
        long current_pos = root_pos;
        node = read_node(root_pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.num_keys && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            if (i > 0 && strcmp(key, node.keys[i - 1].key) == 0) {
                i--;
            }
            current_pos = node.children[i];
            node = read_node(current_pos);
        }

        remove_from_leaf(current_pos, kv);

        // Also check next leaves for the same key
        while (node.next_leaf != -1) {
            long next_pos = node.next_leaf;
            Node next_node = read_node(next_pos);
            if (next_node.num_keys > 0 && strcmp(next_node.keys[0].key, key) == 0) {
                if (remove_from_leaf(next_pos, kv)) {
                    return;
                }
            } else {
                break;
            }
            node = next_node;
        }
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    BPlusTree tree("database.dat");

    int n;
    std::cin >> n;

    std::string cmd;
    for (int i = 0; i < n; i++) {
        std::cin >> cmd;

        if (cmd == "insert") {
            std::string key;
            int value;
            std::cin >> key >> value;
            tree.insert(key.c_str(), value);
        } else if (cmd == "find") {
            std::string key;
            std::cin >> key;
            std::vector<int> result = tree.find(key.c_str());

            if (result.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << result[j];
                }
                std::cout << "\n";
            }
        } else if (cmd == "delete") {
            std::string key;
            int value;
            std::cin >> key >> value;
            tree.remove(key.c_str(), value);
        }
    }

    return 0;
}
