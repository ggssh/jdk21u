#ifndef SHARE_GC_SHARED_BLOCK_PLAB_HPP
#define SHARE_GC_SHARED_BLOCK_PLAB_HPP

#include "gc/g1/heapRegion.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "memory/allocation.hpp"
#include "utilities/globalDefinitions.hpp"

class BlockPLABStats;

class BlockPLABType {
  typedef enum {
    NormalTag = 0,
    StartsBlockPLABTag = 1,
    ContinuesBlockPLABTag = 2,
  } Tag;

  Tag _tag;

public:
  BlockPLABType(Tag t) : _tag(t) { assert(is_valid(_tag), "invalid block plab type: %u", (uint) (t)); }

  bool is_normal() const { return get() == NormalTag; }

  bool is_starts_block_plab() const { return get() == StartsBlockPLABTag; }

  bool is_continues_block_plab() const { return get() == ContinuesBlockPLABTag; }

  bool is_valid() const { return get() == NormalTag || get() == StartsBlockPLABTag || get() == ContinuesBlockPLABTag; }

  Tag get() const { return _tag; }

  void set_normal() {
    _tag = NormalTag;
  }

  void set_starts_block_plab() {
    _tag = StartsBlockPLABTag;
  }

  void set_continues_block_plab() {
    _tag = ContinuesBlockPLABTag;
  }

  static BlockPLABType Default() {
    return BlockPLABType(NormalTag);
  }
};

class BlockPLAB: public CHeapObj<mtGC> {
protected:
  // yizhe: in support of block_plab
  char      head[32];
  HeapWord* _bottom;
  HeapWord* _top;
  HeapWord* _end;           // Last allocatable address + 1
  HeapWord* _hard_end;      // _end + AlignmentReserve

  // In support of ergonomic sizing of PLAB's
  size_t    _allocated;     // in HeapWord units
  size_t    _wasted;        // in HeapWord units
  size_t    _undo_wasted;
  
  // yizhe: in support of block_plab
  BlockPLAB* _prev;
  BlockPLAB* _next;
  HeapRegion* _alloc_region;
  uint _idx;
  bool _full;
  BlockPLABType _type;
  // yizhe: todo per block_plab's remset
  char      tail[32];

  size_t invalidate() {
    _end    = _hard_end;
    size_t remaining = pointer_delta(_end, _top);  // Calculate remaining space.
    _top    = _end;      // Force future allocations to fail.
    _bottom = _end;      // Force future contains() queries to return false.
    return remaining;
  }

  // Fill in remaining space with a dummy object and invalidate the PLAB. Returns
  // the amount of remaining space.
  size_t retire_internal();

  void add_undo_waste(HeapWord* obj, size_t word_sz);

  // Undo the last allocation in the buffer, which is required to be of the
  // "obj" of the given "word_sz".
  void undo_last_allocation(HeapWord* obj, size_t word_sz);

public:
  static void startup_initialization();

  BlockPLAB();

  static size_t size_required_for_allocation(size_t word_size) { return word_size + CollectedHeap::lab_alignment_reserve(); }

  static size_t size();

  // If an allocation of the given "word_sz" can be satisfied within the
  // buffer, do the allocation, returning a pointer to the start of the
  // allocated block.  If the allocation request cannot be satisfied,
  // return null.
  HeapWord* allocate(size_t word_sz) {
    HeapWord* res = _top;
    if (pointer_delta(_end, _top) >= word_sz) {
      _top = _top + word_sz;
      return res;
    } else {
      return nullptr;
    }
  }

  // Undo any allocation in the buffer, which is required to be of the
  // "obj" of the given "word_sz".
  void undo_allocation(HeapWord* obj, size_t word_sz);

  // The total (word) size of the buffer, including both allocated and
  // unallocated space.
  // size_t word_sz() { return _word_sz; }

  size_t waste() { return _wasted; }
  size_t undo_waste() { return _undo_wasted; }

  // The number of words of unallocated space remaining in the buffer.
  size_t words_remaining() {
    assert(_end >= _top, "Negative buffer");
    return pointer_delta(_end, _top, HeapWordSize);
  }

  bool contains(void* addr) {
    return (void*)_bottom <= addr && addr < (void*)_hard_end;
  }

  // Sets the space of the buffer to be [buf, space+word_sz()).
  void set_buf(HeapWord* buf);

  // Flush allocation statistics into the given PLABStats supporting ergonomic
  // sizing of PLAB's and retire the current buffer. To be called at the end of
  // GC.
  void flush_and_retire_stats(BlockPLABStats* stats);

  // Fills in the unallocated portion of the buffer with a garbage object and updates
  // statistics. To be called during GC.
  void retire();

  HeapWord* top() const {
    return _top;
  }

  void set_full(bool full) {
    _full = full;
    if (full) {
      _end = _hard_end;
      _top = _hard_end;
    }
  }

  void set_top(HeapWord* top) {
    _top = top;
  }

  bool is_full() const {
    return _full;
  }

  void set_type(BlockPLABType type) {
    _type = type;
  }

  BlockPLABType get_type() const {
    return _type;
  }

  void set_starts_block_plab() {
    _type.set_starts_block_plab();
  }

  void set_continues_block_plab() {
    _type.set_continues_block_plab();
  }

  void set_normal() {
    _type.set_normal();
  }

  bool is_starts_block_plab() const {
    return _type.is_starts_block_plab();
  }

  bool is_continues_block_plab() const {
    return _type.is_continues_block_plab();
  }

  bool is_normal() const {
    return _type.is_normal();
  }
};

class BlockPLABStats : public CHeapObj<mtGC> {
protected:
  const char* _description;   // Identifying string.

  size_t _allocated;          // Total allocated
  size_t _wasted;             // of which wasted (internal fragmentation)
  size_t _undo_wasted;        // of which wasted on undo (is not used for calculation of PLAB size)
  size_t _unused;             // Unused in last buffer

  virtual void reset() {
    _allocated   = 0;
    _wasted      = 0;
    _undo_wasted = 0;
    _unused      = 0;
  }

public:
  BlockPLABStats(const char* description) :
    _description(description),
    _allocated(0),
    _wasted(0),
    _undo_wasted(0),
    _unused(0)
  { }

  virtual ~BlockPLABStats() { }

  size_t allocated() const { return _allocated; }
  size_t wasted() const { return _wasted; }
  size_t unused() const { return _unused; }
  size_t used() const { return allocated() - (wasted() + unused()); }
  size_t undo_wasted() const { return _undo_wasted; }

  static size_t size() {
    return BlockPLAB::size();
  }

  inline void add_allocated(size_t v);

  inline void add_unused(size_t v);

  inline void add_wasted(size_t v);

  inline void add_undo_wasted(size_t v);
};

#endif // SHARE_GC_SHARED_BLOCK_PLAB_HPP