#ifndef WORKER_RUNNER_H
#define WORKER_RUNNER_H

    #include "postgres.h"
    #include "fmgr.h"
    #include "miscadmin.h"
    
    #include "storage/ipc.h"
    #include "storage/shmem.h"
    #include "storage/lwlock.h"
    
    #include "utils/builtins.h"

    #include "nodes/pg_list.h"

    #include "postmaster/bgworker.h"
    #include "postmaster/interrupt.h"

    typedef struct CounterData 
    {
        LWLock* lock;
        int32_t counter;
    } CounterData;

    typedef struct Entry
    {
        int32_t id;
        char*   name;
    } Entry;

    typedef struct Storage 
    {
        LWLock* lock;
        size_t  store_capacity;
        size_t  size;
        Entry   *store;    
        MemoryContext storage_mem_cxt;

    } Storage;

#endif