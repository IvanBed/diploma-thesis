#include "worker_runner.h"

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

static Storage     *storage     = NULL;
static Latch       *latch       = NULL;

bool full(void);
void add_el(Entry *);

void request_shmem_shared_latch()
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(Latch)));
    RequestNamedLWLockTranche("shmem_shared_latch", 1);
}

void init_shared_latch_if_needed()
{
    bool found;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    latch = ShmemInitStruct("Latch", sizeof(Latch), &found);
    if(!found) 
    {
        InitSharedLatch(latch); 
        ResetLatch(latch);    
    }
    LWLockRelease(AddinShmemInitLock);
}

void request_shmem_storage()
{
    size_t storage_size   = sizeof(Storage);

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
        char *p = (char *) storage;
        
        storage->store_capacity    = STORE_CAPACITY;
        storage->store             = (Entry*) ShmemAlloc(sizeof(Entry) * STORE_CAPACITY);
        storage->free_space_bitmap = (Entry*) ShmemAlloc(sizeof(uint8_t) * STORE_CAPACITY);
        storage->lock              = &(GetNamedLWLockTranche("shmem_storage_chunk"))->lock;

        p += MAXALIGN(sizeof(Storage));
		storage->raw_dsa_area = p;
		storage->dsa = dsa_create_in_place(storage->raw_dsa_area, TEXT_STORE_MAX_SIZE, LWLockNewTrancheId(), 0);
		
        dsa_pin(storage->dsa);
		dsa_set_size_limit(storage->dsa, TEXT_STORE_MAX_SIZE);
         
        dsa_detach(storage->dsa);

        memset(storage->store, 0, sizeof(Entry) * STORE_CAPACITY);
        memset(storage->free_space_bitmap, 0, sizeof(uint8_t) * STORE_CAPACITY);
    }

    LWLockRelease(AddinShmemInitLock);
}

static void test_shmem_request()
{
    if(prev_shmem_request_hook)
        prev_shmem_request_hook();
  
    request_shmem_storage();
    request_shmem_shared_latch();
}

static void test_shmem_startup()
{
    if(prev_shmem_startup_hook)
        prev_shmem_startup_hook();
    
    init_storage_shmem_if_needed();
    init_shared_latch_if_needed();
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

PG_FUNCTION_INFO_V1(set_store_entry);
Datum set_store_entry(PG_FUNCTION_ARGS)
{
    // проверка на null
    int32_t id       = DatumGetInt32(PG_GETARG_DATUM(0));
    const char* name = TextDatumGetCString(PG_GETARG_DATUM(1));
    
    elog(NOTICE, "set_store_entry");
    elog(NOTICE, "id  %d", id);  
    elog(NOTICE, "name  %s", name); 
    
    Entry entry;
    entry.id   = id;
    //entry.name = "name";
    add_el(&entry);

    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(log_print);

Datum log_print(PG_FUNCTION_ARGS)
{
    if (storage)
    {
        elog(NOTICE, "STORAGE CONTENT");  
        elog(NOTICE, "--------------------------------------------------");
        elog(NOTICE, "capacity %ld", storage->store_capacity);
        
        for (size_t i = 0; i < storage->store_capacity; i++)
        {
            if (storage->free_space_bitmap[i] == ALLOCATED)
                elog(NOTICE, "%d %s", (storage->store + i)->id, "TEST LATCH"); 
        } 
        elog(NOTICE, "--------------------------------------------------");       
    } 
    PG_RETURN_VOID();
}

void attach_shmem(void)
{
	MemoryContext oldcontext;

	if (storage->dsa)
		return;

	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	storage->dsa = dsa_attach_in_place(storage->raw_dsa_area, NULL);
	dsa_pin_mapping(storage->dsa);

	MemoryContextSwitchTo(oldcontext);
}

dsa_area *get_dsa_area_for_text(void)
{
	attach_shmem();
	return storage->dsa;
}

int find_pos()
{
    int res_pos = STORAGE_FULL;
    LWLockAcquire(storage->lock, LW_SHARED);
    for (size_t i = 0; i < storage->store_capacity; i++)
    {
        if (storage->free_space_bitmap[i] == FREE)
        {
            res_pos = (int) i;
            break;
        }
    }
    LWLockRelease(storage->lock);
    return res_pos;
}

void add_el_internal(Entry *entry, size_t pos)
{
    LWLockAcquire(storage->lock, LW_EXCLUSIVE);
    memcpy(storage->store + pos, entry, sizeof(Entry));       
    storage->free_space_bitmap[pos] = ALLOCATED;
    LWLockRelease(storage->lock);
}

void add_el(Entry *entry)
{
    if (!entry)
        return;

    elog(NOTICE, "add_el");    
    
    int pos = find_pos();
    elog(NOTICE, "pos %ld", pos);
    if (pos != STORAGE_FULL)
    {
        add_el_internal(entry, pos);
    }
    else
    {
        elog(NOTICE, "wait for worker");    
        elog(NOTICE, "SetLatch"); 
        SetLatch(latch);
        // костыл для теста, поменять
        while ((pos = find_pos()) == STORAGE_FULL) {}
        add_el_internal(entry, pos);
    }
}

/*   
    typedef struct Entry
    {
        int32_t id;
        char*   name;
    } Entry;

    typedef struct Storage 
    {
        LWLock       *lock;
        size_t        store_capacity;
        Entry        *store;
        uint8_t      *free_space_bitmap;
        
        dsa_area     *dsa;
        void         *raw_dsa_area;
        
        MemoryContext storage_mem_cxt;
    } Storage;
*/