#ifndef HEAP_FUNCS_H
    #define HEAP_FUNCS_H

    #include "postgres.h"
    #include "fmgr.h"

    #include "utils/rel.h"
    #include "utils/builtins.h"

    #include "storage/bufpage.h"
    #include "storage/bufmgr.h"

    #include "access/relation.h"
    #include "access/htup_details.h"
    
    #include <stdint.h>

    void get_rel_tuples_info(QueryDesc *);

#endif