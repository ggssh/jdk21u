#ifndef SHARE_UTILITIES_HASHMAP_HPP
#define SHARE_UTILITIES_HASHMAP_HPP

#include "memory/allocation.hpp"
#include "utilities/globalCounter.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/tableStatistics.hpp"

template<typename K, typename V, typename Config, MEMFLAGS F>
class HashMap : public CHeapObj<F> {
private:
    struct Node : public CHeapObj<F> {
        K key;
        V value;
        Node* next;

        Node(const K& k, const V& v, Node* n = nullptr)
            : key(k), value(v), next(n) {}
    };

    Node** table;
    size_t capacity;
    size_t size;

    inline uint hash(const K& key) const {
        // Simple hash using std::hash
        return Config::get_hash(key) % capacity;
    }

public:
    HashMap(uint cap = 16) : capacity(1 << cap), size(0) {
        table = NEW_C_HEAP_ARRAY(Node*, capacity, F);
        for (size_t i = 0; i < capacity; ++i)
            table[i] = nullptr;
    }

    ~HashMap() {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                Node* toDelete = curr;
                curr = curr->next;
                delete toDelete;
            }
        }
        FREE_C_HEAP_ARRAY(Node*, table);
    }

    inline void insert(const K& key, const V& value) {
        uint idx = hash(key);
        Node* curr = table[idx];
        while (curr) {
            if (Config::key_equals(curr->key, key)) {
                curr->value = value;
                return;
            }
            curr = curr->next;
        }
        table[idx] = new Node(key, value, table[idx]);
        size++;
    }

    inline bool get(const K& key, V& value) const {
        uint idx = hash(key);
        Node* curr = table[idx];
        while (curr) {
            if (Config::key_equals(curr->key, key)) {
                value = curr->value;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    inline bool remove(const K& key) {
        uint idx = hash(key);
        Node* curr = table[idx];
        Node* prev = nullptr;
        while (curr) {
            if (Config::key_equals(curr->key, key)) {
                if (prev)
                    prev->next = curr->next;
                else
                    table[idx] = curr->next;
                delete curr;
                size--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    inline void clear() {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr != nullptr) {
                Node* toDelete = curr;
                curr = curr->next;
                delete toDelete;
            }
            table[i] = nullptr;
        }
        size = 0;
    }

    inline size_t getSize() const {
        return size;
    }

    inline bool isEmpty() const {
        return size == 0;
    }

    inline void forEach(void (*func)(const K&, const V&)) const {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                func(curr->key, curr->value);
                curr = curr->next;
            }
        }
    }

    template <class Closure>
    inline void forEachClosure(Closure* cl) {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                cl->work(curr->key, curr->value);
                curr = curr->next;
            }
        }
    }
};

#endif // SHARE_UTILITIES_HASHMAP_HPP