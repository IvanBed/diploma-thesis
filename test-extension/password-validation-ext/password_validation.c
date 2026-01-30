#include "postgres.h"
#include "common/md5.h"
#include "fmgr.h"
#include "commands/user.h"
#include "access/heapam.h"
#include "miscadmin.h"

#include "utils/guc.h"
#include "utils/regproc.h"

#include <utils/builtins.h>
#include <utils/fmgroids.h>
#include <utils/rel.h>
#include <utils/snapmgr.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PASS_MIN_SIZE 10

#define HAS_LOWER    (1 << 0)  // 1
#define HAS_UPPER    (1 << 1)  // 2  
#define HAS_DIGIT    (1 << 2)  // 4
#define HAS_SPECIAL  (1 << 3)  // 8
#define PASS_COMPLEX_VAL    (HAS_LOWER | HAS_UPPER | HAS_DIGIT | HAS_SPECIAL) // 15

#define PASSWORD_STORE_TABLE_NAME "password_store"
#define PASSWORD_STORE_PKEY_SEQ_NAME "password_store_id_seq"

#define MD5_PASSWD_LEN    35

PG_MODULE_MAGIC;

static check_password_hook_type prev_check_password_hook = NULL;
static size_t  pass_min_size;

typedef struct FormData_password_store
{
    int32_t id; // int32_t -> SERIAL
    text password;
    
} FormData_password_store;

typedef FormData_password_store* FormData_password_store_ptr;

typedef enum Anum_password_store
{
    Anum_password_id = 1,
    Anum_password_val = 2,
    _Anum_password_max = 3,
} Anum_password_store;

#define cnt_attr_passwords (_Anum_password_max - 1)

static void define_min_size_passwd()
{
    DefineCustomIntVariable("password_validation.min_pass_size", "Short description here", NULL, &(pass_min_size), 8, 8, 256, PGC_USERSET, 0, NULL, NULL, NULL); 
}

static Oid name_to_oid(char const *name)
{
    return DatumGetObjectId(DirectFunctionCall1(to_regclass, CStringGetTextDatum(name)));
}

static bool plain_password_validate(char const *password)
{
    uint8_t flag = 0;
    uint8_t bit_ptr = 1;
    size_t pass_len = strlen(password);
    
    if (pass_len < pass_min_size) return false;
    
    for (size_t i = 0; i < pass_len; i++)
    {
        if ('a' <= password[i] && password[i] <= 'z')
        {
            flag = flag | bit_ptr;
        }
        else if ('A' <= password[i] && password[i] <= 'Z')
        {
            flag = flag | (bit_ptr << 1);
        }
        else if ('0' <= password[i] && password[i] <= '9')
        {
            flag = flag | (bit_ptr << 2);
        }
        else if ('#' <= password[i] && password[i] <= '/')
        {
            flag = flag | (bit_ptr << 3);
        }    
        if (i > 0 && password[i] == password[i - 1]) return false;
    }
    
    return flag == PASS_COMPLEX_VAL ? true : false;
}

static bool encrypted_password_validate(char const *crypt_password, int password_type)
{
    Relation rel;
    HeapTuple tup;
    TableScanDesc scan;
    
    bool encrypt_password_match = false;
    char const *logdetail = NULL;
    
    // Получаем oid реляции
    Oid tbl_oid = name_to_oid(PASSWORD_STORE_TABLE_NAME);
    
    // Открываем реляцию, ставим lock (read lock) и начинаем сканирование
    rel = table_open(tbl_oid, AccessShareLock);
    scan = table_beginscan(rel, GetTransactionSnapshot(), 0, NULL);

    while ((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
        
        char *plain_cur_password; 
        text *plain_cur_password_text;
        
        Datum values[cnt_attr_passwords];
        bool isnull[cnt_attr_passwords]; 
        char crypt_cur_password[MD5_PASSWD_LEN + 1];
        char *errstr = NULL;
        
        heap_deform_tuple(tup, RelationGetDescr(rel), values, isnull);
		
        plain_cur_password_text = DatumGetTextP(values[Anum_password_val - 1]);
        plain_cur_password = text_to_cstring(plain_cur_password_text);
        
        //elog(NOTICE, "Cur passwords %s", plain_cur_password);       
		// В силу того, что пользователь будет гененрить md5 пароль без соли, получаем чистый хеш
		// Думаю можно добавить вариант с солью по юзер
        if (!pg_md5_encrypt(plain_cur_password, NULL, 0, crypt_cur_password, &errstr))
        {
            logdetail = errstr;
            continue;
        }
        //elog(NOTICE, "Cur crypt passwords %s  ", crypt_cur_password);
		
        if(strcmp(crypt_cur_password, crypt_password) == 0)
        {
            encrypt_password_match = true;
            break;
        }
                     
    }
    //elog(NOTICE, "Crypt passwords %s  ", crypt_password);
    table_endscan(scan);
    table_close(rel, AccessShareLock);

    return !encrypt_password_match;
}

// Проверить const
//Возможно добавить вызов стандартного хука
static void check_password(char const *username, char const *password, int password_type, Datum validuntil_time, bool validuntil_null)
{
    if (password_type == PASSWORD_TYPE_PLAINTEXT)
    {
        if (!plain_password_validate(password))
            ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("password is not valid")));
    }
    else if (password_type == PASSWORD_TYPE_MD5) 
    {
        if (!encrypted_password_validate(password, PASSWORD_TYPE_MD5))
            ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("encrypted_password is not valid")));
    }
}

void _PG_init()
{
    define_min_size_passwd();
    
    if(!process_shared_preload_libraries_in_progress)
        elog(FATAL, "Please use shared_preload_libraries");

    prev_check_password_hook = check_password_hook;
    check_password_hook = check_password; 
}
