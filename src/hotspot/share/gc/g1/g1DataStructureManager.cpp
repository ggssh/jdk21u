#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "gc/g1/g1DataStructureManager.hpp"
#include "gc/g1/heapRegion.hpp"
#include "gc/shared/plab.hpp"

G1DataStructureRegionSet* G1DataStructureManager::get_data_structure_by_root(Symbol* root_symbol) {
    G1DataStructureRegionSet* data_structure = nullptr;
    LinkedListNode<G1DataStructureRegionSet*>* p = _data_structures.head();
    while (p != nullptr) {
        data_structure = *p->data();
        if (data_structure->is_data_structure_root_symbol(root_symbol)) {
            return data_structure;
        }
        p = p->next();
    }
    return nullptr;
}

PLAB* G1DataStructureManager::get_data_structure_plab(oop from_oop, oop to_oop) {
    Symbol* to_symbol = to_oop->klass()->name();
    G1DataStructureRegionSet* data_structure = get_data_structure_by_root(to_symbol);
    if (data_structure != nullptr) {
        return data_structure->plab_data()->alloc_buffer[0];
    }

    if (from_oop != nullptr) {
        Symbol* from_symbol = from_oop->klass()->name();
        Symbol* to_symbol = to_oop->klass()->name();
        HeapRegion* from_region = G1CollectedHeap::heap()->heap_region_containing(from_oop);
        if (from_region->data_structure() != nullptr) {
            G1DataStructureRegionSet* data_structure = from_region->data_structure();
            if (data_structure->find_edge(from_symbol, to_symbol) != nullptr) {
                return data_structure->plab_data()->alloc_buffer[0];
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
            return data_structure->alloc_region()->is_retained_old_region(hr);
        }
        p = p->next();
    }
    return false;
}
