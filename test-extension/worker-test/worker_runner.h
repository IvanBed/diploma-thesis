#ifndef WORKER_RUNNER_H
#define WORKER_RUNNER_H

    #include "postgres.h"
    #include "fmgr.h"
    #include "miscadmin.h"
    
    #include "storage/ipc.h"
    #include "storage/shmem.h"
    #include "storage/lwlock.h"
    #include "storage/latch.h"  
    #include "storage/proc.h"

    #include "utils/builtins.h"
    #include "utils/memutils.h"
    #include "utils/dsa.h"

    #include "nodes/pg_list.h"
    #include "nodes/memnodes.h"

    #include "postmaster/bgworker.h"
    #include "postmaster/interrupt.h"

    #include <string.h>

    #define FREE 0 
    #define ALLOCATED 1
    #define STORAGE_FULL -1

    #define STORE_CAPACITY 25
    #define TEXT_STORE_MAX_SIZE 1024 * 1024

    typedef struct Entry
    {
        int32_t id;
        union
	    {
		    dsa_pointer text_pos;	/* text location within text buffer */
		    char	   *text_pointer;
	    }	test_text;
    } Entry;

    typedef struct Storage 
    {
        LWLock       *lock;
        size_t        store_capacity;
        Entry        *store;
        uint8_t      *free_space_bitmap;
        
        void         *raw_dsa_area;
        MemoryContext storage_mem_cxt;
    } Storage;

#endif