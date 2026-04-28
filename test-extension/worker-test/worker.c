#include "worker.h"

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

static Storage     *storage     = NULL;
static Latch       *latch       = NULL;

static dsa_area    *local_dsa   = NULL;

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
        dsa_area   *dsa;
        char *p = (char *) storage;
        
        storage->store_capacity    = STORE_CAPACITY;
        storage->store             = (Entry*) ShmemAlloc(sizeof(Entry) * STORE_CAPACITY);
        storage->free_space_bitmap = (uint8_t*) ShmemAlloc(sizeof(uint8_t) * STORE_CAPACITY);
        storage->lock              = &(GetNamedLWLockTranche("shmem_storage_chunk"))->lock;

        p += MAXALIGN(sizeof(Storage));
		storage->raw_dsa_area = p;
		
        dsa = dsa_create_in_place(storage->raw_dsa_area, TEXT_STORE_MAX_SIZE, LWLockNewTrancheId(), 0);
		
        dsa_pin(dsa);
		dsa_set_size_limit(dsa, TEXT_STORE_MAX_SIZE);
         
        dsa_detach(dsa);

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

void attach_shmem(void)
{
	elog(NOTICE, "STEP 2: attach_shmem 1");  
    MemoryContext oldcontext;

	if (local_dsa)
		return;
    
    elog(NOTICE, "STEP 2: attach_shmem 2"); 
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	local_dsa = dsa_attach_in_place(storage->raw_dsa_area, NULL);
	dsa_pin_mapping(local_dsa);

	MemoryContextSwitchTo(oldcontext);
}

dsa_area *get_dsa_area_for_text(void)
{
	attach_shmem();
	return local_dsa;
}

void write_data_to_rel()
{
    size_t ret_arr_size = sizeof(int) * storage->store_capacity;
    
    int *ret            = (int*) palloc(ret_arr_size);

    if (!ret)
    {
        elog(WARNING, "Could not allocate memort for return codes array"); 
    }
    memset(ret, 0, ret_arr_size);
    
    SetCurrentStatementStartTimestamp();
    
    StartTransactionCommand();
    SPI_connect();
    PushActiveSnapshot(GetTransactionSnapshot());

    LWLockAcquire(storage->lock, LW_SHARED);
    dsa_area    *dsa = get_dsa_area_for_text();
    dsa_pointer  dsa_text_pointer;
	char	     *text = NULL;

    for(size_t i = 0; i < storage->store_capacity; i++)
    {
        if (storage->free_space_bitmap[i] == ALLOCATED)
        {
            StringInfoData buf;
            initStringInfo(&buf);
            
            text = dsa_get_address(dsa, storage->store[i].test_text.text_pos);
            appendStringInfo(&buf, "INSERT INTO %s (id, name) VALUES (%d, '%s')", TABLE_NAME, storage->store[i].id, text);
            ret[i] = SPI_execute(buf.data, false, 0);
            pfree(buf.data);
        }
    }

    LWLockRelease(storage->lock);

    PopActiveSnapshot();
    SPI_finish();
    CommitTransactionCommand();

    LWLockAcquire(storage->lock, LW_EXCLUSIVE);
    
    for (size_t i = 0; i < storage->store_capacity; i++)
    {
        // Удаляем строку из динамической разделяемой памяти и помечаем позиции в store как свободную.
        if (ret[i] == SPI_OK_INSERT)
        {
            dsa_text_pointer = (storage->store + i)->test_text.text_pos;
            if(DsaPointerIsValid(dsa_text_pointer))
                dsa_free(dsa, dsa_text_pointer);
            
            storage->free_space_bitmap[i] = FREE;
        }
    }

    LWLockRelease(storage->lock);
    
    pfree(ret); 
}

void worker_main(Datum main_arg)
{
    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    pqsignal(SIGTERM, die);
    BackgroundWorkerUnblockSignals();

    // Подумать как прокинуть OID db динамически
    BackgroundWorkerInitializeConnection("postgres", NULL, 0);

    // Передает право владения лэтчем из разделяемой памяти воркеру
    OwnLatch(latch);
    
    bool is_dsa_init = false;
    for (;;)
    {
        // Ожидание будет до сигнала от основного процесса
        (void) WaitLatch(latch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 1000, PG_WAIT_EXTENSION);
        ResetLatch(latch);

        CHECK_FOR_INTERRUPTS();

        if (ConfigReloadPending)
        {
            ConfigReloadPending = false;
            ProcessConfigFile(PGC_SIGHUP);
        }
        write_data_to_rel();
    }
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