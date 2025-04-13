#ifndef SHARE_GC_SHARED_REFERENCECOUNTERDICTIONARY_HPP
#define SHARE_GC_SHARED_REFERENCECOUNTERDICTIONARY_HPP

#include "oops/instanceKlass.hpp"
#include "oops/oop.hpp"
#include "oops/oopHandle.hpp"
#include "utilities/concurrentHashTable.hpp"
#include "utilities/ostream.hpp"
 
 class ReferenceDictionaryEntry;
 class ProtectionDomainEntry;
 template <typename T> class GrowableArray;
 
 //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 // The data structure for the class loader data dictionaries.
 
class ReferenceDictionaryEntry;
 
class ReferenceDictionary : public CHeapObj<mtClass> {
   int _number_of_entries;
 
   class Config {
    public:
     using Value = ReferenceDictionaryEntry*;
     static uintx get_hash(Value const& value, bool* is_dead);
     static void* allocate_node(void* context, size_t size, Value const& value);
     static void free_node(void* context, void* memory, Value const& value);
   };
 
   using ConcurrentTable = ConcurrentHashTable<Config, mtClass>;
   ConcurrentTable* _table;
 
 
   ReferenceDictionaryEntry* get_entry(Thread* current, Klass* from, Klass* to);
   bool check_if_needs_resize();
   int table_size() const;
 
 public:
   ReferenceDictionary(size_t table_size);
   ~ReferenceDictionary();
 
   void add_klass(JavaThread* current, Klass* from, Klass* to);
 
   ReferenceDictionaryEntry* find_entry(Thread* current, Klass* from, Klass* to);
 
  //  void all_entries_do(KlassClosure* closure);
 
};
 
 // An entry in the class loader data dictionaries, this describes a class as
 // { InstanceKlass*, protection_domain_set }.
 
class ReferenceDictionaryEntry : public CHeapObj<mtClass> {
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
   Klass* _from_klass;
   Klass* _to_klass;
 
  public:
   ReferenceDictionaryEntry(Klass* from_klass, Klass* to_klass);
   ~ReferenceDictionaryEntry();


   Klass* from_klass() const { return _from_klass; }
   Klass* to_klass() const { return _to_klass; }

};

 

#endif // SHARE_GC_SHARED_REFERENCECOUNTERDICTIONARY_HPP
