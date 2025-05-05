#include <gc/g1/g1DataStructureRegionSet.hpp>

G1DataStructureNode* G1DataStructure::symbol_in_roots(Symbol* symbol) {
    LinkedListNode<G1DataStructureNode*> p = _roots.head();
    while (p != nullptr) {
        if((*p->data())->symbol() == symbol){
            return *(p->data());
        }
        p = p->next();
    }
    return nullptr;
}