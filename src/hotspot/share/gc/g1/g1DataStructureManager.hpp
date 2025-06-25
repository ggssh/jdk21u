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

#ifndef SHARE_GC_G1_G1DATASTRUCTUREMANAGER_HPP
#define SHARE_GC_G1_G1DATASTRUCTUREMANAGER_HPP

#include "gc/g1/heapRegion.hpp"
#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "utilities/linkedlist.hpp"
#include "utilities/hashMap.hpp"
#include "oops/symbolHandle.hpp"

class PLAB;
class BlockPLAB;

class DataStructureConfig : public AllStatic {
public:
    static uint get_hash(G1DataStructureRegionSet* const& key) { return ((uintptr_t)key) & 0xffffffff; }
    static bool key_equals(G1DataStructureRegionSet* const& key1, G1DataStructureRegionSet* const& key2) {
        return key1 == key2;
    }
};

typedef HashMap<G1DataStructureRegionSet*, G1PLABAllocator::PLABData*, DataStructureConfig, mtGC> DataPLABMap;
typedef HashMap<G1DataStructureRegionSet*, G1PLABAllocator::BlockPLABData*, DataStructureConfig, mtGC> DataBlockPLABMap;

class G1DataStructureManager : public CHeapObj<mtGC> {
private:
    LinkedListImpl<G1DataStructureRegionSet*> _data_structures;
    LinkedListImpl<G1DataStructure*> _data_structure_types;

    G1DataStructure* get_data_structure_by_root(Symbol* root_symbol);
    uint _present_id;
    G1Allocator* _allocator;
    G1EvacInfo* _evacuation_info;
    Mutex _data_structures_lock;

public:
    G1DataStructureManager() : _data_structures(), _data_structure_types(), _present_id(0), 
        _allocator(nullptr), _evacuation_info(nullptr), _data_structures_lock(Mutex::nosafepoint, "data_structures lock") {}
    
    G1DataStructureRegionSet* get_data_structure(oop from_oop, oop to_oop);
    void init_data_structure_alloc_regions(G1Allocator* allocator, G1EvacInfo* evacuation_info);
    void release_data_structure_alloc_regions();
    void abandon_data_structure_alloc_regions();
    uint alloc_count();
    bool is_retained_old_region(HeapRegion* hr);
    
    void initialize_predefined_data_structures();

    DataPLABMap* create_and_initialize_plab_map(uint num_alloc_buffers, size_t desired_plab_size, size_t tolerated_refills);
    DataBlockPLABMap* create_and_initialize_block_plab_map(uint num_alloc_buffers, size_t desired_plab_size, size_t tolerated_refills);

    class DeleteClosure : public StackObj {
    public:
        void work(G1DataStructureRegionSet*& key, G1PLABAllocator::PLABData*& value){
            delete value;
        }
    };

    class DeleteBlockPLABClosure : public StackObj {
    public:
        void work(G1DataStructureRegionSet*& key, G1PLABAllocator::BlockPLABData*& value){
            delete value;
        }
    };

    void delete_plab_map(DataPLABMap* plab_map);
    void delete_block_plab_map(DataBlockPLABMap* block_plab_map);
    void initialize_at_conc_start();
    void data_structures_instances_iterate(G1DataStructureRegionSetClosure* closure);
    void clear_all_out_cards();
    void clear_all_instances();
    void remove_instance(G1DataStructureRegionSet* data_structure_instance);
    void remove_dead_instances();
    void verify_all();
};

#endif // SHARE_GC_G1_G1DIRTYCARDQUEUE_HPP
  