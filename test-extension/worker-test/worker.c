#include "worker.h"

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

static CounterData *counterData;

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

void do_work()
{
    atomic_increment();
}

void write_stats_to_table()
{
	Relation rel;
	HeapTuple tup;
	Datum values[Natts_test_table];
	bool nulls[Natts_test_table];

    int32_t id = atomic_get_counter_value();
    text name  = "test";

    Oid tbl_oid = name_to_oid(PHONEBOOK_TABLE_NAME);

    memset(nulls, false, sizeof(nulls));

    rel = table_open(tbl_oid, RowExclusiveLock);
    values[Anum_test_table_id - 1]   = Int32GetDatum(next_id);
	values[Anum_test_table_name - 1] = NameGetDatum(name);

    tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);
    CatalogTupleInsert(rel, tup);
    heap_freetuple(tup);

    table_close(rel, RowExclusiveLock);
}

void worker_main(Datum main_arg)
{
    CounterData  *data_ptr = DatumGetPointer(main_arg);

    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    pqsignal(SIGTERM, die);
    BackgroundWorkerUnblockSignals();

    // Подумать как прокинуть OID db динамически
    BackgroundWorkerInitializeConnection("postgres", NULL, 0);

    size_t counter = 0;
    for (;;)
    {
        // Ожидание будет до сигнала от основного процесса
        (void) WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 1000, PG_WAIT_EXTENSION);
        ResetLatch(MyLatch);

        CHECK_FOR_INTERRUPTS();

        if (ConfigReloadPending)
        {
            ConfigReloadPending = false;
            ProcessConfigFile(PGC_SIGHUP);
        }
        
        do_work(); 

        write_stats_to_table();
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