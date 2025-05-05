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
#include "utilities/linkedlist.hpp"
#include "oops/symbolHandle.hpp"

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

class G1DataStructureNode : public CHeapObj<mtGC> {
private:
    Symbol* _symbol;
    LinkedListImpl<G1DataStructureEdge*> _to_edges;
    LinkedListImpl<G1DataStructureEdge*> _from_edges;
    G1DataStructureNodeType _type;
public:
    Symbol* symbol() const {
        return _symbol;
    }
};

class G1DataStructureEdge : public CHeapObj<mtGC> {
private:
    G1DataStructureNode* _from;
    G1DataStructureNode* _to;
    G1DataStructureEdgeType _type;
    size_t _evac_count;
    size_t _evac_size;
    size_t _overwrite_count;
    size_t _overwrite_size;
public:
};

class G1DataStructure : public CHeapObj<mtGC> {
private:
    LinkedListImpl<G1DataStructureNode*> _roots;
    LinkedListImpl<SymbolHandle*> _symbols;
    LinkedListImpl<G1DataStructureNode*> _nodes;
    LinkedListImpl<G1DataStructureEdge*> _edges;

public:
    G1DataStructureNode* symbol_in_roots(Symbol* symbol);
};


class G1DataStructureRegionSet : public CHeapObj<mtGC> {
private:

    LinkedListImpl<HeapRegion*> _regions;
    G1DataStructure _data_structure;
public:
    bool is_data_structure_root_symbol(Symbol* symbol) {
        return _data_structure.symbol_in_roots(symbol) != nullptr;
    }

    bool region_in(HeapRegion* region){
        return _regions.find(region) != nullptr;
    }
};

#endif // SHARE_GC_G1_G1DIRTYCARDQUEUE_HPP
 