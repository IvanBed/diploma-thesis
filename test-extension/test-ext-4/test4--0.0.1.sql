CREATE FUNCTION get_counter_value() RETURNS integer
     AS '/var/lib/postgresql/test_ext_4/test4', 'get_counter_value'
     LANGUAGE C STRICT;


CREATE FUNCTION atomic_increment() RETURNS void
     AS '/var/lib/postgresql/test_ext_4/test4', 'atomic_increment'
     LANGUAGE C STRICT;


