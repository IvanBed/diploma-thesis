#ifndef WORKER_RUNNER_H
#define WORKER_RUNNER_H

    #include "postgres.h"
    #include "fmgr.h"
    #include "miscadmin.h"
    
    #include "storage.h"
    
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
    

#endif