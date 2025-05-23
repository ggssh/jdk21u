#ifndef SHARE_GC_G1_G1PARREFINETASK_HPP
#define SHARE_GC_G1_G1PARREFINETASK_HPP

#include "gc/shared/workerThread.hpp"
#include "gc/g1/g1ConcurrentRefine.hpp"
#include "gc/g1/g1FromCardCache.hpp"




class G1ParRefineTask : public WorkerTask {
    G1ConcurrentMark* _cm;
    G1ConcurrentRefine* _cr;

    G1ConcurrentRefineStats* _refinement_stats_array;


public:
    G1ParRefineTask(G1ConcurrentMark* cm, G1ConcurrentRefine cr, uint num_workers) :
        WorkerTask("par refine"),
        _cm(cm),
        _cr(cr) { 
        G1FromCardCache::invalidate(0, G1FromCardCache::max_reserved_regions());
        _refinement_stats_array = NEW_C_HEAP_ARRAY(G1ConcurrentRefineStats, num_workers, mtGC);
        for(uint i = 0; i < num_workers; i++) {
            _refinement_stats_array[i] = new G1ConcurrentRefineStats();
        }
    }

    ~G1ParRefineTask() {
        G1FromCardCache::invalidate(0, G1FromCardCache::max_reserved_regions());
        for(uint i = 0; i < _num_workers; i++) {
            delete _refinement_stats_array[i];
        }
        FREE_C_HEAP_ARRAY(G1ConcurrentRefineStats, _refinement_stats_array);
    }
  
    void work(uint worker_id) {

        // G1CollectedHeap* g1h = G1CollectedHeap::heap();
        while(_cr->try_refinement_step(worker_id, 0, _refinement_stats_array[worker_id]));
    }
};
  

#endif