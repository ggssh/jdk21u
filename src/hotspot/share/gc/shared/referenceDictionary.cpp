/*
 * Copyright (c) 2003, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

 #include "precompiled.hpp"
 #include "classfile/classLoaderData.inline.hpp"
 #include "gc/shared/ReferenceDictionary.hpp"
 #include "classfile/javaClasses.hpp"
 #include "classfile/protectionDomainCache.hpp"
 #include "classfile/systemDictionary.hpp"
 #include "classfile/vmSymbols.hpp"
 #include "logging/log.hpp"
 #include "logging/logStream.hpp"
 #include "memory/iterator.hpp"
 #include "memory/metaspaceClosure.hpp"
 #include "memory/resourceArea.hpp"
 #include "memory/universe.hpp"
 #include "oops/klass.inline.hpp"
 #include "oops/method.hpp"
 #include "oops/oop.inline.hpp"
 #include "oops/oopHandle.inline.hpp"
 #include "runtime/arguments.hpp"
 #include "runtime/handles.inline.hpp"
 #include "runtime/javaCalls.hpp"
 #include "runtime/mutexLocker.hpp"
 #include "runtime/safepointVerifiers.hpp"
 #include "utilities/concurrentHashTable.inline.hpp"
 #include "utilities/growableArray.hpp"
 #include "utilities/tableStatistics.hpp"
 
 // 2^24 is max size, like StringTable.
 const size_t END_SIZE = 24;
 // If a chain gets to 100 something might be wrong
 const size_t REHASH_LEN = 100;
 
 ReferenceDictionary::ReferenceDictionary(size_t table_size)
   : _number_of_entries(0) {
 
   size_t start_size_log_2 = MAX2(ceil_log2(table_size), (size_t)2); // 2 is minimum size even though some dictionaries only have one entry
   size_t current_size = ((size_t)1) << start_size_log_2;
   _table = new ConcurrentTable(start_size_log_2, END_SIZE, REHASH_LEN);
 }
 
 ReferenceDictionary::~ReferenceDictionary() {
   // This deletes the table and all the nodes, by calling free_node in Config.
   delete _table;
 }
 
 uintx ReferenceDictionary::Config::get_hash(Value const& value, bool* is_dead) {
   return value->from_klass()->name()->identity_hash() + value->to_klass()->name()->identity_hash();
 }

 void* ReferenceDictionary::Config::allocate_node(void* context, size_t size, Value const& value) {
   return AllocateHeap(size, mtClass);
 }
 
 void ReferenceDictionary::Config::free_node(void* context, void* memory, Value const& value) {
   delete value; // Call ReferenceDictionaryEntry destructor
   FreeHeap(memory);
 }
 
 ReferenceDictionaryEntry::ReferenceDictionaryEntry(Klass* from_class, Klass* to_class)
   : _from_klass(from_class), _to_klass(to_class) {
 }
 
 ReferenceDictionaryEntry::~ReferenceDictionaryEntry() {
 }
 
 const int _resize_load_trigger = 5;       // load factor that will trigger the resize
 
 int ReferenceDictionary::table_size() const {
   return 1 << _table->get_size_log2(Thread::current());
 }
 
 bool ReferenceDictionary::check_if_needs_resize() {
   return ((_number_of_entries > (_resize_load_trigger * table_size())) &&
          !_table->is_max_size_reached());
 }
 
//  // All classes, and their class loaders, including initiating class loaders
//  void ReferenceDictionary::all_entries_do(KlassClosure* closure) {
//    auto all_doit = [&] (ReferenceDictionaryEntry** value) {
//      InstanceKlass* k = (*value)->instance_klass();
//      closure->do_klass(k);
//      return true;
//    };
 
//    _table->do_scan(Thread::current(), all_doit);
//  }
 
 
class DictionaryLookup : StackObj {
private:
  Klass* _from;
  Klass* _to;
public:
  DictionaryLookup(Klass* from, Klass* to) : _from(from), _to(to) { }
  uintx get_hash() const {
    return _from->->name()->identity_hash() + _to->name()->identity_hash();
  }
  bool equals(ReferenceDictionaryEntry** value) {
    ReferenceDictionaryEntry *entry = *value;
    return (entry->from_klass()->name() == _from->name() &&
            entry->to_klass()->name() == _to->name());
  }
  bool is_dead(ReferenceDictionaryEntry** value) {
    return false;
  }
};
 
// Add a loaded class to the ReferenceDictionary.
void ReferenceDictionary::add_klass(JavaThread* current, Klass* from, Klass* to) {
//   assert_locked_or_safepoint(SystemDictionary_lock); // doesn't matter now
//   assert(obj != nullptr, "adding nullptr obj");
//   assert(obj->name() == class_name, "sanity check on name");

  ReferenceDictionaryEntry* entry = new ReferenceDictionaryEntry(obj);
  DictionaryLookup lookup(from, to);
  bool needs_rehashing, clean_hint;
  bool created = _table->insert(current, lookup, entry, &needs_rehashing, &clean_hint);
  assert(created, "should be because we have a lock");
  assert (!needs_rehashing, "should never need rehashing");
  assert(!clean_hint, "no class should be unloaded");
  _number_of_entries++;  // still locked
  // This table can be resized while another thread is reading it.
  if (check_if_needs_resize()) {
    _table->grow(current);

    // It would be nice to have a JFR event here, add some logging.
    LogTarget(Info, class, loader, data) lt;
    if (lt.is_enabled()) {
      ResourceMark rm;
      LogStream ls(&lt);
      ls.print("ReferenceDictionary resized to %d entries %d for ", table_size(), _number_of_entries);
      loader_data()->print_value_on(&ls);
    }
  }
}

// This routine does not lock the ReferenceDictionary.
//
// Since readers don't hold a lock, we must make sure that system
// ReferenceDictionary entries are only removed at a safepoint (when only one
// thread is running), and are added to in a safe way (all links must
// be updated in an MT-safe manner).
//
// Callers should be aware that an entry could be added just after
// the table is read here, so the caller will not see the new entry.
// The entry may be accessed by the VM thread in verification.
ReferenceDictionaryEntry* ReferenceDictionary::get_entry(Thread* current,
                                       Klass* from, Klass* to) {
  DictionaryLookup lookup(from, to);
  ReferenceDictionaryEntry* result = nullptr;
  auto get = [&] (ReferenceDictionaryEntry** value) {
    // function called if value is found so is never null
    result = (*value);
  };
  bool needs_rehashing = false;
  _table->get(current, lookup, get, &needs_rehashing);
  assert (!needs_rehashing, "should never need rehashing");
  return result;
}


InstanceKlass* ReferenceDictionary::find(Thread* current, Symbol* name,
                                Handle protection_domain) {
  NoSafepointVerifier nsv;

  ReferenceDictionaryEntry* entry = get_entry(current, name);
  if (entry != nullptr && entry->is_valid_protection_domain(protection_domain)) {
    return entry->instance_klass();
  } else {
    return nullptr;
  }
}

ReferenceDictionaryEntry* ReferenceDictionary::find_entry(Thread* current,
                                      Klass* from, Klass* to) {
//   assert_locked_or_safepoint(SystemDictionary_lock);
  ReferenceDictionaryEntry* entry = get_entry(current, name);
  return entry;
}


