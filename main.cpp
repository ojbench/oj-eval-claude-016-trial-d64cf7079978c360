#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

const int MAX_KEY_SIZE = 65;
const int M = 85; // Order of B+ tree (can store M-1 keys)

struct Entry {
    char key[MAX_KEY_SIZE];
    int value;

    Entry() {
        memset(key, 0, MAX_KEY_SIZE);
        value = 0;
    }

    Entry(const char* k, int v) : value(v) {
        strncpy(key, k, MAX_KEY_SIZE - 1);
        key[MAX_KEY_SIZE - 1] = '\0';
    }

    bool operator<(const Entry& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Entry& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

struct Node {
    bool is_leaf;
    int n; // number of keys
    Entry entries[M];
    int children[M + 1]; // file offsets for children (for internal nodes)
    int next; // next leaf node (for leaf nodes)

    Node() : is_leaf(true), n(0), next(-1) {
        for (int i = 0; i <= M; i++) {
            children[i] = -1;
        }
    }
};

class BPlusTree {
private:
    std::fstream file;
    std::string filename;
    int root_offset;
    int next_offset;

    void init_file() {
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        root_offset = sizeof(int) * 2;
        next_offset = root_offset + sizeof(Node);

        file.seekp(0);
        file.write(reinterpret_cast<char*>(&root_offset), sizeof(int));
        file.write(reinterpret_cast<char*>(&next_offset), sizeof(int));

        Node root;
        write_node(root_offset, root);
        file.flush();
    }

    void load_metadata() {
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&root_offset), sizeof(int));
        file.read(reinterpret_cast<char*>(&next_offset), sizeof(int));
    }

    void save_metadata() {
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&root_offset), sizeof(int));
        file.write(reinterpret_cast<char*>(&next_offset), sizeof(int));
        file.flush();
    }

    int alloc_node() {
        int offset = next_offset;
        next_offset += sizeof(Node);
        save_metadata();
        return offset;
    }

    Node read_node(int offset) {
        Node node;
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        return node;
    }

    void write_node(int offset, const Node& node) {
        file.seekp(offset);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }

    int find_child_index(const Node& node, const Entry& entry) {
        int i = 0;
        while (i < node.n && node.entries[i] < entry) {
            i++;
        }
        return i;
    }

    void split_child(Node& parent, int index) {
        Node full_child = read_node(parent.children[index]);
        Node new_child;
        new_child.is_leaf = full_child.is_leaf;

        int mid = M / 2;

        // Move second half to new node
        new_child.n = full_child.n - mid;
        for (int i = 0; i < new_child.n; i++) {
            new_child.entries[i] = full_child.entries[mid + i];
        }

        if (!full_child.is_leaf) {
            for (int i = 0; i <= new_child.n; i++) {
                new_child.children[i] = full_child.children[mid + i];
            }
        } else {
            new_child.next = full_child.next;
        }

        full_child.n = mid;

        int new_offset = alloc_node();

        if (full_child.is_leaf) {
            full_child.next = new_offset;
        }

        write_node(parent.children[index], full_child);
        write_node(new_offset, new_child);

        // Insert new key into parent
        for (int i = parent.n; i > index; i--) {
            parent.entries[i] = parent.entries[i - 1];
            parent.children[i + 1] = parent.children[i];
        }

        parent.entries[index] = new_child.entries[0];
        parent.children[index + 1] = new_offset;
        parent.n++;
    }

    void insert_non_full(int offset, const Entry& entry) {
        Node node = read_node(offset);

        if (node.is_leaf) {
            // Check if entry already exists
            for (int i = 0; i < node.n; i++) {
                if (node.entries[i] == entry) {
                    return; // Duplicate
                }
            }

            // Insert in sorted order
            int i = node.n - 1;
            while (i >= 0 && entry < node.entries[i]) {
                node.entries[i + 1] = node.entries[i];
                i--;
            }
            node.entries[i + 1] = entry;
            node.n++;
            write_node(offset, node);
        } else {
            // Find child to insert into
            int i = node.n - 1;
            while (i >= 0 && entry < node.entries[i]) {
                i--;
            }
            i++;

            Node child = read_node(node.children[i]);
            if (child.n >= M - 1) {
                split_child(node, i);
                write_node(offset, node);
                node = read_node(offset);

                if (!(entry < node.entries[i])) {
                    i++;
                }
            }

            insert_non_full(node.children[i], entry);
        }
    }

public:
    BPlusTree(const std::string& fname) : filename(fname) {
        std::ifstream test(filename);
        bool exists = test.good();
        test.close();

        if (exists) {
            load_metadata();
        } else {
            init_file();
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const char* key, int value) {
        Entry entry(key, value);
        Node root = read_node(root_offset);

        if (root.n >= M - 1) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 0;
            int new_root_offset = alloc_node();
            new_root.children[0] = root_offset;

            write_node(new_root_offset, new_root);
            split_child(new_root, 0);

            root_offset = new_root_offset;
            save_metadata();

            insert_non_full(root_offset, entry);
        } else {
            insert_non_full(root_offset, entry);
        }
    }

    std::vector<int> find(const char* key) {
        std::vector<int> result;
        Node node = read_node(root_offset);

        // Navigate to leaf
        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && strcmp(key, node.entries[i].key) >= 0) {
                i++;
            }
            node = read_node(node.children[i]);
        }

        // Collect all matching values from this and subsequent leaves
        while (true) {
            for (int i = 0; i < node.n; i++) {
                if (strcmp(node.entries[i].key, key) == 0) {
                    result.push_back(node.entries[i].value);
                } else if (strcmp(node.entries[i].key, key) > 0) {
                    goto done;
                }
            }

            if (node.next == -1) break;
            node = read_node(node.next);
        }

    done:
        std::sort(result.begin(), result.end());
        return result;
    }

    void remove(const char* key, int value) {
        Entry entry(key, value);
        Node node = read_node(root_offset);

        // Navigate to leaf
        int offset = root_offset;
        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && entry < node.entries[i]) {
                i++;
            }
            offset = node.children[i];
            node = read_node(offset);
        }

        // Find and remove entry
        for (int i = 0; i < node.n; i++) {
            if (node.entries[i] == entry) {
                for (int j = i; j < node.n - 1; j++) {
                    node.entries[j] = node.entries[j + 1];
                }
                node.n--;
                write_node(offset, node);
                return;
            }
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
