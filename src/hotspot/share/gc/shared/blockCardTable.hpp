#ifndef SHARE_GC_SHARED_BLOCKCARDTABLE_HPP
#define SHARE_GC_SHARED_BLOCKCARDTABLE_HPP

#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "gc/g1/g1RegionToSpaceMapper.hpp"
#include "gc/g1/heapRegion.hpp"
#include "logging/log.hpp"
#include "memory/allocation.hpp"
#include "memory/memRegion.hpp"
#include "memory/virtualspace.hpp"
#include "oops/oopsHierarchy.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include <cstdint>

class BlockCardTable;

class BlockCardTableChangedListener : public G1MappingChangedListener {
private:
  BlockCardTable *_card_table;

public:
  BlockCardTableChangedListener() : _card_table(nullptr) {}

  void set_card_table(BlockCardTable *card_table) { _card_table = card_table; }

  void on_commit(uint start_idx, size_t num_regions, bool zero_filled) override;
};

class BlockCardTable : public CHeapObj<mtGC> {
  friend class VMStructs;
  friend class BlockCardTableChangedListener;

  BlockCardTableChangedListener _listener;

public:
  // typedef uint64_t CardValue;
  // yizhe: each card takes sizeof(G1DataStructureRegionSet*) bytes
  typedef G1DataStructureRegionSet *CardValue;
  // yizhe: "byte" is the unit of the card table, in BlockCardTable it should be the same as the size of "CardValue"

  STATIC_ASSERT(sizeof(CardValue) == sizeof(G1DataStructureRegionSet *));

protected:
  const MemRegion _whole_heap; // the region covered by the card table
  const size_t _page_size;     // page size used when mapping _byte_map
  size_t _byte_map_size;       // in bytes
  CardValue *_byte_map;        // the card marking array
  CardValue *_byte_map_base;

  // yizhe: the value of the card table entry is the the data_structure address
  // of the block
  static uint _card_shift;
  static uint _card_size;
  static uint _card_size_in_words;

  // Some barrier sets create tables whose elements correspond to parts of
  // the heap; the CardTableBarrierSet is an example.  Such barrier sets will
  // normally reserve space for such tables, and commit parts of the table
  // "covering" parts of the heap that are committed. At most one covered
  // region per generation is needed.
  static constexpr int max_covered_regions = 2;

  // The covered regions should be in address order.
  MemRegion _covered[max_covered_regions];

public:
  STATIC_ASSERT(BitsPerByte == 8);

  BlockCardTable(MemRegion whole_heap);
  ~BlockCardTable() = default;

  static CardValue clean_card_value() { return nullptr; }

  void initialize(G1RegionToSpaceMapper *mapper);

  inline size_t cards_required(size_t covered_words) const {
    assert(is_aligned(covered_words, _card_size_in_words), "precondition");
    return covered_words / _card_size_in_words;
  }

  inline size_t last_valid_index() const {
    return cards_required(_whole_heap.word_size()) - 1;
  }

  inline size_t compute_byte_map_size(size_t num_bytes);

  bool is_card_aligned(HeapWord *p) {
    CardValue *pcard = byte_for(p);
    return (addr_for(pcard) == p);
  }

  HeapWord *addr_for(const CardValue *p) const {
    assert(p >= _byte_map && p < _byte_map + _byte_map_size,
           "out of bounds access to card marking array. p: " PTR_FORMAT
           " _byte_map: " PTR_FORMAT " _byte_map + _byte_map_size: " PTR_FORMAT,
           p2i(p), p2i(_byte_map), p2i(_byte_map + _byte_map_size));
    // As _byte_map_base may be "negative" (the card table has been allocated
    // before the heap in memory), do not use pointer_delta() to avoid the
    // assertion failure.
    size_t delta = p - _byte_map_base;
    HeapWord *result = (HeapWord *)(delta << _card_shift);
    assert(_whole_heap.contains(result),
           "Returning result = " PTR_FORMAT " out of bounds of "
           " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT
           ")",
           p2i(result), p2i(_whole_heap.start()), p2i(_whole_heap.end()));
    return result;
  }

  static size_t compute_size(size_t mem_region_size_in_words) {
    guarantee(_card_size_in_words > 0, "card_size_in_words not initialized");
    size_t number_of_slots = (mem_region_size_in_words / _card_size_in_words);
    // log_error(gc) ("mem_region_size_in_words: %zu", mem_region_size_in_words);
    // log_error(gc) ("_card_size_in_words: %u", _card_size_in_words);
    // log_error(gc) ("number_of_slots: %zu", number_of_slots);
    // log_error(gc) ("sizeof(CardValue): %zu", sizeof(CardValue));
    // log_error(gc) ("heap_map_factor: %zu", heap_map_factor());
    size_t total_bytes = number_of_slots * sizeof(CardValue);
    return ReservedSpace::allocation_align_size_up(total_bytes);
  }

  // Mapping from address to card marking array entry
  CardValue *byte_for(const void *p) const {
    assert(_whole_heap.contains(p),
           "Attempt to access p = " PTR_FORMAT " out of bounds of "
           " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT
           ")",
           p2i(p), p2i(_whole_heap.start()), p2i(_whole_heap.end()));
    CardValue *result = &_byte_map_base[uintptr_t(p) >> _card_shift];
    assert(result >= _byte_map && result < _byte_map + _byte_map_size,
           "out of bounds accessor for card marking array");
    return result;
  }

  // The card table byte one after the card marking array
  // entry for argument address. Typically used for higher bounds
  // for loops iterating through the card table.
  CardValue *byte_after(const void *p) const { return byte_for(p) + 1; }

  // Provide read-only access to the card table array.
  const CardValue *byte_for_const(const void *p) const { return byte_for(p); }

  const CardValue *byte_after_const(const void *p) const {
    return byte_after(p);
  }

  // Mapping from address to card marking array index.
  size_t index_for(void *p) {
    assert(_whole_heap.contains(p),
           "Attempt to access p = " PTR_FORMAT " out of bounds of "
           " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT
           ")",
           p2i(p), p2i(_whole_heap.start()), p2i(_whole_heap.end()));
    return byte_for(p) - _byte_map;
  }

  size_t index_for_cardvalue(CardValue const *p) const {
    return pointer_delta(p, _byte_map, sizeof(CardValue));
  }

  CardValue *byte_for_index(const size_t card_index) const {
    return _byte_map + card_index;
  }

  static uint card_shift() { return _card_shift; }

  static uint card_size() { return _card_size; }

  static uint card_size_in_words() { return _card_size_in_words; }

  // Initialize card size
  static void initialize_card_size();

  // Card marking array base (adjusted for heap low boundary)
  // This would be the 0th element of _byte_map, if the heap started at 0x0.
  // But since the heap starts at some higher address, this points to somewhere
  // before the beginning of the actual _byte_map.
  CardValue *byte_map_base() const { return _byte_map_base; }

  // Returns how many bytes of the heap a single byte of the Card Table
  // corresponds to.
  static size_t heap_map_factor() { 
    // log_error(gc) ("heap_map_factor: _card_size: %u, sizeof(CardValue): %zu", _card_size, sizeof(CardValue));
    // log_error(gc) ("region_size: %zu", HeapRegion::GrainBytes);
    return _card_size / sizeof(CardValue); 
  }

  inline bool mark_clean(CardValue *card) {
    CardValue value = *card;
    if (value != clean_card_value()) {
      *card = clean_card_value();
      return true;
    }
    return false;
  }

  inline bool mark_data_structure(CardValue *card, G1DataStructureRegionSet *data_structure) {
    CardValue value = *card;
    if (value != data_structure) {
      *card = data_structure;
      return true;
    }
    return false;
  }

  // Clear (to clean_card) the bytes entirely contained within "mr" (not
  // all of which must be covered.)
  void clear_MemRegion(MemRegion mr);

  // Print a description of the memory for the card table
  void print_on(outputStream *st) const;
};

#endif // SHARE_GC_SHARED_BLOCKCARDTABLE_HPP