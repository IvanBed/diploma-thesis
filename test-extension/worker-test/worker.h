#ifndef WORKER_H
#define WORKER_H

    #include "postgres.h"
    
    #include "miscadmin.h"
    
    #include "storage/ipc.h"
    #include "storage/shmem.h"
    #include "storage/lwlock.h"
    #include "storage/proc.h"
    #include "storage/latch.h"
    
    #include <access/amapi.h>
    #include <access/heapam.h>
    #include <access/htup_details.h>
    #include <access/table.h>
    #include <access/tableam.h>
    
    
    #include "postmaster/bgworker.h"
    #include "postmaster/interrupt.h"
    #include "tcop/tcopprot.h"
    
    
    
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

    typedef struct FormData_test_table
    {
        int32 id;
        text name;
    
    } FormData_test_table;

    typedef enum Anum_test_table
    {
    	Anum_test_table_id = 1,
    	Anum_test_table_name,
    	_Anum_test_table_max,
    } Anum_test_table;
    
    #define Natts_test_table (_Anum_test_table_max - 1)
    
    typedef enum Anum_test_table_name_idx
    {
    	Anum_test_table_name_idx_name = 1,
    	_Anum_test_table_name_idx_max,
    } Anum_test_table_name_idx;
    
    #define Natts_test_table_name_idx (_Anum_test_table_name_idx_max - 1)

    inline Oid name_to_oid(const char* name)
    {
    	return DatumGetObjectId(DirectFunctionCall1(to_regclass, CStringGetTextDatum(name)));
    }

#endif