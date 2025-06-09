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

#ifndef SHARE_GC_G1_G1DATASTRUCTUREREGIONSET_HPP
#define SHARE_GC_G1_G1DATASTRUCTUREREGIONSET_HPP

#include "gc/g1/heapRegion.hpp"
#include "gc/g1/g1Allocator.hpp"
// #include "gc/g1/g1CollectedHeap.hpp"
// #include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/g1/g1CardTable.hpp"
#include "utilities/linkedlist.hpp"
#include "oops/symbolHandle.hpp"
#include "runtime/mutexLocker.hpp"


enum G1DataStructureNodeType {
    KlassNode,
    HeapRootNode,
    TreeRootNode,
};

enum G1DataStructureEdgeType {
    NormalEdge,
    BoundaryEdge,
};

class G1DataStructureEdge;

struct G1DataStructureNode : public CHeapObj<mtGC> {
    SymbolHandle _symbol;
    LinkedListImpl<G1DataStructureEdge*> _to_edges;
    LinkedListImpl<G1DataStructureEdge*> _from_edges;
    G1DataStructureNodeType _type;

    Symbol* symbol() const {
        return (Symbol*)_symbol;
    }
};

struct G1DataStructureEdge : public CHeapObj<mtGC> {
    G1DataStructureNode* _from;
    G1DataStructureNode* _to;
    G1DataStructureEdgeType _type;
    size_t _evac_count;
    size_t _evac_size;
    size_t _overwrite_count;
    size_t _overwrite_size;
};

class G1DataStructure : public CHeapObj<mtGC> {
private:
    LinkedListImpl<G1DataStructureNode*> _roots;
    // LinkedListImpl<SymbolHandle*> _symbols;
    LinkedListImpl<G1DataStructureNode*> _nodes;
    LinkedListImpl<G1DataStructureEdge*> _edges;

    G1DataStructureNode* create_new_node(Symbol* symbol, G1DataStructureNodeType type);

public:
    G1DataStructureNode* symbol_in_roots(Symbol* symbol);
    G1DataStructureEdge* find_edge(Symbol* from, Symbol* to);
    G1DataStructureNode* find_node(Symbol* symbol);
    void add_root(Symbol* symbol);
    void add_edge(Symbol* from, Symbol* to);
};


class G1DataStructureRegionSet : public CHeapObj<mtGC> {
private:
    static int compare(G1CardTable::CardValue* const& left, G1CardTable::CardValue* const& right){
        return (uintptr_t)left - (uintptr_t)right;
    }
    LinkedListImpl<HeapRegion*> _regions;
    SortedLinkedList<G1CardTable::CardValue*, compare> _out_cards;
    G1DataStructure* _data_structure;
    OldDataStructureGCAllocRegion _alloc_region;
    // G1PLABAllocator::PLABData _plab_data;
    HeapRegion* _retained_old_region;
    Mutex _regions_lock;
    uint _id;
    volatile bool _is_alive;

public:
    G1DataStructureRegionSet(G1CollectedHeap* heap, G1DataStructure* data_structure, uint id);
    ~G1DataStructureRegionSet();

    void clear_regions();


    void add_region(HeapRegion* region) {
        MutexLocker ml(&_regions_lock, Mutex::_no_safepoint_check_flag);
        _regions.add(region);
    }

    void remove_region(HeapRegion* region) {
        MutexLocker ml(&_regions_lock, Mutex::_no_safepoint_check_flag);
        _regions.remove(region);
    }

    bool is_data_structure_root_symbol(Symbol* symbol) {
        return _data_structure->symbol_in_roots(symbol) != nullptr;
    }

    bool region_in(HeapRegion* region){
        MutexLocker ml(&_regions_lock, Mutex::_no_safepoint_check_flag);
        return _regions.find(region) != nullptr;
    }

    OldDataStructureGCAllocRegion* alloc_region() {
        return &_alloc_region;
    }

    // G1PLABAllocator::PLABData* plab_data() {
    //     return &_plab_data;
    // }

    G1DataStructureEdge* find_edge(Symbol* from, Symbol* to) {
        return _data_structure->find_edge(from, to);
    }

    void init_data_structure_alloc_region(G1Allocator* allocator, G1EvacInfo* evacuation_info);
    void release_data_structure_alloc_region();

    bool is_retained_old_region(HeapRegion* hr) {
        return _retained_old_region == hr;
    }

    uint id() const {
        return _id;
    }

    void clear_out_cards(){
        _out_cards.clear();
    }

    void add_out_card(G1CardTable::CardValue* card) {
        _out_cards.add(card);
    }

    bool is_alive(){
        return _is_alive;
    }

    void set_alive(bool is_alive){
        _is_alive = is_alive;
    }

    bool set_alive_par(){
        if(_is_alive){
            return false;
        }
        bool val = Atomic::cmpxchg(&_is_alive, false, true);
        if(!val){
            return true;
        }
        return false;
    }

    template<typename Func> void scan_cards(Func&& f);
    // void scan_cards(Func&& f){
    //     HeapRegion* present_region = nullptr;
    //     LinkedListNode<G1CardTable::CardValue*>* p = _out_cards.head();
    //     G1CardTable::CardValue* left = nullptr, *right = nullptr;

    //     G1CollectedHeap* g1h = G1CollectedHeap::heap();
    //     G1CardTable* ct = g1h->card_table();

    //     if(p != nullptr){
    //         left = *p->data();
    //         right = *p->data();
    //         present_region = g1h->heap_region_containing(ct->addr_for(left));
    //         assert(present_region->data_structure == this);
    //     } else {
    //         return;
    //     }
        
    //     while (p != nullptr) {
    //         G1CardTable::CardValue* present = *p->data();
    //         HeapRegion* region = g1h->heap_region_containing(ct->addr_for(present));
    //         assert(region->data_structure == this);
    //         if(region != present_region || present - right != 1){
    //             f(present_region->hrm_index(), left, right + 1);
    //             left = present;
    //             right = present;
    //             present_region = region;
    //         } else {
    //             right = present;
    //         }
    //     }
    //     f(present_region->hrm_index(), left, right + 1);
    // }

};

class G1DataStructureRegionSetClosure : public Closure {
public:
    virtual void do_data_structure_instance(G1DataStructureRegionSet* data_structure_instance) = 0;
};    

#endif // SHARE_GC_G1_G1DIRTYCARDQUEUE_HPP
 