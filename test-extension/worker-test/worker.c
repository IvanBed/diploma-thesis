#include <postgres.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/shmem.h>
#include <storage/lwlock.h>
#include <utils/builtins.h>


typedef struct CounterData 
{
    LWLock* lock;
    int32_t counter;
} CounterData;

void atomic_increment(CounterData *data)
{
   
  LWLockAcquire(data->lock, LW_EXCLUSIVE);
  data->counter++;
  LWLockRelease(data->lock);

  PG_RETURN_VOID();
}


void do_work(CounterData *data)
{
    atomic_increment(data);
}

void worker_spi_main(Datum main_arg)
{

    CounterData  *data_ptr = DatumGetPointer(main_arg);

    pqsignal(SIGHUP, SignalHandlerForConfigReload);
    pqsignal(SIGTERM, die);
    BackgroundWorkerUnblockSignals();

    // Подумать как прокинуть OID db динамически
    BackgroundWorkerInitializeConnection("postgres", NULL, 0);

    
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
        
        do_work(data_ptr);
        
    }
}

