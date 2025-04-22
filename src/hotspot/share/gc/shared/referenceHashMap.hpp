#ifndef SHARE_GC_SHARED_REFERENCEHASHMAP_HPP
#define SHARE_GC_SHARED_REFERENCEHASHMAP_HPP

#include "oops/symbolHandle.hpp"
#include "utilities/ostream.hpp"


class ReferenceEntry : public CHeapObj<mtGC> {
private:
    // Contains the set of approved protection domains that can access
    // this dictionary entry.
    //
    // [Note that C.protection_domain(), which is stored in the java.lang.Class
    // mirror of C, is NOT the same as PD]
    //
    // If an entry for PD exists in the list, it means that
    // it is okay for a caller class to reference the class in this dictionary entry.
    //
    // The usage of the PD set can be seen in SystemDictionary::validate_protection_domain()
    // It is essentially a cache to avoid repeated Java up-calls to
    // ClassLoader.checkPackageAccess().
    //
    SymbolHandle _from_symbol;
    SymbolHandle _to_symbol;

public:
    ReferenceEntry(SymbolHandle from_symbol, SymbolHandle to_symbol) {
        _from_symbol = from_symbol;
        _to_symbol = to_symbol;
    };
    ~ReferenceEntry() {};


    inline SymbolHandle from_symbol() const { return _from_symbol; }
    inline SymbolHandle to_symbol() const { return _to_symbol; }

};

class ReferenceHashMap : CHeapObj<mtGC> {
    struct Node : public CHeapObj<mtGC> {
        SymbolHandle _from;
        SymbolHandle _to;
        size_t _count;
        size_t _size;
        Node* _next;
        
        Node(SymbolHandle from, SymbolHandle to, size_t v, size_t size, Node* n = nullptr)
            : _from(from), _to(to), _count(v), _size(size), _next(n) {}
        // Node(SymbolHandle from, SymbolHandle to, size_t v, Node* n = nullptr)
        //     : _from(from), _to(to), _count(v), _next(n) {}
    };

    Node** table;
    size_t capacity;
    size_t size;

    // static inline bool get_hash(const Node* entry) {
    //     return entry->from_symbol()->identity_hash() + entry->to_symbol()->identity_hash();
    // }

    // static inline bool key_equals(const Node* entry1, const Node* entry2) {
    //     return entry1->from_symbol() == entry2->from_symbol() &&
    //            entry1->to_symbol() == entry2->to_symbol();
    // }

private:
    inline uint hash(Symbol* from, Symbol* to) {
        return (from->identity_hash() + to->identity_hash()) % capacity;
    }

    static inline bool key_equals(Node* entry, Symbol* from, Symbol* to) {
        return entry->_from == from && entry->_to == to;
    }

    static void print_entries(SymbolHandle& from, SymbolHandle& to, size_t& count, size_t& size) {
        log_info(gc)("ReferenceHashMap: %s -> %s : count %zu size %zu",
                     from->as_C_string(),
                     to->as_C_string(),
                     count, size);
    }

public:

    // inline bool get(const K& key, V& count) const {
    //     uint idx = hash(key);
    //     Node* curr = table[idx];
    //     while (curr) {
    //         if (Config::key_equals(curr->key, key)) {
    //             count = curr->count;
    //             return true;
    //         }
    //         curr = curr->next;
    //     }
    //     return false;
    // }

    inline void add_or_inc(Symbol* from, Symbol* to, size_t inc = 1, size_t size = 1) {
        uint idx = hash(from, to);
        Node* curr = table[idx];
        while (curr) {
            if (key_equals(curr, from, to)) {
                curr->_count += inc;
                curr->_size += size;
                return;
            }
            curr = curr->_next;
        }
        table[idx] = new Node(SymbolHandle(from), SymbolHandle(to), inc, size, table[idx]);
        size++;
    }

    // inline bool remove(const K& key) {
    //     uint idx = hash(key);
    //     Node* curr = table[idx];
    //     Node* prev = nullptr;
    //     while (curr) {
    //         if (Config::key_equals(curr->key, key)) {
    //             if (prev)
    //                 prev->next = curr->next;
    //             else
    //                 table[idx] = curr->next;
    //             delete curr;
    //             size--;
    //             return true;
    //         }
    //         prev = curr;
    //         curr = curr->next;
    //     }
    //     return false;
    // }

    inline size_t getSize() const {
        return size;
    }

    inline bool isEmpty() const {
        return size == 0;
    }

    inline void forEach(void (*func)(SymbolHandle&, SymbolHandle&, size_t&, size_t&)) const {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                func(curr->_from, curr->_to, curr->_count, curr->_size);
                curr = curr->_next;
            }
        }
    }

    template <class Closure>
    inline void forEachClosure(Closure* cl) {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                cl->work(curr->_from, curr->_to, curr->_count, curr->_size);
                curr = curr->_next;
            }
        }
    }

public:
    ReferenceHashMap(uint cap = 16) : capacity((1 << cap) - 1), size(0) {
        table = NEW_C_HEAP_ARRAY(Node*, capacity, mtGC);
        for (size_t i = 0; i < capacity; ++i)
            table[i] = nullptr;
    }

    ~ReferenceHashMap() {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                Node* toDelete = curr;
                curr = curr->_next;
                delete toDelete;
            }
        }
        FREE_C_HEAP_ARRAY(Node*, table);
    }

    // inline void add_or_inc(SymbolHandle from, SymbolHandle to, size_t inc = 1) {
    //     ReferenceEntry entry(from, to);
    //     // size_t count = 1;
    //     size_t * old_count_ptr = nullptr;
    //     _table->get_ptr_or_insert(entry, inc, old_count_ptr);
    //     if(old_count_ptr != nullptr) {
    //         (*old_count_ptr) += inc;
    //     }
    //     // if(_table->get(entry, count)) {
    //     //     _table->insert(entry, count + inc);
    //     // } else {
    //     //     _table->insert(entry, inc);
    //     //     // ResourceMark rm;
    //     //     // log_info(gc)("add ReferenceHashMap: %s -> %s : %zu",
    //     //     //     entry.from_symbol()->as_C_string(),
    //     //     //     entry.to_symbol()->as_C_string(),
    //     //     //     count);
    //     // }
    // }

    void print_all(){
        log_info(gc)("print entries");
        forEach(print_entries);
    }

    // inline void for_each(void (*func)(const ReferenceEntry&, const size_t&)) const {
    //     _table->forEach(func);
    // }

    template <class Closure>
    inline void for_each_closure(Closure* cl) {
        forEachClosure(cl);
    }
};


#endif