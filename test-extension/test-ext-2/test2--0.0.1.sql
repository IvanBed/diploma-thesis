CREATE FUNCTION add_test_str(text) RETURNS text
     AS '/var/lib/postgresql/test_ext_2/test2', 'add_test_str'
     LANGUAGE C STRICT;

