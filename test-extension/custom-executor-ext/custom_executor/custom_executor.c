#include "postgres.h"
#include "fmgr.h"
#include "pgstat.h"

#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/rel.h"

#include "storage/bufpage.h"
#include "storage/lock.h"
#include "storage/bufmgr.h"

#include "access/xact.h"
#include "access/relation.h"
#include "access/htup_details.h"

#include "miscadmin.h"
#include "nodes/pg_list.h"
#include "commands/explain.h"

#include "tcop/utility.h"
#include "datatype/timestamp.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define EXECUTOR_START  0
#define EXECUTOR_FINISH 1

PG_MODULE_MAGIC;

struct LockInstanceDataStorage
{
    LockData *current_lock_data;
    List     *prev_lock_data;
};

typedef struct LockInstanceDataStorage LockInstanceDataStorage;

static ExecutorStart_hook_type prev_ExecutorStart    = NULL;
static ExecutorRun_hook_type prev_ExecutorRun        = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish  = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd        = NULL;
static ProcessUtility_hook_type prev_ProcessUtility  = NULL;

static LockInstanceDataStorage  *lock_storage = NULL;

static void create_lock_storage()
{
    lock_storage = (LockInstanceDataStorage*) palloc(sizeof(LockInstanceDataStorage));
    
    lock_storage->current_lock_data = NULL;
    lock_storage->prev_lock_data    = NULL;
}

static void free_lock_data(LockData *lock_data)
{
    if (lock_data)
    {
        if (lock_data->locks)
        {
            pfree(lock_data->locks);
        }
        pfree(lock_data);
    }
}

static void destroy_lock_storage()
{
    if (lock_storage)
    {
        if (lock_storage->current_lock_data)
            free_lock_data(lock_storage->current_lock_data);
        
        if (lock_storage->prev_lock_data)
            list_free(lock_storage->prev_lock_data);
       
        pfree(lock_storage);
    } 
}

static void init_lock_storage()
{    
    if (!lock_storage->current_lock_data && !lock_storage->prev_lock_data)
    {
        elog(INFO, "init_lock_storage\n First case");
        size_t temp_wait_start          = 0;

        lock_storage->current_lock_data = GetLockStatusData();
        LockInstanceData     *instance  = NULL;
        
        for (size_t i = 0; i < lock_storage->current_lock_data->nelements; i++)
        {
            instance = &(lock_storage->current_lock_data->locks[i]);
            temp_wait_start = instance->waitStart;
            instance->waitStart = 0;
            lock_storage->prev_lock_data = lappend(instance, lock_storage->prev_lock_data);
            instance->waitStart = temp_wait_start;
        }
    }
    else
    {
        elog(INFO, "init_lock_storage\n Second case");
        LockData *temp_data = GetLockStatusData();
        elog(NOTICE, "temp_data pointer %ld", temp_data);
        lock_storage->current_lock_data = temp_data;        
    } 

    elog(NOTICE, "current_lock_data pointer %ld", lock_storage->current_lock_data);
    elog(NOTICE, "prev_lock_data pointer %ld", lock_storage->prev_lock_data);

    elog(NOTICE, "current_lock_data size %ld", lock_storage->current_lock_data->nelements);
    elog(NOTICE, "prev_lock_data size %ld", lock_storage->prev_lock_data->nelements);    
}

static bool lock_data_inst_compare(LockInstanceData *l_instance, LockInstanceData *r_instance)
{
    if (l_instance->locktag.locktag_type != r_instance->locktag.locktag_type)
        return false;
    
    switch(l_instance->locktag.locktag_type)
    {
        case LOCKTAG_RELATION: case LOCKTAG_RELATION_EXTEND: case LOCKTAG_DATABASE_FROZEN_IDS:
            if (l_instance->locktag.locktag_field1 == r_instance->locktag.locktag_field1 && l_instance->locktag.locktag_field2 == r_instance->locktag.locktag_field2)
                return true;
            else
                return false;
        
        case LOCKTAG_VIRTUALTRANSACTION: case LOCKTAG_TRANSACTION: case LOCKTAG_SPECULATIVE_TOKEN:
            if (l_instance->locktag.locktag_field1 == r_instance->locktag.locktag_field1)
                return true;
            else
                return false;        

        case LOCKTAG_PAGE:
            if (l_instance->locktag.locktag_field1 == r_instance->locktag.locktag_field1 
                && l_instance->locktag.locktag_field2 == r_instance->locktag.locktag_field2 
                    && l_instance->locktag.locktag_field3 == r_instance->locktag.locktag_field3)
                return true;
            else
                return false; 
        
        case LOCKTAG_TUPLE: case LOCKTAG_OBJECT: case LOCKTAG_ADVISORY: case LOCKTAG_APPLY_TRANSACTION:
            if (l_instance->locktag.locktag_field1 == r_instance->locktag.locktag_field1 
                && l_instance->locktag.locktag_field2 == r_instance->locktag.locktag_field2 
                    && l_instance->locktag.locktag_field3 == r_instance->locktag.locktag_field3
                        &&l_instance->locktag.locktag_field4 == r_instance->locktag.locktag_field4)
                return true;
            else
                return false; 

        default:
            return false;
    }
}

static void update_value(List *lock_data, LockInstanceData *prev_instance)
{
    LockInstanceData * cur_instance = NULL;
    for (size_t i = 0; i < lock_data->nelements; i++)
    {
        cur_instance = &(lock_data->locks[i]);
        if (lock_data_inst_compare(prev_instance, cur_instance))
        {
            prev_instance->waitStart = cur_instance->waitStart;
            return;
        }
    }

    
}

static void update_lock_storage()
{
    LockInstanceData *instance = NULL;
    ListCell	     *list_cell     = NULL; 

    foreach(list_cell, lock_storage->prev_lock_data)
    {
        instance = (LockInstanceData *) lfirst(list_cell);
        update_value(lock_storage->current_lock_data, instance);
    }
}

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

void print_rel_info(Relation rel)
{
    if (rel->pgstat_info)
    {
        elog(INFO, "Tuples that have been updated: %ld", rel->pgstat_info->counts.tuples_updated);
        elog(INFO, "Tuples that have been moved to a new page: %ld", rel->pgstat_info->counts.tuples_newpage_updated);
        elog(INFO, "Tuples that have been hot updated: %ld", rel->pgstat_info->counts.tuples_hot_updated);
    }
    else 
    {
        elog(INFO, "Relation info structure is NULL");
    }
}

void print_query_rels_info(QueryDesc *queryDesc)
{
    EState     *query_state   = queryDesc->estate;
    Relation   *rels_arr       = query_state->es_relations;
    for (size_t rel_idx = 0; rel_idx < query_state->es_range_table_size; rel_idx++)
    {
        Relation cur_rel = rels_arr[rel_idx];
        if (cur_rel)
            print_rel_info(cur_rel);
    }
}

static void print_lock_info()
{
    LockInstanceData *cur_instance   = NULL;
    LockInstanceData *prev_instance  = NULL;

    bool              granted   = false; 
    LOCKMODE          mode      = 0;
    

    TimestampTz end_timestamp   = GetCurrentTimestamp();
    
    for (size_t i = 0; i < lock_storage->current_lock_data->nelements; i++)
    {
        cur_instance  = &(lock_storage->current_lock_data->locks[i]);
        prev_instance = &(lock_storage->prev_lock_data->locks[i]);
        if (cur_instance->holdMask)
        {
            for (mode = 0; mode < MAX_LOCKMODES; mode++)
            {
                if (cur_instance->holdMask & LOCKBIT_ON(mode))
                {
                    granted = true;
                    cur_instance->holdMask &= LOCKBIT_OFF(mode);
                    break;
                }
            }
        }
        
        if (!granted)
        {
            elog(NOTICE, "NOT GRANTED!");
            if (cur_instance->waitLockMode != NoLock)
            {
                mode = cur_instance->waitLockMode;
            }
            else
            {
                continue;
            }
        }
        
        print_lock_tag((LockTagType) cur_instance->locktag.locktag_type); 
        elog(INFO, "Proccess id: %d", cur_instance->pid);
        elog(INFO, "Lock name: %s", GetLockmodeName(cur_instance->locktag.locktag_lockmethodid, mode));
        elog(INFO, "database: %d",cur_instance->locktag.locktag_field1);
        elog(INFO, "relation: %d",cur_instance->locktag.locktag_field2);
        
        elog(INFO, "Lock time wait start: %ld", cur_instance->waitStart);
        if (cur_instance->waitStart != 0)
            elog(INFO, "Lock wait time activity: %f seconds", TimestampDifferenceMilliseconds(cur_instance->waitStart, end_timestamp) / 1000.0);
        else 
            elog(INFO, "Lock  wait time activity: %ld", 0);
    }
}

void print_instr_info(Instrumentation *per_node_info)
{
    elog(INFO, "-------------------------Main info--------------------------------------------");
    elog(INFO, "Total tuples emitted at the current node cycle: %f", per_node_info->tuplecount);
    elog(INFO, "Time spent at the current node cycle: %f seconds", INSTR_TIME_GET_DOUBLE(per_node_info->counter));
    elog(INFO, "Time spent at the current node№2 cycle: %f seconds", per_node_info->total);
        
    elog(INFO, "need_timer: %d", per_node_info->need_timer);
    elog(INFO, "need_bufusage: %d", per_node_info->need_bufusage);
    elog(INFO, "need_walusage: %d", per_node_info->need_walusage);
    elog(INFO, "-------------------------Buffer usage info------------------------------------");
    elog(INFO, "Time spent reading blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage.blk_read_time));
    elog(INFO, "Time spent writing blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage.blk_write_time));
    elog(INFO, "Time spent reading temp blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage.temp_blk_read_time));
    elog(INFO, "Time spent writing temp blocks at the current node cycle: %ld nanoseconds", INSTR_TIME_GET_NANOSEC(per_node_info->bufusage.temp_blk_write_time));
        
    elog(INFO, "Local block hits at the current node cycle: %ld", per_node_info->bufusage.local_blks_hit);
    elog(INFO, "Local block reads at the current node cycle: %ld", per_node_info->bufusage.local_blks_read);
    elog(INFO, "Local block dirtied at the current node cycle: %ld", per_node_info->bufusage.local_blks_dirtied);
    elog(INFO, "Local block written at the current node cycle: %ld", per_node_info->bufusage.local_blks_written);    

    elog(INFO, "Shared block hits at the current node cycle: %ld", per_node_info->bufusage.shared_blks_hit - per_node_info->bufusage_start.shared_blks_hit);
    elog(INFO, "Shared block reads at the current node cycle: %ld", per_node_info->bufusage.shared_blks_read  - per_node_info->bufusage_start.shared_blks_read);
    elog(INFO, "Shared block dirtied at the current node cycle: %ld", per_node_info->bufusage.shared_blks_dirtied - per_node_info->bufusage_start.shared_blks_dirtied);
    elog(INFO, "Shared block written at the current node cycle: %ld", per_node_info->bufusage.shared_blks_written - per_node_info->bufusage_start.shared_blks_written);    
        
    elog(INFO, "Shared block hits at the current node cycle: %ld", per_node_info->bufusage.shared_blks_hit);
    elog(INFO, "Shared block reads at the current node cycle: %ld", per_node_info->bufusage.shared_blks_read);
    elog(INFO, "Shared block dirtied at the current node cycle: %ld", per_node_info->bufusage.shared_blks_dirtied);
    elog(INFO, "Shared block written at the current node cycle: %ld", per_node_info->bufusage.shared_blks_written);    

    elog(INFO, "-------------------------WAL PER NODE usage info---------------------------------------");
    elog(INFO, "WAL records produced at the current node cycle: %ld", per_node_info->walusage_start.wal_records);
    elog(INFO, "WAL full page images produced at the current node cycle: %ld", per_node_info->walusage_start.wal_fpi);
    elog(INFO, "Size of WAL records produced at the current node cycle: %ld", per_node_info->walusage_start.wal_bytes); 
}

/*static void bfs_plan_state(PlanState *node)
{
    List      *plan_state_queue = NULL;
    PlanState *cur_node         = NULL;
    
    plan_state_queue = lcons(node, plan_state_queue);
    while(list_length(plan_state_queue) > 0)
    {
        cur_node = list_head(plan_state_queue);
        print_node_name(node->type);
        if (node->instrument)
        {
            print_instr_info(node->instrument);
        }
        else 
        {
            elog(INFO, "Instrumentation is not init");
        }
        
        plan_state_queue = lappend(cur_node->lefttree, plan_state_queue);
        plan_state_queue = lappend(cur_node->righttree, plan_state_queue);
        list_delete_first(plan_state_queue);
    }
}
*/
static void dfs_plan_state(PlanState *node, int level)
{
    if (!node)
        return;

    //elog(INFO, "Nesting level of plane node: %d", level);
    
    elog(INFO, "Node type: %d", node->type);
    print_node_name(node->type);

    Instrumentation *per_node_info  = node->instrument;

    if (per_node_info)
    {
        print_instr_info(per_node_info);
    }
    else 
    {
        elog(INFO, "Instrumentation is not init");
    }

    level++;
    dfs_plan_state(node->lefttree, level);
    dfs_plan_state(node->righttree, level);
}

static void custom_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    init_lock_storage();
    
    queryDesc->instrument_options |= INSTRUMENT_TIMER; 
    queryDesc->instrument_options |= INSTRUMENT_BUFFERS;
    queryDesc->instrument_options |= INSTRUMENT_ROWS;
    queryDesc->instrument_options |= INSTRUMENT_WAL;
    
    TransactionId xid = GetCurrentTransactionId();
    elog(INFO, "Transaction id: %d", xid);
    
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
     
    elog(INFO, "LOCK INFO AT STEP Executor Start", queryDesc->totaltime->total);
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
    
    /*if (queryDesc->totaltime)
    {
        elog(INFO, "Time spent to execute the query: %f seconds", queryDesc->totaltime->total);
        elog(INFO, "-------------------------WAL PER Query usage info---------------------------------------");
        elog(INFO, "WAL records produced at the current node cycle: %ld", queryDesc->totaltime->walusage.wal_records);
        elog(INFO, "WAL full page images produced at the current node cycle: %ld", queryDesc->totaltime->walusage.wal_fpi);
        elog(INFO, "Size of WAL records produced at the current node cycle: %ld", queryDesc->totaltime->walusage.wal_bytes);
        elog(INFO, "-------------------------Buffer PER Query usage info---------------------------------------");
        elog(INFO, "Total local block hits at the current node cycle: %ld", queryDesc->totaltime->bufusage.local_blks_hit);
        elog(INFO, "Total local block reads at the current node cycle: %ld", queryDesc->totaltime->bufusage.local_blks_read);
        elog(INFO, "Total local block dirtied at the current node cycle: %ld", queryDesc->totaltime->bufusage.local_blks_dirtied);
        elog(INFO, "Total local block written at the current node cycle: %ld", queryDesc->totaltime->bufusage.local_blks_written);
    }*/
    
    //dfs_plan_state(queryDesc->planstate, 0);
    //elog(INFO, "LOCK INFO AT STEP ExecutorFinish", queryDesc->totaltime->total);
    print_lock_info();
    
}

static void custom_ExecutorEnd(QueryDesc *queryDesc)
{  
    update_lock_storage();
    //print_query_rels_info(queryDesc);
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
}

void _PG_init()
{
    if(!process_shared_preload_libraries_in_progress)
        elog(FATAL, "Please use shared_preload_libraries");
    
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

    create_lock_storage();
}
