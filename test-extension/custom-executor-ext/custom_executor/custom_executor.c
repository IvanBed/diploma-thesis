#include "postgres.h"
#include "fmgr.h"

#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/rel.h"

#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "commands/explain.h"
#include <access/xact.h>
#include "tcop/utility.h"
#include "storage/lock.h"
#include "datatype/timestamp.h"
//#include "executor/executor.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define EXECUTOR_START  0
#define EXECUTOR_FINISH 1

PG_MODULE_MAGIC;


struct LockInstanceDataWrapper
{
    LockData *lock_data;
    bool reinit_flag;
};

typedef struct LockInstanceDataWrapper LockInstanceDataWrapper;

static ExecutorStart_hook_type prev_ExecutorStart    = NULL;
static ExecutorRun_hook_type prev_ExecutorRun        = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish  = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd        = NULL;
static ProcessUtility_hook_type prev_ProcessUtility  = NULL;

static LockInstanceDataWrapper  *lock_data_wrapper   = NULL;

static void print_log_type_of_query(CmdType cur_type)
{
    switch(cur_type)
    {
        case CMD_SELECT:
            elog(NOTICE, "Type of query is SELECT");
            break;        
        case CMD_UPDATE:
            elog(NOTICE, "Type of query is UPDATE");
            break;
        case CMD_INSERT:
            elog(NOTICE, "Type of query is INSERT");        
            break;
        case CMD_DELETE:
            elog(NOTICE, "Type of query is DELETE");
            break;
        case CMD_MERGE:
            elog(NOTICE, "Type of query is MERGE");        
            break;
        case CMD_UTILITY:
            elog(NOTICE, "Type of query is UTILITY");
            break;
        case CMD_NOTHING:
            elog(NOTICE, "Type of query is NOTHING");    
            break;            
        default:
            elog(NOTICE, "Type of query is UNKNOWN");
            break;        
    }    
}

static void print_node_name(NodeTag tag)
{
    switch(tag)
    {
        case T_MergeJoinState:
            elog(NOTICE, "Node tag: MergeJoin");
            break;
        case T_SortState:
            elog(NOTICE, "Node tag: Sort");
            break;        
        case T_SeqScanState:
            elog(NOTICE, "Node tag: Seq Scan");
            break;  
        case T_NestLoopState:
            elog(NOTICE, "Node tag: Nest Loop");
            break;
        case T_HashJoinState:
            elog(NOTICE, "Node tag: Hash Join");
            break;                
        case T_AggState:
            elog(NOTICE, "Node tag: Aggregate");
            break;    
        case T_HashState:
            elog(NOTICE, "Node tag: Hash");
            break;
        case T_ValuesScanState:
            elog(NOTICE, "Node tag: Values Scan");
            break;
            
        case T_ModifyTableState:
            elog(NOTICE, "Node tag: Modify Table");
            break;
            
        default:
            elog(NOTICE, "Node tag: Unknown");
            break; 
    }
}

static void print_lock_tag(LockTagType locktag_type)
{
    switch (locktag_type)
    {
        case LOCKTAG_RELATION:
            elog(NOTICE, "LOCKTAG_RELATION");
            break;
        case LOCKTAG_RELATION_EXTEND:
            elog(NOTICE, "LOCKTAG_RELATION_EXTEND");
            break;
        case LOCKTAG_DATABASE_FROZEN_IDS:
            elog(NOTICE, "LOCKTAG_DATABASE_FROZEN_IDS");
            break;
        case LOCKTAG_PAGE:
            elog(NOTICE, "LOCKTAG_PAGE");
            break;
        case LOCKTAG_TUPLE:
            elog(NOTICE, "LOCKTAG_TUPLE");
            break;
        case LOCKTAG_TRANSACTION:
            elog(NOTICE, "LOCKTAG_TRANSACTION");
            break;
        case LOCKTAG_VIRTUALTRANSACTION:
            elog(NOTICE, "LOCKTAG_VIRTUALTRANSACTION");
            break;
        case LOCKTAG_SPECULATIVE_TOKEN:
            elog(NOTICE, "LOCKTAG_SPECULATIVE_TOKEN");
            break;
        case LOCKTAG_APPLY_TRANSACTION:
            elog(NOTICE, "LOCKTAG_APPLY_TRANSACTION");
            break;
        case LOCKTAG_OBJECT:
            elog(NOTICE, "LOCKTAG_OBJECT");
            break;
        case LOCKTAG_USERLOCK:
            elog(NOTICE, "LOCKTAG_USERLOCK");
            break;        
        case LOCKTAG_ADVISORY:
            elog(NOTICE, "LOCKTAG_ADVISORY");
            break;        
        default:            /* treat unknown locktags like OBJECT */
            elog(NOTICE, "UNKNOWN LOCKTAG");
            break;
    }
}

static void init_lock_info(QueryDesc *queryDesc)
{    
    lock_data_wrapper->lock_data = GetLockStatusData();
    LockInstanceData *instance   = NULL;
    
    if (lock_data_wrapper->reinit_flag)
    {
        for (size_t i = 0; i < lock_data_wrapper->lock_data->nelements; i++)
        {
            instance = &(lock_data_wrapper->lock_data->locks[i]);
            instance->waitStart = 0;
        }
        lock_data_wrapper->reinit_flag = false;
    }
}

static void print_lock_info(QueryDesc *queryDesc)
{
    LockInstanceData *instance  = NULL;
    bool              granted   = false; 
    LOCKMODE          mode      = 0;
    
    TimestampTz end_timestamp   = GetCurrentTimestamp();
    
    for (size_t i = 0; i < lock_data_wrapper->lock_data->nelements; i++)
    {
        instance = &(lock_data_wrapper->lock_data->locks[i]);
        
        if (instance->holdMask)
        {
            for (mode = 0; mode < MAX_LOCKMODES; mode++)
            {
                if (instance->holdMask & LOCKBIT_ON(mode))
                {
                    granted = true;
                    instance->holdMask &= LOCKBIT_OFF(mode);
                    break;
                }
            }
        }
        
        if (!granted)
        {
            if (instance->waitLockMode != NoLock)
            {
                mode = instance->waitLockMode;
            }
            else
            {
                continue;
            }
        }
        
        print_lock_tag((LockTagType) instance->locktag.locktag_type); 
        elog(NOTICE, "Proccess id: %d", instance->pid);
        elog(NOTICE, "Lock name: %s",GetLockmodeName(instance->locktag.locktag_lockmethodid, mode));
        elog(NOTICE, "database: %d",instance->locktag.locktag_field1);
        elog(NOTICE, "relation: %d",instance->locktag.locktag_field2);
        
        if (instance->waitStart != 0)
        {
            elog(NOTICE, "end_timestamp: %ld", end_timestamp);
            elog(NOTICE, "Lock time wait start: %ld", instance->waitStart);
            elog(NOTICE, "Lock time activity: %ld", end_timestamp - instance->waitStart);
            lock_data_wrapper->reinit_flag = true;
        }    
    }
    
}

static void dfs_plan_state(PlanState *node, int level)
{
    if (!node)
        return;

    elog(NOTICE, "Nesting level of plane node: %d", level);
    
    elog(NOTICE, "Node type: %d", node->type);
    print_node_name(node->type);

    Instrumentation *per_node_info  = node->instrument;

    if (per_node_info)
    {
        elog(NOTICE, "-------------------------Main info--------------------------------------------");
        //elog(NOTICE, "Total tuples emitted at the current node cycle: %f", per_node_info->tuplecount);
        elog(NOTICE, "Time spent at the current node cycle: %f seconds", INSTR_TIME_GET_DOUBLE(per_node_info->counter));
        elog(NOTICE, "-------------------------Buffer usage info------------------------------------");
        /*elog(NOTICE, "Time spent reading blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage_start.blk_read_time));
        elog(NOTICE, "Time spent writing blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage_start.blk_write_time));
        elog(NOTICE, "Time spent reading temp blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage_start.temp_blk_read_time));
        elog(NOTICE, "Time spent writing temp blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage_start.temp_blk_write_time));
        */elog(NOTICE, "-------------------------WAL usage info---------------------------------------");
        elog(NOTICE, "WAL records produced at the current node cycle: %ld", per_node_info->walusage_start.wal_records);
/*        elog(NOTICE, "-------------------------Add usage info---------------------------------------");    
        elog(NOTICE, "need_timer: %ld", per_node_info->need_timer);
        elog(NOTICE, "need_bufusage: %ld", per_node_info->need_bufusage);
        elog(NOTICE, "need_walusage: %ld", per_node_info->need_walusage);*/
    }
    else 
    {
        elog(NOTICE, "Instrumentation is not init");
    }

    level++;
    dfs_plan_state(node->lefttree, level);
    dfs_plan_state(node->righttree, level);
}


static void custom_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    queryDesc->instrument_options |= INSTRUMENT_ALL; 

    TransactionId xid = GetCurrentTransactionId();
    elog(NOTICE, "Transaction id: %d", xid);
    
    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else 
        standard_ExecutorStart(queryDesc, eflags);    
        
    if (queryDesc->totaltime == NULL)
    {
        MemoryContext oldcxt;
        oldcxt = MemoryContextSwitchTo(queryDesc->estate->es_query_cxt);
        queryDesc->totaltime = InstrAlloc(1, INSTRUMENT_ALL, false);
        MemoryContextSwitchTo(oldcxt);
    }
    
    elog(NOTICE, "LOCK INFO AT STEP Executor Start", queryDesc->totaltime->total);
    init_lock_info(queryDesc);
}

static void custom_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
{
    if (prev_ExecutorRun)
        prev_ExecutorRun(queryDesc, direction, count, execute_once);
    else 
        standard_ExecutorRun(queryDesc, direction, count, execute_once);
    
    print_log_type_of_query(queryDesc->operation);
}

static  void custom_ExecutorFinish(QueryDesc *queryDesc)
{
    if (prev_ExecutorFinish)
        prev_ExecutorFinish(queryDesc);
    else 
        standard_ExecutorFinish(queryDesc);
    
    InstrEndLoop(queryDesc->totaltime);
    
    if (queryDesc->totaltime)
    {
        elog(NOTICE, "Time spent to execute the query: %f seconds", queryDesc->totaltime->total);
    }
    
    dfs_plan_state(queryDesc->planstate, 0);
    elog(NOTICE, "LOCK INFO AT STEP ExecutorFinish", queryDesc->totaltime->total);
    print_lock_info(queryDesc);
    
}

static void custom_ExecutorEnd(QueryDesc *queryDesc)
{  
    if (prev_ExecutorEnd)
        prev_ExecutorEnd(queryDesc);
    else 
        standard_ExecutorEnd(queryDesc);
}

static void custom_ProcessUtility(PlannedStmt *pstmt, const char *queryString, bool readOnlyTree, ProcessUtilityContext context, ParamListInfo params, 
                    QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc)
{
    if (prev_ProcessUtility)
        prev_ProcessUtility(pstmt, queryString,readOnlyTree, context, params, queryEnv, dest, qc);
    else
        standard_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
    
    elog(NOTICE, "TEST custom_ProcessUtility %s", queryString);
    
}

static void init_lock_wrapper()
{
    lock_data_wrapper = (LockInstanceDataWrapper*) palloc(sizeof(LockInstanceDataWrapper));
    lock_data_wrapper->lock_data   = NULL;
    lock_data_wrapper->reinit_flag = false;
}

void _PG_init()
{
    
    if(!process_shared_preload_libraries_in_progress)
        elog(FATAL, "Please use shared_preload_libraries");
    
    init_lock_wrapper();
        
    prev_ExecutorStart = ExecutorStart_hook;
    ExecutorStart_hook = custom_ExecutorStart;

    prev_ExecutorRun = ExecutorRun_hook;
    ExecutorRun_hook = custom_ExecutorRun;

    prev_ExecutorFinish = ExecutorFinish_hook;
    ExecutorFinish_hook = custom_ExecutorFinish;

    prev_ExecutorEnd = ExecutorEnd_hook;
    ExecutorEnd_hook = custom_ExecutorEnd;
    
    prev_ProcessUtility = ProcessUtility_hook;
    ProcessUtility_hook = custom_ProcessUtility;
}
