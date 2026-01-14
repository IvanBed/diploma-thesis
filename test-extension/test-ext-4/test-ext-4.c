#include <postgres.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/builtins.h>


static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;


typedef struct CounterData 
{
    LWLock* lock;
    int32_t counter;
} CounterData;

CounterData *counterData;

static void test_shmem_request()
{
    if(prev_shmem_request_hook)
        prev_shmem_request_hook();

    RequestAddinShmemSpace(MAXALIGN(sizeof(CounterData)));
    RequestNamedLWLockTranche("experiment", 1);
}

static void test_shmem_startup()
{
    bool found;
    
    if(prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    counterData = ShmemInitStruct("CounterData", sizeof(CounterData), &found);
    if(!found) 
	{
        counterData->counter = 0;
        counterData->lock = &(GetNamedLWLockTranche("experiment"))->lock;
    }

    LWLockRelease(AddinShmemInitLock);
}

void _PG_init()
{
    if(!process_shared_preload_libraries_in_progress)
        elog(FATAL, "Please use shared_preload_libraries");

    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = test_shmem_request;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = test_shmem_startup;
}

PG_FUNCTION_INFO_V1(get_counter_value);

Datum get_counter_value(PG_FUNCTION_ARGS)
{
  int32_t result;

  LWLockAcquire(counterData->lock, LW_SHARED);
  result = counterData->counter;
  LWLockRelease(counterData->lock);

  PG_RETURN_INT32(result);
}

PG_FUNCTION_INFO_V1(atomic_increment);

Datum atomic_increment(PG_FUNCTION_ARGS)
{
   
  LWLockAcquire(counterData->lock, LW_EXCLUSIVE);
  counterData->counter++;
  LWLockRelease(counterData->lock);

  PG_RETURN_VOID();
}
