#ifndef SHARE_GC_G1_G1FLUSHLOGBUFFERBATCHTASK_HPP
#define SHARE_GC_G1_G1FLUSHLOGBUFFERBATCHTASK_HPP

#include "gc/g1/g1BatchedTask.hpp"

class G1FlushLogBufferBatchTask : public G1BatchedTask {
    class JavaThreadFlushLogs;
    class NonJavaThreadFlushLogs;
  
    size_t _old_pending_cards;
  
    // References to the tasks to retain access to statistics.
    JavaThreadFlushLogs* _java_retire_task;
    NonJavaThreadFlushLogs* _non_java_retire_task;
  
  public:
    G1FlushLogBufferBatchTask();
    ~G1FlushLogBufferBatchTask();
};

#endif