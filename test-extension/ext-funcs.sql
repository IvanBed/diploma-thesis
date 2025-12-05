CREATE FUNCTION get_int() RETURNS integer
     AS '/home/test/postgres-ext-test/test-ext', 'get_int'
     LANGUAGE C STRICT;

CREATE FUNCTION add_float() RETURNS double precision
     AS '/home/test/postgres-ext-test/test-ext', 'get_float'
     LANGUAGE C STRICT;
