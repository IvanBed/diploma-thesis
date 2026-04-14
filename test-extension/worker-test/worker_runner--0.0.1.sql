CREATE FUNCTION get_counter_value() RETURNS integer
     AS '/var/lib/postgresql/worker/worker_runner', 'get_counter_value'
     LANGUAGE C STRICT;

