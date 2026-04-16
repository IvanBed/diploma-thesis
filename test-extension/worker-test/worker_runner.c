#include "worker_runner.h"

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

CounterData *counterData;

static void test_shmem_request()
{
    if(prev_shmem_request_hook)
        prev_shmem_request_hook();

    RequestAddinShmemSpace(MAXALIGN(sizeof(CounterData)));
    RequestNamedLWLockTranche("shmem_chunk", 1);
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
        counterData->lock = &(GetNamedLWLockTranche("shmem_chunk"))->lock;
    }

    LWLockRelease(AddinShmemInitLock);
}

void init_worker(BackgroundWorker *worker)
{
    memset(worker, 0, sizeof(*worker));
    
    (*worker).bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    (*worker).bgw_start_time = BgWorkerStart_RecoveryFinished;
    (*worker).bgw_restart_time = BGW_NEVER_RESTART;
    
    sprintf((*worker).bgw_library_name, "worker");
    sprintf((*worker).bgw_function_name, "worker_main");
    (*worker).bgw_notify_pid = 0;
    snprintf((*worker).bgw_name, BGW_MAXLEN, "worker worker %d", 1);
    snprintf((*worker).bgw_type, BGW_MAXLEN, "worker");
    //(*worker).bgw_main_arg = PointerGetDatum(counterData);
}

void _PG_init()
{

    if(!process_shared_preload_libraries_in_progress)
        elog(FATAL, "Please use shared_preload_libraries");

    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = test_shmem_request;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = test_shmem_startup;

    BackgroundWorker worker;
    init_worker(&worker);

    RegisterBackgroundWorker(&worker); 
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

