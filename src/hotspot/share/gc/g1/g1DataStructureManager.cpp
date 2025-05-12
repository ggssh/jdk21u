#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "gc/g1/g1DataStructureManager.hpp"
#include "gc/g1/heapRegion.hpp"
#include "gc/g1/g1CollectedHeap.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/shared/plab.hpp"
#include "oops/oop.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klass.hpp"
#include "oops/symbol.hpp"
#include "classfile/symbolTable.hpp"


G1DataStructureRegionSet* G1DataStructureManager::get_data_structure_by_root(Symbol* root_symbol) {
    G1DataStructureRegionSet* data_structure = nullptr;
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        data_structure = *p->data();
        // log_info(gc)("data structure");
        if (data_structure->is_data_structure_root_symbol(root_symbol)) {
            return data_structure;
        }
        p = p->next();
    }
    return nullptr;
}

G1DataStructureRegionSet* G1DataStructureManager::get_data_structure(oop from_oop, oop to_oop) {
    Symbol* to_symbol = to_oop->klass()->name();
    G1DataStructureRegionSet* data_structure = get_data_structure_by_root(to_symbol);
    if (data_structure != nullptr) {
        return data_structure;
    }

    G1CollectedHeap* g1h = G1CollectedHeap::heap();

    if (from_oop != nullptr) {
        Symbol* from_symbol = from_oop->klass()->name();
        Symbol* to_symbol = to_oop->klass()->name();
        HeapRegion* from_region = g1h->heap_region_containing(from_oop);
        if (from_region->data_structure() != nullptr) {
            G1DataStructureRegionSet* data_structure = from_region->data_structure();
            if (data_structure->find_edge(from_symbol, to_symbol) != nullptr) {
                return data_structure;
            }
        }
    }

    return nullptr;
}

void G1DataStructureManager::init_data_structure_alloc_regions(G1Allocator* allocator, G1EvacInfo* evacuation_info) {
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        G1DataStructureRegionSet* data_structure = *p->data();
        data_structure->init_data_structure_alloc_region(allocator, evacuation_info);
        p = p->next();
    }
}

uint G1DataStructureManager::alloc_count() {
    uint count = 0;
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        G1DataStructureRegionSet* data_structure = *p->data();
        count += data_structure->alloc_region()->count();
        p = p->next();
    }
    return count;
}

void G1DataStructureManager::release_data_structure_alloc_regions() {
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        G1DataStructureRegionSet* data_structure = *p->data();
        data_structure->release_data_structure_alloc_region();
        p = p->next();
    }
}

bool G1DataStructureManager::is_retained_old_region(HeapRegion* hr) {
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        G1DataStructureRegionSet* data_structure = *p->data();
        if (data_structure->region_in(hr)) {
            return data_structure->is_retained_old_region(hr);
        }
        p = p->next();
    }
    return false;
}

void G1DataStructureManager::initialize_predefined_data_structures() {
    // Symbol* s1 = SymbolTable::new_symbol("[Ledu/cmu/graphchi/ChiVertex;");
    Symbol* s2 = SymbolTable::new_symbol("edu/cmu/graphchi/ChiVertex");
    Symbol* s3 = SymbolTable::new_symbol("[I");

    G1DataStructure* data_structure = new G1DataStructure();
    // data_structure->add_root(s1);
    data_structure->add_root(s2);

    data_structure->add_edge(s2, s3);

    G1DataStructureRegionSet* data_structure_region_set = new G1DataStructureRegionSet(G1CollectedHeap::heap(), data_structure);
    _data_structures.add(data_structure_region_set);
}

DataPLABMap* G1DataStructureManager::create_and_initialize_plab_map(uint num_alloc_buffers, size_t desired_plab_size, size_t tolerated_refills){
    DataPLABMap* plab_map = new DataPLABMap();
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        G1DataStructureRegionSet* data_structure = *p->data();
        G1PLABAllocator::PLABData* plab_data = new G1PLABAllocator::PLABData();
        plab_data->initialize(num_alloc_buffers, desired_plab_size, tolerated_refills);
        plab_map->insert(data_structure, plab_data);
        p = p->next();
    }
    return plab_map;

    // return nullptr;
}

void G1DataStructureManager::delete_plab_map(DataPLABMap* plab_map) {
    DeleteClosure cl;
    plab_map->forEachClosure(&cl);
    delete plab_map;
}