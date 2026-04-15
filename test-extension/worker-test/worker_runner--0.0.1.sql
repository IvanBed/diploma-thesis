CREATE FUNCTION get_counter_value() RETURNS integer
     AS '/usr/lib/postgresql/16/lib/worker_runner', 'get_counter_value'
     LANGUAGE C STRICT;

