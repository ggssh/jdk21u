#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "gc/g1/g1CollectedHeap.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"

G1DataStructureEdge* G1DataStructure::find_edge(Symbol* from, Symbol* to) {
    LinkedListNode<G1DataStructureEdge*>* p = _edges.head();
    while (p != nullptr) {
        if((*p->data())->_from->symbol() == from && (*p->data())->_to->symbol() == to){
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

G1DataStructureNode* G1DataStructure::create_new_node(Symbol* symbol, G1DataStructureNodeType type) {
    G1DataStructureNode* node = new G1DataStructureNode();
    node->_symbol = symbol;
    node->_type = type;
    _nodes.add(node);
    // _symbols.add(new SymbolHandle(symbol));
    return node;
}

G1DataStructureNode* G1DataStructure::find_node(Symbol* symbol) {
    LinkedListNode<G1DataStructureNode*>* p = _nodes.head();
    while (p != nullptr) {
        if((*p->data())->symbol() == symbol){
            return *(p->data());
        }
        p = p->next();
    }
    return nullptr;
}

void G1DataStructure::add_root(Symbol* symbol) {
    if(symbol_in_roots(symbol) != nullptr) {
        return;
    }

    G1DataStructureNode* node;

    if((node = find_node(symbol)) != nullptr) {
        node->_type = HeapRootNode;
        _roots.add(node);
        return;
    } else {
        node = create_new_node(symbol, HeapRootNode);
        _roots.add(node);
    }
}

void G1DataStructure::add_edge(Symbol* from, Symbol* to) {
    if(find_edge(from, to) != nullptr) {
        return;
    }
    G1DataStructureNode* from_node = find_node(from);
    G1DataStructureNode* to_node = find_node(to);

    if(from_node == nullptr) {
        from_node = create_new_node(from, KlassNode);
    }

    if(to_node == nullptr) {
        to_node = create_new_node(to, KlassNode);
    }

    G1DataStructureEdge* edge = new G1DataStructureEdge();
    edge->_from = from_node;
    edge->_to = to_node;
    edge->_type = NormalEdge;
    edge->_evac_count = 0;
    edge->_evac_size = 0;
    edge->_overwrite_count = 0;
    edge->_overwrite_size = 0;

    _edges.add(edge);
}


G1DataStructureRegionSet::G1DataStructureRegionSet(G1CollectedHeap* heap, G1DataStructure* data_structure, uint id) :
    _regions(),
    _out_cards(),
    _data_structure(data_structure),
    _alloc_region(heap->alloc_buffer_stats(G1HeapRegionAttr::Old), this),
    // _plab_data(),
    _retained_old_region(nullptr),
    _regions_lock(Mutex::nosafepoint, "regions lock"),
    _id(id),
    _is_alive(false) { 
    
    // size_t _tolerated_refills = 0;
    
    // if (ResizePLAB) {
    //     // See G1EvacStats::compute_desired_plab_sz for the reasoning why this is the
    //     // expected number of refills.
    //     double const ExpectedNumberOfRefills = G1LastPLABAverageOccupancy / TargetPLABWastePct;
    //     // Add some padding to the threshold to not boost exactly when the targeted refills
    //     // were reached.
    //     // E.g. due to limitation of PLAB size to non-humongous objects and region boundaries
    //     // a thread may experience more refills than expected. Keeping the PLAB waste low
    //     // is the main goal, so being a bit conservative is better.
    //     double const PadFactor = 1.5;
    //     _tolerated_refills = MAX2(ExpectedNumberOfRefills, 1.0) * PadFactor;
    // } else {
    //     // Make the tolerated refills a huge number.
    //     _tolerated_refills = SIZE_MAX;
    // }
    // // The initial PLAB refill should not count, hence the +1 for the first boost.
    // size_t initial_tolerated_refills = ResizePLAB ? _tolerated_refills + 1 : _tolerated_refills;
    // _plab_data.initialize(1, heap->desired_plab_sz(G1HeapRegionAttr::Old), initial_tolerated_refills);
}

void G1DataStructureRegionSet::init_data_structure_alloc_region(G1Allocator* allocator, G1EvacInfo* evacuation_info) {
    _alloc_region.init();
    allocator->reuse_retained_old_region(evacuation_info,
                                    &_alloc_region,
                                    &_retained_old_region);
}

void G1DataStructureRegionSet::release_data_structure_alloc_region() {
    _retained_old_region = _alloc_region.release();
}