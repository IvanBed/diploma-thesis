#include "worker_runner.h"
#define STORE_CAPACITY 5


static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

static CounterData *counterData;

static Storage *storage;

void request_shmem_storage()
{
    size_t storage_size   = sizeof(Storage)

    RequestAddinShmemSpace(MAXALIGN(storage_size));
    RequestNamedLWLockTranche("shmem_storage_chunk", 1);
}

void init_storage_shmem_if_needed()
{
    bool found;
    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    storage = ShmemInitStruct("Storage", sizeof(Storage), &found);
    if(!found) 
	{
        storage->store_capacity = STORE_CAPACITY;
        storage->size           = 0;
        storage->store          = (Entry*) ShmemAlloc(sizeof(Entry) * STORE_CAPACITY);
        storage->lock           = &(GetNamedLWLockTranche("shmem_storage_chunk"))->lock;
    }

    LWLockRelease(AddinShmemInitLock);
}

void request_shmem_counter_data()
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(CounterData)));
    RequestNamedLWLockTranche("shmem_chunk", 1);
}

void init_storage_counter_data()
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    counterData = ShmemInitStruct("CounterData", sizeof(CounterData), &found);
    if(!found) 
	{
        counterData->counter = 0;
        counterData->lock = &(GetNamedLWLockTranche("shmem_chunk"))->lock;
    }

    LWLockRelease(AddinShmemInitLock);
}

static void test_shmem_request()
{
    if(prev_shmem_request_hook)
        prev_shmem_request_hook();


    request_shmem_counter_data();
    request_shmem_storage();

}

static void test_shmem_startup()
{
    bool found;
    
    if(prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    init_storage_counter_data();
    init_storage_shmem_if_needed();
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


PG_FUNCTION_INFO_V1(set_store_entry);

Datum set_store_entry(PG_FUNCTION_ARGS)
{
    int32_t id       = DatumGetInt(PG_GETARG_DATUM(0));
    const char* name = TextDatumGetCString(PG_GETARG_DATUM(1));
    if (!full())
    {
        Entry entry;
        entry.id   = id;
        entry.name = name;
        add_el(&entry);
    }
}

bool full()
{
    bool res = false;
    LWLockAcquire(storage->lock, LW_SHARED);
    if (storage->size < storage->capacity)
        res = false;
    else
        res = true;

    LWLockRelease(counterData->lock);
    return res;
}

void add_el(Entry *entry)
{
    if (!entry)
        return;
        
    LWLockAcquire(storage->lock, LW_EXCLUSIVE);
    size_t pos = storage->size + 1;
    
    memcpy(storage->store[pos], entry, sizeof(Entry)); 
    ++storage->size;

    LWLockRelease(storage->lock);
}


/*    typedef struct Storage 
    {
        LWLock* lock;
        size_t  store_capacity;
        size_t  size;
        Entry   *store;    
        MemoryContext storage_mem_cxt;

    } Storage;*/