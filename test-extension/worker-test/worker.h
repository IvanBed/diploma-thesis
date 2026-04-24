#ifndef WORKER_H
#define WORKER_H

    #include "postgres.h"
    
    #include "miscadmin.h"
    
    #include "storage/ipc.h"
    #include "storage/shmem.h"
    #include "storage/lwlock.h"
    #include "storage/proc.h"
    #include "storage/latch.h"   
    
    #include "postmaster/bgworker.h"
    #include "postmaster/interrupt.h"
    #include "tcop/tcopprot.h"
     
    #include "executor/spi.h"

    #include "utils/builtins.h"
    #include "utils/wait_event.h"
    #include "utils/guc.h"
    #include <utils/rel.h>
    #include <utils/snapmgr.h>
    #include "utils/dsa.h"
    #include "utils/memutils.h"

    #include "libpq/pqsignal.h"

    #define TABLE_NAME "test_table"

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