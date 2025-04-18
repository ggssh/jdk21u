#ifndef SHARE_GC_SHARED_REFERENCEHASHMAP_HPP
#define SHARE_GC_SHARED_REFERENCEHASHMAP_HPP

#include "oops/symbolHandle.hpp"
#include "utilities/hashMap.hpp"
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

    class Config {
    public:
        static inline bool get_hash(const ReferenceEntry& entry) {
            return entry.from_symbol()->identity_hash() + entry.to_symbol()->identity_hash();
        }
        static inline bool key_equals(const ReferenceEntry& entry1, const ReferenceEntry& entry2) {
            return entry1.from_symbol() == entry2.from_symbol() &&
                   entry1.to_symbol() == entry2.to_symbol();
        }
    };

    using RefHashMap = HashMap<ReferenceEntry, size_t, Config, mtGC>;

    RefHashMap* _table;

private:
    static void print_entries(const ReferenceEntry& entry, const size_t& value){
        log_info(gc)("ReferenceHashMap: %s -> %s : %zu",
                     entry.from_symbol()->as_C_string(),
                     entry.to_symbol()->as_C_string(),
                     value);
    }

public:
    ReferenceHashMap(size_t log_table_size) {
        _table = new RefHashMap(log_table_size);
    }

    ~ReferenceHashMap() {
        delete _table;
    }

    inline void add_or_inc(SymbolHandle from, SymbolHandle to, size_t inc = 1) {
        ReferenceEntry entry(from, to);
        // size_t value = 1;
        size_t * old_value_ptr = nullptr;
        _table->get_ptr_or_insert(entry, inc, old_value_ptr);
        if(old_value_ptr != nullptr) {
            (*old_value_ptr) += inc;
        }
        // if(_table->get(entry, value)) {
        //     _table->insert(entry, value + inc);
        // } else {
        //     _table->insert(entry, inc);
        //     // ResourceMark rm;
        //     // log_info(gc)("add ReferenceHashMap: %s -> %s : %zu",
        //     //     entry.from_symbol()->as_C_string(),
        //     //     entry.to_symbol()->as_C_string(),
        //     //     value);
        // }
    }

    void print_all(){
        log_info(gc)("print entries");
        _table->forEach(print_entries);
    }

    inline void for_each(void (*func)(const ReferenceEntry&, const size_t&)) const {
        _table->forEach(func);
    }

    template <class Closure>
    inline void for_each_closure(Closure* cl) {
        _table->forEachClosure(cl);
    }
};


#endif