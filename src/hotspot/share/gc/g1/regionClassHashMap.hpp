#ifndef SHARE_GC_SHARED_REGIONCLASSHASHMAP_HPP
#define SHARE_GC_SHARED_REGIONCLASSHASHMAP_HPP

#include "oops/symbolHandle.hpp"
#include "utilities/ostream.hpp"
#include "gc/g1/heapRegion.hpp"
#include "logging/log.hpp"
// #include "gc/g1/heapRegion.inline.hpp"

class RegionClassHashMap : CHeapObj<mtGC> {
    struct Node : public CHeapObj<mtGC> {
        HeapRegion* _region;
        SymbolHandle _obj_class;
        size_t _count;
        size_t _size;
        Node* _next;
        
        Node(HeapRegion* region, SymbolHandle obj_class, size_t v, size_t size, Node* n = nullptr)
            : _region(region), _obj_class(obj_class), _count(v), _size(size), _next(n) {}
    };

    Node** table;
    size_t capacity;
    size_t size;

private:
    inline uint hash(HeapRegion* region, Symbol* obj_class) {
        return (region->hrm_index() + obj_class->identity_hash()) % capacity;
    }

    static inline bool key_equals(Node* entry, HeapRegion* region, Symbol* obj_class) {
        return entry->_obj_class == obj_class && entry->_region == region;
    }

    static void print_entries(HeapRegion* region, SymbolHandle& obj_class, size_t& count, size_t& size) {
        log_info(gc)("Region %u %s contains %s : count %zu size %zu",
                     region->hrm_index(),
                     region->is_old_or_humongous() ? "(old)" : "(young)",
                     obj_class->as_C_string(),
                     count, size);
    }

public:

    inline void add_or_inc(HeapRegion* region, Symbol* obj_class, size_t inc = 1, size_t size = 1) {
        uint idx = hash(region, obj_class);
        Node* curr = table[idx];
        while (curr) {
            if (key_equals(curr, region, obj_class)) {
                curr->_count += inc;
                curr->_size += size;
                return;
            }
            curr = curr->_next;
        }
        table[idx] = new Node(region, obj_class, inc, size, table[idx]);
        size++;
    }

    inline size_t getSize() const {
        return size;
    }

    inline bool isEmpty() const {
        return size == 0;
    }

    inline void forEach(void (*func)(HeapRegion*, SymbolHandle&, size_t&, size_t&)) const {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                func(curr->_region, curr->_obj_class, curr->_count, curr->_size);
                curr = curr->_next;
            }
        }
    }

    template <class Closure>
    inline void forEachClosure(Closure* cl) {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            while (curr) {
                cl->work(curr->_region, curr->_obj_class, curr->_count, curr->_size);
                curr = curr->_next;
            }
        }
    }

public:
    RegionClassHashMap(uint cap = 16) : capacity((1 << cap) - 1), size(0) {
        table = NEW_C_HEAP_ARRAY(Node*, capacity, mtGC);
        for (size_t i = 0; i < capacity; ++i)
            table[i] = nullptr;
    }

    ~RegionClassHashMap() {
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

    void print_all(){
        log_info(gc)("print entries");
        forEach(print_entries);
    }

    template <class Closure>
    inline void for_each_closure(Closure* cl) {
        forEachClosure(cl);
    }

    void clear() {
        for (size_t i = 0; i < capacity; ++i) {
            Node* curr = table[i];
            table[i] = nullptr;
            while (curr) {
                Node* toDelete = curr;
                curr = curr->_next;
                delete toDelete;
            }
        }
    }
};


#endif