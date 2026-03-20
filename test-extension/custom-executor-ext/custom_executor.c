#include "postgres.h"
#include "fmgr.h"

#include "utils/builtins.h"
#include "utils/guc.h"

#include "miscadmin.h"

#include "nodes/nodetags.h"
#include "nodes/pg_list.h"

#include "commands/explain.h"
#include "executor/executor.h"

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

static void custom_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else 
        standard_ExecutorStart(queryDesc, eflags);    
    
    //elog(NOTICE, "custom_ExecutorStart is started, level of subquery is: %d", level);
}

static void custom_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
{
    if (prev_ExecutorRun)
        prev_ExecutorRun(queryDesc, direction, count, execute_once);
    else 
        standard_ExecutorRun(queryDesc, direction, count, execute_once);
    
    //elog(NOTICE, "custom_ExecutorRun is started, level of subquery is: %d", level);
    //elog(NOTICE, "Query text: %s", (char *) queryDesc->sourceText);
    print_log_type_of_query(queryDesc->operation);
    level++;
}

static  void custom_ExecutorFinish(QueryDesc *queryDesc)
{
    if (prev_ExecutorFinish)
        prev_ExecutorFinish(queryDesc);
    else 
        standard_ExecutorFinish(queryDesc);
    
    //elog(NOTICE, "custom_ExecutorFinish is started, level of subquery is: %d", level);
    //elog(NOTICE, "Query text: %s", (char *) queryDesc->sourceText);
    level++;
}

static void custom_ExecutorEnd(QueryDesc *queryDesc)
{
    if (prev_ExecutorEnd)
        prev_ExecutorEnd(queryDesc);
    else 
        standard_ExecutorEnd(queryDesc);
    
    elog(NOTICE, "custom_ExecutorEnd is started, level of subquery is: %d", level);
}

static void custom_per_node_hook(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, ExplainState *es)
{
    
    if (prev_explain_per_node_hook)
        prev_explain_per_node_hook(planstate, ancestors, relationship,plan_name, es);

    
    elog(NOTICE, "custom_per_node_hook is started, current plan name is %s", plan_name);
    elog(NOTICE, "custom_per_node_hook is started, string info is %s", es->str);
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
