#include <gc/g1/g1DataStructureRegionSet.hpp>

G1DataStructureEdge* G1DataStructure::find_edge(Symbol* from, Symbol* to) {
    LinkedListNode<G1DataStructureEdge*>* p = _edges.head();
    while (p != nullptr) {
        if((*p->data())->from()->symbol() == from && (*p->data())->to()->symbol() == to){
            return *(p->data());
        }
        p = p->next();
    }
    return nullptr;
}

G1DataStructureNode* G1DataStructure::symbol_in_roots(Symbol* symbol) {
    LinkedListNode<G1DataStructureNode*>* p = _roots.head();
    while (p != nullptr) {
        if((*p->data())->symbol() == symbol){
            return *(p->data());
        }
        p = p->next();
    }
    return nullptr;
}

G1DataStructureRegionSet::G1DataStructureRegionSet(G1CollectedHeap* heap, G1DataStructure* data_structure) :
    _regions(),
    _data_structure(data_structure),
    _alloc_region(heap->alloc_buffer_stats(G1HeapRegionAttr::Old), this),
    _plab_data(),
    _retained_old_region(nullptr) { }

void G1DataStructureRegionSet::init_data_structure_alloc_region(G1Allocator* allocator, G1EvacInfo* evacuation_info) {
    _alloc_region.init();
    allocator->reuse_retained_old_region(evacuation_info,
                                    &_alloc_region,
                                    &_retained_old_region);
}

void G1DataStructureRegionSet::release_data_structure_alloc_region() {
    _retained_old_region = _alloc_region.release();
}