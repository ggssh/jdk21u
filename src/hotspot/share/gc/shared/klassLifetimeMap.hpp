#ifndef SHARE_GC_SHARED_KLASSLIFETIMEMAP_HPP
#define SHARE_GC_SHARED_KLASSLIFETIMEMAP_HPP

#include "memory/allocation.hpp"
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
    class Config {
    public:
        static inline bool get_hash(const KlassLifetimeEntry& entry) {
            return entry.obj_symbol()->identity_hash();
        }
        static inline bool key_equals(const KlassLifetimeEntry& entry1, const KlassLifetimeEntry& entry2) {
            return entry1.obj_symbol() == entry2.obj_symbol();
        }
    };

    using LifetimeHashMap = HashMap<KlassLifetimeEntry, UIntArray, Config, mtGC>;

    LifetimeHashMap* _table;

private:
    static void print_entries(const KlassLifetimeEntry& entry, const UIntArray& value){
        log_info(gc)("RefereLifetimeHashMapnceHashMap: %s : %s",
                     entry.obj_symbol()->as_C_string(),
                     value.to_string(","));
    }

public:
    inline void add_or_inc(SymbolHandle obj_symbol, size_t age) {
        KlassLifetimeEntry entry(obj_symbol);
        UIntArray arr;
        _table->get(entry, arr);
        arr.age_histogram[age]--;
        size_t new_age = MIN2(age + 1, (size_t) 255);
        arr.age_histogram[new_age]++;
        _table->insert(entry, arr);
        // if (_table->get(entry, arr)) {
        //     // if (arr.age_histogram[age] > 0){
        //         arr.age_histogram[age]--;
        //     // }
        //     size_t new_age = MIN2(age + 1, (size_t) 255);
        //     arr.age_histogram[new_age]++;
        //     _table->insert(entry, arr);
        // } else {
        //     arr.age_histogram[age]--;
        //     size_t new_age = MIN2(age + 1, (size_t) 255);
        //     arr.age_histogram[new_age]++;
        //     _table->insert(entry, arr);
        // }
    }

    // yyz: odd obj is object whose age is lower than 15 but is located in old gen
    inline void add_odd_obj(SymbolHandle obj_symbol) {
        KlassLifetimeEntry entry(obj_symbol);
        UIntArray arr;
        _table->get(entry, arr);
        arr.age_histogram[0]--;
        arr.age_histogram[15]++;
        _table->insert(entry, arr);
    }

    void add_or_merge(const KlassLifetimeEntry& key, const UIntArray& value) const {
        UIntArray temp;
        if (_table->get(key, temp)) {
            UIntArray new_value = temp + value;
            _table->insert(key, new_value);
        } else {
            _table->insert(key, value);
        }
    }

    KlassLifetimeMap(size_t log_table_size) {
        _table = new LifetimeHashMap(log_table_size);
    }

    ~KlassLifetimeMap() {
        delete _table;
    }

    inline void for_each(void (*func)(const KlassLifetimeEntry&, const UIntArray&)) const {
        _table->forEach(func);
    }

    template <class Closure>
    inline void for_each_closure(Closure* cl) {
        _table->forEachClosure(cl);
    }

    void print_all(){
        // todo
        _table->forEach(print_entries);
    }

    inline void clear() {
        _table->clear();
    }
};

#endif