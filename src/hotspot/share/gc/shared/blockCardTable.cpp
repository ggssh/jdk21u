#include "gc/shared/blockCardTable.hpp"
#include "gc/g1/g1CollectedHeap.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/shared/block_plab.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/gcLogPrecious.hpp"
#include "gc/shared/gc_globals.hpp"
#include "gc/shared/space.inline.hpp"
#include "logging/log.hpp"
#include "memory/virtualspace.hpp"
#include "precompiled.hpp"
#include "runtime/init.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"
#include "services/memTracker.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/powerOfTwo.hpp"
#include "gc/g1/g1DataStructureRegionSet.hpp"
#if INCLUDE_PARALLELGC
#include "gc/parallel/objectStartArray.hpp"
#endif

uint BlockCardTable::_card_shift = 0;
uint BlockCardTable::_card_size = 0;
uint BlockCardTable::_card_size_in_words = 0;

// Static member variable definition
G1DataStructureRegionSet* BlockCardTable::_clean_card_value_obj = nullptr;

void BlockCardTable::initialize_card_size() {
  assert(
      UseG1GC || UseParallelGC || UseSerialGC,
      "Initialize card size should only be called by card based collectors.");
  _card_size = BlockPLAB::size_in_bytes();
  // _card_size = HeapRegion::GrainBytes / 16;
  // log_error(gc) ("BlockPLAB::size(): %u", _card_size);
  guarantee(_card_size > 0, "card_size not initialized");
  _card_shift = log2i_exact(_card_size);
  _card_size_in_words = _card_size / sizeof(HeapWord);

  log_info(gc, init)("BlockCardTable::initialize_card_size: _card_size: %u Bytes, _card_shift: %u, _card_size_in_words: %u", _card_size, _card_shift, _card_size_in_words);

  // Set blockOffsetTable size based on card table entry size
  //   BOTConstants::initialize_bot_size(_card_shift);

#if INCLUDE_PARALLELGC
  // Set ObjectStartArray block size based on card table entry size
//   ObjectStartArray::initialize_block_size(_card_shift);
#endif

  log_info_p(gc, init)("BlockCardTable entry size: " UINT32_FORMAT, _card_size);

  // Initialize the clean card value object
  initialize_clean_card_value();
}

size_t BlockCardTable::compute_byte_map_size(size_t num_bytes) {
  assert(_page_size != 0, "uninitialized, check declaration order");
  const size_t granularity = os::vm_allocation_granularity();
  return align_up(num_bytes, MAX2(_page_size, granularity));
}

BlockCardTable::BlockCardTable(MemRegion whole_heap)
    : _whole_heap(whole_heap), _page_size(os::vm_page_size()),
      _byte_map_size(0), _byte_map(nullptr), _byte_map_base(nullptr) {
  assert((uintptr_t(_whole_heap.start()) & (_card_size - 1)) == 0,
         "heap must start at card boundary");
  assert((uintptr_t(_whole_heap.end()) & (_card_size - 1)) == 0,
         "heap must end at card boundary");
  log_info(gc, init)("BlockCardTable::BlockCardTable: _whole_heap: " PTR_FORMAT " - " PTR_FORMAT, p2i(_whole_heap.start()), p2i(_whole_heap.end()));
  _listener.set_card_table(this);
}

void BlockCardTable::initialize(G1RegionToSpaceMapper* mapper) {
  guarantee(_card_shift > 0, "card_shift is not initialized");
  mapper->set_mapping_changed_listener(&_listener);

  _byte_map_size = mapper->reserved().byte_size() / sizeof(CardValue);

  HeapWord* low_bound  = _whole_heap.start();
  HeapWord* high_bound = _whole_heap.end();

  _covered[0] = _whole_heap;

  _byte_map = (CardValue*) mapper->reserved().start();
  _byte_map_base = _byte_map - (uintptr_t(low_bound) >> _card_shift);
  
  assert(byte_for(low_bound) == &_byte_map[0], "Checking start of map");
  assert(byte_for(high_bound-1) <= &_byte_map[last_valid_index()], "Checking end of map");

  log_trace(gc, barrier)("BlockCardTable::BlockCardTable: ");
  log_trace(gc, barrier)("    &_byte_map[0]: " PTR_FORMAT "  &_byte_map[last_valid_index()]: " PTR_FORMAT,
                         p2i(&_byte_map[0]), p2i(&_byte_map[last_valid_index()]));
  log_info(gc, barrier)("_byte_map: " PTR_FORMAT " _byte_map_base: " PTR_FORMAT " _card_shift: %d, low_bound: " PTR_FORMAT " high_bound: " PTR_FORMAT "",  p2i(_byte_map), p2i(_byte_map_base), _card_shift, p2i(low_bound), p2i(high_bound));
  log_info(gc, barrier)("uintptr_t(low_bound): " PTR_FORMAT ", (uintptr_t(low_bound) >> _card_shift): " PTR_FORMAT, p2i(low_bound), uintptr_t(low_bound) >> _card_shift);
}

void BlockCardTableChangedListener::on_commit(uint start_idx, size_t num_regions, bool zero_filled) {
  // Default value for a clean card on the card table is -1. So we cannot take advantage of the zero_filled parameter.
  MemRegion mr(G1CollectedHeap::heap()->bottom_addr_for_region(start_idx), num_regions * HeapRegion::GrainWords);
  // log_info(gc, init)("BlockCardTable: clearing MemRegion [" PTR_FORMAT ", " PTR_FORMAT ")", 
    // p2i(mr.start()), p2i(mr.end()));
  _card_table->clear_MemRegion(mr);
}

void BlockCardTable::clear_MemRegion(MemRegion mr) {
  // Be conservative: only clean cards entirely contained within the
  // region.
  CardValue* cur;
  if (mr.start() == _whole_heap.start()) {
    cur = byte_for(mr.start());
  } else {
    assert(mr.start() > _whole_heap.start(), "mr is not covered.");
    cur = byte_after(mr.start() - 1);
  }
  CardValue* last = byte_after(mr.last());
  // memset(cur, clean_card_value(), pointer_delta(last, cur, sizeof(CardValue)));
  size_t count = 0;
  for (CardValue* c = cur; c < last; c++) {
    count++;
    mark_clean(c);
  }
  // log_info(gc)("BlockCardTable: cleared %zu cards", count);
  // for (size_t i = 0; i < _byte_map_size; i++) {
  //   log_info(gc, init)("BlockCardTable: byte_map[%zu]: " PTR_FORMAT, i, p2i(_byte_map[i]));
  // }
  // memset(cur, 0, pointer_delta(last, cur, sizeof(CardValue)));
}

void BlockCardTable::print_on(outputStream *st) const {
  st->print_cr("BlockCardTable byte_map: [" PTR_FORMAT "," PTR_FORMAT "] _byte_map_base: " PTR_FORMAT,
    p2i(_byte_map), p2i(_byte_map + _byte_map_size), p2i(_byte_map_base));
  
  // iterate over the byte_map
  for (size_t i = 0; i < _byte_map_size; i++) {
    if (_byte_map[i] != 0x0) {
      st->print_cr("[%p] BlockCardTable: byte_map[%zu]: " PTR_FORMAT, &_byte_map[i], i, p2i(_byte_map[i]));
    } else {
      // st->print_cr("BlockCardTable: byte_map[%zu]: 0x0", i);
    }
  }
}

void BlockCardTable::initialize_clean_card_value() {
  if (_clean_card_value_obj == nullptr) {
    // Create a special G1DataStructureRegionSet object that will never be used
    // This object has a special ID (UINT_MAX) to distinguish it from real data structures
    G1CollectedHeap* heap = G1CollectedHeap::heap();
    G1DataStructure* dummy_data_structure = nullptr; // nullptr for clean card value
    _clean_card_value_obj = new G1DataStructureRegionSet(heap, dummy_data_structure, UINT_MAX);
    
    // Mark this object as a special clean card value
    _clean_card_value_obj->set_alive(false);
    
    log_info(gc, init)("BlockCardTable: Initialized clean card value object at %p", _clean_card_value_obj);
  }
}

// void BlockCardTable::cleanup_clean_card_value() {
//   if (_clean_card_value_obj != nullptr) {
//     delete _clean_card_value_obj;
//     _clean_card_value_obj = nullptr;
//     log_info(gc)("BlockCardTable: Cleaned up clean card value object");
//   }
// }