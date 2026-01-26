#include <postgres.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PASS_MIN_SIZE 10

#define HAS_LOWER	(1 << 0)  // 1
#define HAS_UPPER	(1 << 1)  // 2  
#define HAS_DIGIT	(1 << 2)  // 4
#define HAS_SPECIAL  (1 << 3)  // 8
#define PASS_COMPLEX_VAL	(HAS_LOWER | HAS_UPPER | HAS_DIGIT | HAS_SPECIAL) // 15


PG_MODULE_MAGIC;

static check_password_hook_type prev_check_password_hook = NULL;
static size_t * pass_min_size = NULL;

static void define_min_size_passwd()
{
	DefineCustomIntVariable("password_validation.min_pass_size", "Short description here", NULL, &pass_min_size, "", PGC_USERSET, 0, NULL, NULL, NULL); 
}

static bool password_validate(char const *password, size_t pass_min_size)
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

static void check_password(const char *username, const char *password, int password_type, Datum validuntil_time, bool validuntil_null)
{
	
	if (password_type == PASSWORD_TYPE_PLAINTEXT && !password_validate(password))
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("password is not valid")));
	
	else if (password_type == PASSWORD_TYPE_MD5) 
		ereport(ERROR,(errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("MD5 is not supported!")));
	
	PG_RETURN_VOID();
}

void _PG_init()
{
	define_min_size_passwd();
	if (pass_min_size == NULL)
		*pass_min_size = 10;
	
	if(!process_shared_preload_libraries_in_progress)
		elog(FATAL, "Please use shared_preload_libraries");
	prev_check_password_hook = check_password_hook;
	check_password_hook = check_password; 
}