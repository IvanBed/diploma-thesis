#include "postgres.h"
#include "fmgr.h"
#include <string.h>

//PG_MODULE_MAGIC_EXT("my_first_module", "0.1.0");

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(get_int);

Datum get_int(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT32(5);
}

PG_FUNCTION_INFO_V1(get_float);

Datum get_float(PG_FUNCTION_ARGS)
{
    PG_RETURN_FLOAT8(5.0);
}
