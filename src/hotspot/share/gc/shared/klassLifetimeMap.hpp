#ifndef SHARE_GC_SHARED_KLASSLIFETIMEMAP_HPP
#define SHARE_GC_SHARED_KLASSLIFETIMEMAP_HPP

#include "memory/allocation.hpp"
#include "oops/symbol.hpp"
#include "oops/symbolHandle.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/hashMap.hpp"
#include "logging/log.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

class KlassLifetimeEntry : public CHeapObj<mtGC> {
private:
    SymbolHandle _obj_symbol;

public:
    KlassLifetimeEntry(SymbolHandle obj_symbol) : _obj_symbol(obj_symbol) {};
    ~KlassLifetimeEntry() {};

    inline SymbolHandle obj_symbol() const { return _obj_symbol; }
};

class UIntArray  : public CHeapObj<mtGC> {
public:
    long* age_histogram;

    UIntArray() {
        age_histogram = NEW_C_HEAP_ARRAY(long, 256, mtGC);
        memset(age_histogram, 0, sizeof(long) * 256);
    }

    ~UIntArray() {
        FREE_C_HEAP_ARRAY(long, age_histogram);
    }

    UIntArray(const UIntArray& other) {
        age_histogram = NEW_C_HEAP_ARRAY(long, 256, mtGC);
        memcpy(age_histogram, other.age_histogram, sizeof(long) * 256);
    }

    UIntArray& operator=(const UIntArray& other) {
        if (this != &other) {
            memcpy(age_histogram, other.age_histogram, sizeof(long) * 256);
        }
        return *this;
    }

    char* to_string(const char* delimiter) const {
        int total_size = 1;
        for (int i = 0; i < 256; i++) {
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%ld", ABS(age_histogram[i]));
            total_size += strlen(buffer);
            if (i < 255) total_size += strlen(delimiter);
        }

        char* str = NEW_RESOURCE_ARRAY(char, total_size);
        if (str == nullptr) return nullptr;

        str[0] = '\0';

        for (int i = 0; i < 256; i++) {
            char buffer[20];
            snprintf(buffer, sizeof(buffer), "%ld", ABS(age_histogram[i]));
            strcat(str, buffer);
            if (i < 255) strcat(str, delimiter);
        }

        return str;
    }

    // used to merge par_scan_thread_state's klasslifetimemap
    UIntArray operator+(const UIntArray& other) const {
        UIntArray result;
        for (int i = 0; i < 256; i++) {
            result.age_histogram[i] = this->age_histogram[i] + other.age_histogram[i];
        }
        return result;
    }
};

class KlassLifetimeMap : public CHeapObj<mtGC> {
    struct Node : public CHeapObj<mtGC> {
        SymbolHandle _obj_symbol;
        UIntArray _array;
        Node* _next;

        Node(SymbolHandle obj_symbol, UIntArray array, Node* n = nullptr)
            : _obj_symbol(obj_symbol), _array(array), _next(n) {}
    };
    // class Config {
    // public:
    //     static inline bool get_hash(const KlassLifetimeEntry& entry) {
    //         return entry.obj_symbol()->identity_hash();
    //     }
    //     static inline bool key_equals(const KlassLifetimeEntry& entry1, const KlassLifetimeEntry& entry2) {
    //         return entry1.obj_symbol() == entry2.obj_symbol();
    //     }
    // };

    // using LifetimeHashMap = HashMap<KlassLifetimeEntry, UIntArray, Config, mtGC>;

    // LifetimeHashMap* _table;
    Node** _table;
    size_t _capacity;
    size_t _size;

private:
    inline uint hash(Symbol* obj_symbol) {
        return obj_symbol->identity_hash() % _capacity;
    }
    
    static inline bool key_equals(Node* entry, Symbol* obj_symbol) {
        return entry->_obj_symbol == obj_symbol;
    }

    // static void print_entries(const KlassLifetimeEntry& entry, const UIntArray& value){
    //     log_info(gc)("RefereLifetimeHashMapnceHashMap: %s : %s",
    //                  entry.obj_symbol()->as_C_string(),
    //                  value.to_string(","));
    // }

    static void print_entries(SymbolHandle& entry, UIntArray& value){
        log_info(gc)("LifetimeHashMapnceHashMap: %s : %s",
                     entry->as_C_string(),
                     value.to_string(","));
    }

public:
    // inline void add_or_inc(SymbolHandle obj_symbol, size_t age) {
    //     KlassLifetimeEntry entry(obj_symbol);
    //     UIntArray arr;
    //     _table->get(entry, arr);
    //     arr.age_histogram[age]--;
    //     size_t new_age = MIN2(age + 1, (size_t) 255);
    //     arr.age_histogram[new_age]++;
    //     _table->insert(entry, arr);
    //     // if (_table->get(entry, arr)) {
    //     //     // if (arr.age_histogram[age] > 0){
    //     //         arr.age_histogram[age]--;
    //     //     // }
    //     //     size_t new_age = MIN2(age + 1, (size_t) 255);
    //     //     arr.age_histogram[new_age]++;
    //     //     _table->insert(entry, arr);
    //     // } else {
    //     //     arr.age_histogram[age]--;
    //     //     size_t new_age = MIN2(age + 1, (size_t) 255);
    //     //     arr.age_histogram[new_age]++;
    //     //     _table->insert(entry, arr);
    //     // }
    // }
    inline void add_or_inc(Symbol* obj_symbol, size_t age, size_t count = 1) {
        // KlassLifetimeEntry entry(obj_symbol);
        uint idx = hash(obj_symbol);
        Node* curr = _table[idx];
        
        while(curr) {
            if (key_equals(curr, obj_symbol)) {
                curr->_array.age_histogram[age] -= count;
                size_t new_age = MIN2(age + 1, (size_t) 255);
                curr->_array.age_histogram[new_age] += count;
                return;
            }
            curr = curr->_next;
        }
        UIntArray arr;
        arr.age_histogram[age] -= count;
        size_t new_age = MIN2(age + 1, (size_t) 255);
        arr.age_histogram[new_age] += count;
        _table[idx] = new Node(SymbolHandle(obj_symbol), arr, _table[idx]);
        _size++;
    }

    inline size_t get_size() const {
        return _size;
    }

    inline bool is_empty() const {
        return _size == 0;
    }

    // yyz: odd obj is object whose age is lower than 15 but is located in old gen
    // inline void add_odd_obj(SymbolHandle obj_symbol) {
    //     KlassLifetimeEntry entry(obj_symbol);
    //     UIntArray arr;
    //     _table->get(entry, arr);
    //     arr.age_histogram[0]--;
    //     arr.age_histogram[15]++;
    //     _table->insert(entry, arr);
    // }

    // void add_or_merge(const KlassLifetimeEntry& key, const UIntArray& value) const {
    //     UIntArray temp;
    //     if (_table->get(key, temp)) {
    //         UIntArray new_value = temp + value;
    //         _table->insert(key, new_value);
    //     } else {
    //         _table->insert(key, value);
    //     }
    // }

    void add_or_merge(Symbol* obj_symbol, UIntArray& value) {
        // UIntArray temp;
        // if (_table->get(key, temp)) {
        //     UIntArray new_value = temp + value;
        //     _table->insert(key, new_value);
        // } else {
        //     _table->insert(key, value);
        // }
        uint idx = hash(obj_symbol);
        Node* curr = _table[idx];
        while (curr) {
            if (key_equals(curr, obj_symbol)) {
                curr->_array = curr->_array + value;
                return;
            }
            curr = curr->_next;
        }
        _table[idx] = new Node(SymbolHandle(obj_symbol), value, _table[idx]);
        _size++;
    }

    KlassLifetimeMap(uint cap = 16) : _capacity((1 << cap) - 1), _size(0) {
        // _table = new LifetimeHashMap(log_table_size);
        _table = NEW_C_HEAP_ARRAY(Node*, _capacity, mtGC);
        for (size_t i = 0; i < _capacity; i++) {
            _table[i] = nullptr;
        }
    }

    ~KlassLifetimeMap() {
        // delete _table;
        for (size_t i = 0; i < _capacity; i++) {
            Node* curr = _table[i];
            while (curr) {
                Node* to_delete = curr;
                curr = curr->_next;
                delete to_delete;
            }
        }
        FREE_C_HEAP_ARRAY(Node*, _table);
    }

    // inline void for_each(void (*func)(const KlassLifetimeEntry&, const UIntArray&)) const {
    //     _table->forEach(func);
    // }

    inline void forEach(void (*func)(SymbolHandle&, UIntArray&)) const {
        for (size_t i = 0; i < _capacity; i++) {
            Node* curr = _table[i];
            while (curr) {
                func(curr->_obj_symbol, curr->_array);
                curr = curr->_next;
            }
        }
    }

    template <class Closure>
    inline void forEachClosure(Closure* cl) {
        for (size_t i = 0; i < _capacity; ++i) {
            Node* curr = _table[i];
            while (curr) {
                cl->work(curr->_obj_symbol, curr->_array);
                curr = curr->_next;
            }
        }
    }

    template <class Closure>
    inline void for_each_closure(Closure* cl) {
        forEachClosure(cl);
    }

    void print_all(){
        // todo
        // _table->forEach(print_entries);
        forEach(print_entries);
    }

    inline void clear() {
        // _table->clear();
        for (size_t i = 0; i < _capacity; i++) {
            Node* curr = _table[i];
            while (curr) {
                Node* toDelete = curr;
                curr = curr->_next;
                delete toDelete;
            }
            _table[i] = nullptr;
        }
        _size = 0;
    }
};

#endif