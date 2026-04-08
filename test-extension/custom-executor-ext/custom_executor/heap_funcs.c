#include "heap_funcs.h"

static bytea *get_raw_page(Oid relationid, ForkNumber forknum, BlockNumber blkno)
{
    bytea      *raw_page;
    Relation    rel;
    char       *raw_page_data;
    Buffer      buf;

    rel  = relation_open(relationid, AccessShareLock);

    if (!RELKIND_HAS_STORAGE(rel->rd_rel->relkind))
        ereport(ERROR,
                (errcode(ERRCODE_WRONG_OBJECT_TYPE),
                 errmsg("cannot get raw page from relation \"%s\"",
                        RelationGetRelationName(rel)),
                 errdetail_relkind_not_supported(rel->rd_rel->relkind)));

    if (RELATION_IS_OTHER_TEMP(rel))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("cannot access temporary tables of other sessions")));

    if (blkno >= RelationGetNumberOfBlocksInFork(rel, forknum))
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("block number %u is out of range for relation \"%s\"",
                        blkno, RelationGetRelationName(rel))));

    raw_page = (bytea *) palloc(BLCKSZ + VARHDRSZ);
    SET_VARSIZE(raw_page, BLCKSZ + VARHDRSZ);
    raw_page_data = VARDATA(raw_page);

    buf = ReadBufferExtended(rel, forknum, blkno, RBM_NORMAL, NULL);
    LockBuffer(buf, BUFFER_LOCK_SHARE);

    memcpy(raw_page_data, BufferGetPage(buf), BLCKSZ);

    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    ReleaseBuffer(buf);

    relation_close(rel, AccessShareLock);
    return raw_page;
}

static Page get_page_from_raw(bytea *raw_page)
{
    Page        page;
    int         raw_page_size;

    raw_page_size = VARSIZE_ANY_EXHDR(raw_page);

    if (raw_page_size != BLCKSZ)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid page size"), errdetail("Expected %d bytes, got %d.", BLCKSZ, raw_page_size)));

    page = palloc(raw_page_size);

    memcpy(page, VARDATA_ANY(raw_page), raw_page_size);
    return page;
}

static size_t get_tuples_new_page(Page *page, BlockNumber blk_idx)
{
    OffsetNumber item_data_size = PageGetMaxOffsetNumber(page);
    
    ItemId           id;
    HeapTupleHeader  tuphdr;
    ItemPointerData  tid;
    uint16           lp_offset;
    uint16           lp_flags;
    uint16           lp_len;
    BlockNumber      block_num;

    size_t tuples_new_page = 0;

    for (OffsetNumber offset = FirstOffsetNumber; offset <= item_data_size; offset++)
    {
        id        = PageGetItemId(page, offset);
        lp_offset = ItemIdGetOffset(id);
        lp_flags  = ItemIdGetFlags(id);
        lp_len    = ItemIdGetLength(id);
        
        if (ItemIdHasStorage(id) && lp_len >= MinHeapTupleSize &&
            lp_offset == MAXALIGN(lp_offset) && lp_offset + lp_len <= BLCKSZ)
        {
        
            tuphdr = (HeapTupleHeader) PageGetItem(page, id);
            
            tid = tuphdr->t_ctid;
            
            block_num = ItemPointerGetBlockNumber(&tid); 
            if (block_num != blk_idx)
                tuples_new_page++;
        }     
    } 
    return tuples_new_page;
}

static size_t get_page_tuples_info(Relation rel, BlockNumber blk_idx)
{
    bytea      *raw_page      = NULL;
    Page       *cur_page      = NULL;
    Oid         rel_oid       = rel->rd_id;
    
    size_t tuples_new_page    = 0;

    raw_page = get_raw_page(rel_oid, MAIN_FORKNUM, blk_idx);
    if (raw_page)
    {
        cur_page = get_page_from_raw(raw_page);
        if (cur_page)
        {
            tuples_new_page = get_tuples_new_page(cur_page, blk_idx);
            pfree(cur_page);
        }
        pfree(raw_page);
    }
    return tuples_new_page;
}

void get_rel_tuples_info(QueryDesc *queryDesc)
{
    EState     *query_state   = queryDesc->estate;
    Relation   *rel_arr       = query_state->es_relations;
    size_t      total_tuples_new_page = 0;
    for (size_t rel_idx = 0; rel_idx < query_state->es_range_table_size; rel_idx++)
    {
        Relation cur_rel = query_state->es_relations[rel_idx];
        if (cur_rel)
        {
            BlockNumber total_blck_cnt = RelationGetNumberOfBlocks(cur_rel);
            for (BlockNumber blk_idx = 0; blk_idx < total_blck_cnt; blk_idx++)
                total_tuples_new_page += get_page_tuples_info(cur_rel, blk_idx);
        }            
    }
    elog(INFO, "Tuples that have been moved to a new page(CUSTOM CALC): %d", total_tuples_new_page);
}