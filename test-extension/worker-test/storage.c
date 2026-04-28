
#include "storage.h"

static int find_pos(Storage *storage)
{
    int res_pos = STORAGE_FULL;
    LWLockAcquire(storage->lock, LW_SHARED);
    for (size_t i = 0; i < storage->store_capacity; i++)
    {
        if (storage->free_space_bitmap[i] == FREE)
        {
            res_pos = (int) i;
            break;
        }
    }
    LWLockRelease(storage->lock);
    return res_pos;
}

static void add_el_internal(Entry *entry, size_t pos, Storage *storage, dsa_area *local_dsa)
{
    if (!entry)
    {
        return;
    }

    char	   *text_buff;
    dsa_pointer text_dsa_pointer;
    
    size_t      text_len;
    char       *text;
    
    elog(NOTICE, "STEP 1: get text from entry");  
    text     = entry->test_text.text_pointer;
    text_len = strlen(text);
    //elog(NOTICE, "text %s len %d", text, text_len); 

    LWLockAcquire(storage->lock, LW_EXCLUSIVE);

    elog(NOTICE, "STEP 2: init dsa area");  

    elog(NOTICE, "STEP 2: dsa_allocate_extended");  
    text_dsa_pointer = dsa_allocate_extended(local_dsa, text_len + 1,  DSA_ALLOC_ZERO);
    elog(NOTICE, "STEP 2: end"); 
    if (DsaPointerIsValid(text_dsa_pointer))
    {
        elog(NOTICE, "STEP 3: store text in dsa area");  
        text_buff = dsa_get_address(local_dsa, text_dsa_pointer);
        memcpy(text_buff, text, text_len);
        text_buff[text_len] = 0;
        entry->test_text.text_pos = text_dsa_pointer;
    } 
    elog(NOTICE, "STEP 4: copy entry struct into shared mem"); 

    //elog(NOTICE, "MEM INFO: cur mem %ld",  dsa_get_total_size(local_dsa)); 
    //elog(NOTICE, "TEST");  
    memcpy(storage->store + pos, entry, sizeof(Entry));       
    storage->free_space_bitmap[pos] = ALLOCATED;

    LWLockRelease(storage->lock);
}

bool add_el(Entry *entry, Storage *storage, dsa_area *local_dsa)
{
    if (!entry)
        return false;

    elog(NOTICE, "add_el NEW!");    
    
    int pos = find_pos(storage);
    elog(NOTICE, "pos %ld", pos);
    if (pos != STORAGE_FULL)
    {
        add_el_internal(entry, pos, storage, local_dsa);
        return true;
    }
    else
    {
        return false;
    }
}

void cleanup_storage(Storage *storage, dsa_area *local_dsa, int const *ret)
{
    dsa_pointer dsa_text_pointer;
    LWLockAcquire(storage->lock, LW_EXCLUSIVE);
    
    for (size_t i = 0; i < storage->store_capacity; i++)
    {
        // Удаляем строку из динамической разделяемой памяти и помечаем позиции в store как свободную.
        if (ret[i] == SPI_OK_INSERT)
        {
            dsa_text_pointer = (storage->store + i)->test_text.text_pos;
            if(DsaPointerIsValid(local_dsa))
                dsa_free(local_dsa, dsa_text_pointer);
            
            storage->free_space_bitmap[i] = FREE;
        }
    }

    LWLockRelease(storage->lock);

}