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
    
    #include "libpq/pqsignal.h"

    #define TABLE_NAME "test_table"

    typedef struct CounterData 
    {
        LWLock* lock;
        int32_t counter;
    } CounterData;

#endif