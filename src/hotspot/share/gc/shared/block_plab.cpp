/*
 * Copyright (c) 2001, 2023, Oracle and/or its affiliates. All rights reserved.
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

#include "gc/g1/heapRegion.hpp"
#include "precompiled.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/gc_globals.hpp"
#include "gc/shared/block_plab.inline.hpp"
#include "gc/shared/threadLocalAllocBuffer.hpp"
#include "gc/shared/tlab_globals.hpp"
#include "logging/log.hpp"
#include "memory/universe.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/globals_extension.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstring>

// size_t BlockPLAB::min_size() {
//   // Make sure that we return something that is larger than AlignmentReserve
//   return align_object_size(MAX2(MinTLABSize / HeapWordSize, (size_t)oopDesc::header_size())) + CollectedHeap::lab_alignment_reserve();
// }

// size_t BlockPLAB::max_size() {
//   return ThreadLocalAllocBuffer::max_size();
// }

size_t BlockPLAB::size() {
  // BlockPLAB size is 1/16 of region size, aligned to object size
  // size_t region_size_16th = HeapRegion::GrainBytes / 16;

  // log_error(gc) ("HeapRegion::GrainBytes: %lu", HeapRegion::GrainBytes);
  // log_error(gc) ("region_size_16th: %lu", region_size_16th);
  // Convert to heap words and align to object size
  // size_t size_in_words = region_size_16th / HeapWordSize;

  size_t size_in_words = 256 * K / HeapWordSize;
  // log_error(gc) ("size_in_words: %lu", size_in_words);

  guarantee(size_in_words >= (size_t)oopDesc::header_size() + 2, "size_in_words is too small");
  return size_in_words;
  // return align_object_size(MAX2(size_in_words, (size_t)oopDesc::header_size())) + CollectedHeap::lab_alignment_reserve();
  // size_t size_value = align_object_size(MAX2(size_in_words, (size_t)oopDesc::header_size())) + CollectedHeap::lab_alignment_reserve();
  // log_error(gc) ("size_value: %lu", size_value);
  // log_error(gc) ("oopDesc::header_size: %lu", (size_t)oopDesc::header_size());
  // log_error(gc) ("CollectedHeap::lab_alignment_reserve: %lu", CollectedHeap::lab_alignment_reserve());
  // return size_value;
}

void BlockPLAB::startup_initialization() {
  // do nothing
}

BlockPLAB::BlockPLAB() :
  _bottom(nullptr), _top(nullptr),
  _end(nullptr), _hard_end(nullptr), _allocated(0), _wasted(0), _undo_wasted(0), 
  _prev(nullptr), _next(nullptr), _alloc_region(nullptr), _idx(0), _full(false),
  _type(BlockPLABType::Default())
{
  // BlockPLAB now has fixed size
  assert(is_object_aligned(size()), "BlockPLAB size is not aligned to object size");
}

void BlockPLAB::flush_and_retire_stats(BlockPLABStats* stats) {
  // Retire the last allocation buffer.
  size_t unused = retire_internal();

  // Now flush the statistics.
  // stats->add_allocated(_allocated);
  // stats->add_wasted(_wasted);
  // stats->add_undo_wasted(_undo_wasted);
  // stats->add_unused(unused);

  // Since we have flushed the stats we need to clear  the _allocated and _wasted
  // fields in case somebody retains an instance of this over GCs. Not doing so
  // will artificially inflate the values in the statistics.
  _allocated   = 0;
  _wasted      = 0;
  _undo_wasted = 0;
}

void BlockPLAB::retire() {
  _wasted += retire_internal();
}

size_t BlockPLAB::retire_internal() {
  size_t result = 0;
  if (_top < _hard_end) {
    Universe::heap()->fill_with_dummy_object(_top, _hard_end, true);
    result += invalidate();
  }
  return result;
}

void BlockPLAB::add_undo_waste(HeapWord* obj, size_t word_sz) {
  Universe::heap()->fill_with_dummy_object(obj, obj + word_sz, true);
  _undo_wasted += word_sz;
}

void BlockPLAB::undo_last_allocation(HeapWord* obj, size_t word_sz) {
  assert(pointer_delta(_top, _bottom) >= word_sz, "Bad undo");
  assert(pointer_delta(_top, obj) == word_sz, "Bad undo");
  _top = obj;
}

void BlockPLAB::undo_allocation(HeapWord* obj, size_t word_sz) {
  // Is the alloc in the current alloc buffer?
  if (contains(obj)) {
    assert(contains(obj + word_sz - 1),
      "should contain whole object");
    undo_last_allocation(obj, word_sz);
  } else {
    add_undo_waste(obj, word_sz);
  }
}

void BlockPLAB::set_buf(HeapWord* buf) {
  // BlockPLAB has fixed size
  assert(BlockPLAB::size() > CollectedHeap::lab_alignment_reserve(), "Too small");

  _bottom   = buf;
  _top      = _bottom;
  _hard_end = _bottom + size();
  _end      = _hard_end - CollectedHeap::lab_alignment_reserve();
  assert(_end >= _top, "Negative buffer");
  // In support of ergonomic sizing
  _allocated += size();
}
