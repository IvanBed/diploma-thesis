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

#include "libpq/pqsignal.h"

#define N_ARGS 1

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

PG_MODULE_MAGIC;

typedef struct CounterData 
{
    LWLock* lock;
    int32_t counter;
} CounterData;

static CounterData *counterData;
static SPIPlanPtr savedPlanInsert = NULL;

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
    char *command  = "INSERT INTO public.test_table VALUES($1);";
    char *command1 = "INSERT INTO test_table VALUES(5);";
    Oid         argtypes[N_ARGS] = {INT4OID};
    Datum       values[N_ARGS];
    char        nulls[N_ARGS];

    //values[0]   = Int32GetDatum(atomic_get_counter_value());
    //nulls[0]    = ' ';
     
	
	SetCurrentStatementStartTimestamp();
	StartTransactionCommand();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());

    //SPI_execute(command1, false, 0);
    //SPI_execute_with_args(command, N_ARGS, argtypes, values, nulls, false, 1);
    /*if(savedPlanInsert == NULL)
    {
        savedPlanInsert = SPI_prepare("INSERT INTO test_table VALUES(5);", 0, NULL);
        
        if (savedPlanInsert == NULL)
            elog(ERROR, "Error preparing query");
        
        if (SPI_keepplan(savedPlanInsert))
            elog(ERROR, "Error keeping plan");
    }

    if (SPI_execute_plan(savedPlanInsert, NULL, NULL, true, 1) < 0 || SPI_processed != 1)
        elog(ERROR, "Failed to get current dict_id");
    */
	PopActiveSnapshot();
    SPI_finish();
	CommitTransactionCommand();   
}

void worker_main(Datum main_arg)
{
    CounterData  *data_ptr = DatumGetPointer(main_arg);

    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    pqsignal(SIGTERM, die);
    BackgroundWorkerUnblockSignals();

    // Подумать как прокинуть OID db динамически
    BackgroundWorkerInitializeConnection("postgres", NULL, 0);
    //write_stats_to_table();

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

        //if (counter % 10000 == 0)
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