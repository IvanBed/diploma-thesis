#include "postgres.h"
#include "fmgr.h"

#include "utils/builtins.h"
#include "utils/guc.h"

#include "miscadmin.h"


#include "nodes/pg_list.h"

#include "commands/explain.h"
//#include "executor/executor.h"

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

PG_MODULE_MAGIC;

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorRun_hook_type prev_ExecutorRun = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;
static explain_per_node_hook_type prev_explain_per_node_hook = NULL;

static int level = 0;
static int plan_level = 0;


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
        default:
            elog(NOTICE, "Node tag: Unknown");
            break; 
    }
}

static void dfs_plan_state(PlanState *node, int level)
{
    if (!node)
        return;

    elog(NOTICE, "level: %d", level);
     
    print_node_name(node->type);

    Instrumentation *cur_instr  = node->instrument;

    if (cur_instr)
    {
        elog(NOTICE, "Total tuples emitted so far the current node cycle: %f", cur_instr->tuplecount);
        elog(NOTICE, "Nanoseconds spent on the current node cycle: %ld", INSTR_TIME_GET_NANOSEC(cur_instr->counter));
    }
    else 
    {
        elog(NOTICE, "Instrumentation is not init");
    }

    EState *cur_estatr  = node->state;
    if (cur_estatr)
        elog(NOTICE, "Source row: %s", cur_estatr->es_sourceText);
    else
        elog(NOTICE, "EState is NULL");

    level++;
    dfs_plan_state(node->lefttree, level);
    dfs_plan_state(node->righttree, level);
}


static void custom_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    queryDesc->instrument_options |= INSTRUMENT_TIMER; 
    queryDesc->instrument_options |= INSTRUMENT_BUFFERS; 
    queryDesc->instrument_options |= INSTRUMENT_ROWS; 
    queryDesc->instrument_options |= INSTRUMENT_WAL; 

    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else 
        standard_ExecutorStart(queryDesc, eflags);    
    
}

static void custom_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count)
{
    if (prev_ExecutorRun)
        prev_ExecutorRun(queryDesc, direction, count, execute_once);
    else 
        standard_ExecutorRun(queryDesc, direction, count, execute_once);
    
    print_log_type_of_query(queryDesc->operation);
    level++;
}

static  void custom_ExecutorFinish(QueryDesc *queryDesc)
{
    if (prev_ExecutorFinish)
        prev_ExecutorFinish(queryDesc);
    else 
        standard_ExecutorFinish(queryDesc);
    
    dfs_plan_state(queryDesc->planstate, 0;
    level++;
}

static void custom_ExecutorEnd(QueryDesc *queryDesc)
{
    if (prev_ExecutorEnd)
        prev_ExecutorEnd(queryDesc);
    else 
        standard_ExecutorEnd(queryDesc);


}

static void custom_per_node_hook(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, struct ExplainState *es)
{
    plan_level++;
    if (prev_explain_per_node_hook)
        prev_explain_per_node_hook(planstate, ancestors, relationship,plan_name, es);

    //elog(NOTICE, "custom_per_node_hook is started, current plan name is %s, plan level is %d", plan_name, plan_level);
    //elog(NOTICE, "custom_per_node_hook is started, string info is %s", es->str);
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

    prev_explain_per_node_hook = explain_per_node_hook;
    explain_per_node_hook = custom_per_node_hook;

}
