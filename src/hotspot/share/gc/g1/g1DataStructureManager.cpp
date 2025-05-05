#include "gc/g1/g1DataStructureRegionSet.hpp"
#include "gc/g1/g1DataStructureManager.hpp"

G1DataStructureRegionSet* G1DataStructureManager::get_data_structure_by_root(Symbol* root_symbol) {
    G1DataStructureRegionSet* data_structure = nullptr;
    LinkedListNode<G1DataStructureRegionSet> p = _data_structures.head();
    while (p != nullptr) {
        data_structure = p->data();
        if (data_structure->symbol_in_roots(root_symbol) != nullptr) {
            return data_structure;
        }
        p = p->next();
    }
    return nullptr;
}