#ifndef STORAGE_H
#define STORAGE_H

    #define TABLE_NAME "test_table"

    #define FREE 0 
    #define ALLOCATED 1
    #define STORAGE_FULL -1

    #define STORE_CAPACITY 25
    #define TEXT_STORE_MAX_SIZE 1024 * 1024


    typedef struct Entry
    {
        int32_t id;
        union
      {
        dsa_pointer text_pos;  /* text location within text buffer */
        char     *text_pointer;
       }  test_text;
    } Entry;

    typedef struct Storage 
    {
        LWLock       *lock;
        size_t        store_capacity;
        Entry        *store;
        uint8_t      *free_space_bitmap;
        
        void         *raw_dsa_area;
        
        MemoryContext storage_mem_cxt;
    } Storage;

#endif