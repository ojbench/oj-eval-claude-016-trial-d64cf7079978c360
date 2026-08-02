#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

constexpr int MAX_KEY_LEN = 65;
constexpr int ORDER = 80; // B+ tree order

struct Pair {
    char key[MAX_KEY_LEN];
    int val;

    Pair() { memset(key, 0, MAX_KEY_LEN); val = 0; }
    Pair(const char* k, int v) : val(v) {
        strncpy(key, k, MAX_KEY_LEN - 1);
        key[MAX_KEY_LEN - 1] = '\0';
    }

    int cmpKey(const char* k) const {
        return strcmp(key, k);
    }

    bool operator<(const Pair& o) const {
        int c = strcmp(key, o.key);
        return c < 0 || (c == 0 && val < o.val);
    }

    bool operator==(const Pair& o) const {
        return strcmp(key, o.key) == 0 && val == o.val;
    }
};

struct BPNode {
    bool leaf;
    int cnt;
    Pair data[ORDER + 5];
    int ch[ORDER + 5];
    int nxt;

    BPNode() : leaf(true), cnt(0), nxt(-1) {
        memset(ch, -1, sizeof(ch));
    }
};

class BPTree {
    std::fstream f;
    std::string fname;
    int root, freep;

    void writeInt(int pos, int val) {
        f.seekp(pos);
        f.write((char*)&val, sizeof(int));
    }

    int readInt(int pos) {
        f.seekg(pos);
        int val;
        f.read((char*)&val, sizeof(int));
        return val;
    }

    void writeNode(int pos, const BPNode& nd) {
        f.seekp(pos);
        f.write((char*)&nd, sizeof(BPNode));
    }

    BPNode readNode(int pos) {
        f.seekg(pos);
        BPNode nd;
        f.read((char*)&nd, sizeof(BPNode));
        return nd;
    }

    int allocNode() {
        int p = freep;
        freep += sizeof(BPNode);
        writeInt(4, freep);
        return p;
    }

    void saveRoot() {
        writeInt(0, root);
    }

    int findPos(const BPNode& nd, const Pair& p) {
        int l = 0, r = nd.cnt;
        while (l < r) {
            int m = (l + r) / 2;
            if (nd.data[m] < p) l = m + 1;
            else r = m;
        }
        return l;
    }

    int findPosKey(const BPNode& nd, const char* key) {
        int l = 0, r = nd.cnt;
        while (l < r) {
            int m = (l + r) / 2;
            if (nd.data[m].cmpKey(key) < 0) l = m + 1;
            else r = m;
        }
        return l;
    }

    void split(int par, int idx) {
        BPNode p = readNode(par);
        BPNode full = readNode(p.ch[idx]);
        int mid = full.cnt / 2;

        BPNode right;
        right.leaf = full.leaf;
        right.cnt = full.cnt - mid;
        for (int i = 0; i < right.cnt; i++) {
            right.data[i] = full.data[mid + i];
        }

        if (!full.leaf) {
            for (int i = 0; i <= right.cnt; i++) {
                right.ch[i] = full.ch[mid + i];
            }
        } else {
            right.nxt = full.nxt;
        }

        full.cnt = mid;
        int rightp = allocNode();

        if (full.leaf) {
            full.nxt = rightp;
        }

        writeNode(p.ch[idx], full);
        writeNode(rightp, right);

        for (int i = p.cnt; i > idx; i--) {
            p.data[i] = p.data[i - 1];
            p.ch[i + 1] = p.ch[i];
        }

        p.data[idx] = right.data[0];
        p.ch[idx + 1] = rightp;
        p.cnt++;

        writeNode(par, p);
    }

    void insertNonFull(int pos, const Pair& p) {
        BPNode nd = readNode(pos);

        if (nd.leaf) {
            // Check duplicate
            for (int i = 0; i < nd.cnt; i++) {
                if (nd.data[i] == p) return;
            }

            int idx = findPos(nd, p);
            for (int i = nd.cnt; i > idx; i--) {
                nd.data[i] = nd.data[i - 1];
            }
            nd.data[idx] = p;
            nd.cnt++;
            writeNode(pos, nd);
        } else {
            int idx = findPos(nd, p);
            BPNode ch = readNode(nd.ch[idx]);
            if (ch.cnt >= ORDER) {
                split(pos, idx);
                nd = readNode(pos);
                if (!(p < nd.data[idx])) {
                    idx++;
                }
            }
            insertNonFull(nd.ch[idx], p);
        }
    }

public:
    BPTree(const std::string& fn) : fname(fn) {
        std::ifstream test(fname);
        bool exists = test.good();
        test.close();

        if (exists) {
            f.open(fname, std::ios::in | std::ios::out | std::ios::binary);
            root = readInt(0);
            freep = readInt(4);
        } else {
            f.open(fname, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
            root = 8;
            freep = 8 + sizeof(BPNode);
            writeInt(0, root);
            writeInt(4, freep);
            BPNode rt;
            writeNode(root, rt);
        }
    }

    ~BPTree() {
        if (f.is_open()) {
            f.flush();
            f.close();
        }
    }

    void insert(const char* key, int val) {
        Pair p(key, val);
        BPNode rt = readNode(root);

        if (rt.cnt >= ORDER) {
            BPNode newrt;
            newrt.leaf = false;
            newrt.cnt = 0;
            newrt.ch[0] = root;

            int newrp = allocNode();
            writeNode(newrp, newrt);
            split(newrp, 0);
            root = newrp;
            saveRoot();
        }

        insertNonFull(root, p);
    }

    std::vector<int> find(const char* key) {
        std::vector<int> res;
        BPNode nd = readNode(root);

        while (!nd.leaf) {
            int idx = findPosKey(nd, key);
            nd = readNode(nd.ch[idx]);
        }

        while (true) {
            for (int i = 0; i < nd.cnt; i++) {
                int c = nd.data[i].cmpKey(key);
                if (c == 0) {
                    res.push_back(nd.data[i].val);
                } else if (c > 0) {
                    goto done;
                }
            }
            if (nd.nxt == -1) break;
            nd = readNode(nd.nxt);
        }

    done:
        std::sort(res.begin(), res.end());
        return res;
    }

    void remove(const char* key, int val) {
        Pair p(key, val);
        BPNode nd = readNode(root);
        int pos = root;

        while (!nd.leaf) {
            int idx = findPos(nd, p);
            pos = nd.ch[idx];
            nd = readNode(pos);
        }

        for (int i = 0; i < nd.cnt; i++) {
            if (nd.data[i] == p) {
                for (int j = i; j < nd.cnt - 1; j++) {
                    nd.data[j] = nd.data[j + 1];
                }
                nd.cnt--;
                writeNode(pos, nd);
                return;
            }
        }
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    BPTree tree("database.dat");

    int n;
    std::cin >> n;

    for (int i = 0; i < n; i++) {
        std::string cmd;
        std::cin >> cmd;

        if (cmd == "insert") {
            std::string key;
            int val;
            std::cin >> key >> val;
            tree.insert(key.c_str(), val);
        } else if (cmd == "find") {
            std::string key;
            std::cin >> key;
            auto res = tree.find(key.c_str());
            if (res.empty()) {
                std::cout << "null\n";
            } else {
                for (size_t j = 0; j < res.size(); j++) {
                    if (j) std::cout << " ";
                    std::cout << res[j];
                }
                std::cout << "\n";
            }
        } else {
            std::string key;
            int val;
            std::cin >> key >> val;
            tree.remove(key.c_str(), val);
        }
    }

    return 0;
}
