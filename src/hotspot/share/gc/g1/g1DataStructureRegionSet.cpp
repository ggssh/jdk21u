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
    _symbols.add(new SymbolHandle(symbol));
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

    if(node = find_node(symbol) != nullptr) {
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
        from_node = create_new_node(from, NormalNode);
    }

    if(to_node == nullptr) {
        to_node = create_new_node(to, NormalNode);
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