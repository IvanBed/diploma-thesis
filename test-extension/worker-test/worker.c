#include "worker.h"

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

static CounterData *counterData;
static Latch       *latch;

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

void request_shmem_counter_data()
{
    RequestAddinShmemSpace(MAXALIGN(sizeof(CounterData)));
    RequestNamedLWLockTranche("shmem_chunk", 1);
}

void init_counter_data_if_needed()
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

    request_shmem_shared_latch();
}

static void test_shmem_startup()
{
    if(prev_shmem_startup_hook)
        prev_shmem_startup_hook();
    
    init_counter_data_if_needed();

    init_shared_latch_if_needed();
}

void atomic_increment()
{

    LWLockAcquire(counterData->lock, LW_EXCLUSIVE);
    counterData->counter++;
    LWLockRelease(counterData->lock);
}

int32_t atomic_get_counter_value()
{
    int32_t result;

    LWLockAcquire(counterData->lock, LW_SHARED);
    result = counterData->counter;
    LWLockRelease(counterData->lock);
   
    return result;
}

void write_stats_to_table()
{
    int ret;
    StringInfoData buf;
    int32_t id = atomic_get_counter_value();
    
    SetCurrentStatementStartTimestamp();
    
    StartTransactionCommand();
    SPI_connect();
    PushActiveSnapshot(GetTransactionSnapshot());
    
    initStringInfo(&buf);
    appendStringInfo(&buf, "INSERT INTO %s (id, name) VALUES (%d, 'test')", TABLE_NAME, id);
    
    ret = SPI_execute(buf.data, false, 0);
    if (ret != SPI_OK_INSERT)
        elog(WARNING, "SPI_execute failed: error code %d", ret);
    
    PopActiveSnapshot();
    SPI_finish();
    CommitTransactionCommand();
    
    pfree(buf.data);
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

    size_t counter = 0;
    for (;;)
    {
        // Ожидание будет до сигнала от основного процесса
        (void) WaitLatch(latch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 10000000, PG_WAIT_EXTENSION);
        ResetLatch(latch);

        CHECK_FOR_INTERRUPTS();

        if (ConfigReloadPending)
        {
            ConfigReloadPending = false;
            ProcessConfigFile(PGC_SIGHUP);
        }
        
        atomic_increment();
        //write_stats_to_table();

        counter++;
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