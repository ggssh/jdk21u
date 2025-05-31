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

#ifndef SHARE_GC_G1_G1DATASTRUCTUREREGIONSET_INLINE_HPP
#define SHARE_GC_G1_G1DATASTRUCTUREREGIONSET_INLINE_HPP

#include "gc/g1/heapRegion.hpp"
#include "gc/g1/g1Allocator.hpp"
#include "gc/g1/g1CollectedHeap.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/g1/g1CardTable.hpp"
#include "utilities/linkedlist.hpp"
#include "oops/symbolHandle.hpp"
#include "runtime/mutexLocker.hpp"

template<typename Func>
void G1DataStructureRegionSet::scan_cards(Func&& f){
    HeapRegion* present_region = nullptr;
    LinkedListNode<G1CardTable::CardValue*>* p = _out_cards.head();
    G1CardTable::CardValue* left = nullptr, *right = nullptr;

    G1CollectedHeap* g1h = G1CollectedHeap::heap();
    G1CardTable* ct = g1h->card_table();

    if(p != nullptr){
        left = *p->data();
        right = *p->data();
        present_region = g1h->heap_region_containing(ct->addr_for(left));
        assert(present_region->data_structure() == this, "must be");
    } else {
        return;
    }
    
    while (p != nullptr) {
        G1CardTable::CardValue* present = *p->data();
        HeapRegion* region = g1h->heap_region_containing(ct->addr_for(present));
        assert(region->data_structure() == this, "must be");
        if(region != present_region || present - right != 1){
            f(present_region->hrm_index(), left, right + 1);
            left = present;
            right = present;
            present_region = region;
        } else {
            right = present;
        }
        p = p->next();
    }
    f(present_region->hrm_index(), left, right + 1);
}

#endif // SHARE_GC_G1_G1DIRTYCARDQUEUE_HPP
 