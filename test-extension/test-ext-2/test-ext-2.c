#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include <string.h>
#include <stdlib.h>

#define NULLTERM 1

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(add_test_str);

Datum add_test_str(PG_FUNCTION_ARGS)
{
    char *arg_str = text_to_cstring(PG_GETARG_TEXT_PP(0));
    char *test_str = "test string";

    int first_offset = strlen(arg_str);
    int second_offset = strlen(test_str);
    int res_len = first_offset + second_offset + NULLTERM;

    char *new_str = (char *)palloc(res_len);
    if (new_str == NULL)
        PG_RETURN_NULL();

    memcpy(new_str, arg_str, first_offset);
    memcpy(new_str + first_offset, test_str, second_offset);
    new_str[res_len - 1] = '\0';

    text *res = cstring_to_text(new_str);
    
    pfree (new_str);
    PG_RETURN_TEXT_P(res);
}
