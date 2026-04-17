CREATE TABLE IF NOT EXISTS test_table (id INTEGER, name TEXT);
/*CREATE INDEX test_table_name_idx ON test_table USING btree(name);*/

CREATE FUNCTION get_counter_value() RETURNS integer
     AS '/usr/lib/postgresql/16/lib/worker_runner', 'get_counter_value'
     LANGUAGE C STRICT;

CREATE FUNCTION set_store_entry(id INTEGER, name TEXT) RETURNS void
     AS '/usr/lib/postgresql/16/lib/worker_runner', 'set_store_entry'
     LANGUAGE C STRICT;

CREATE FUNCTION log_print() RETURNS void
     AS '/usr/lib/postgresql/16/lib/worker_runner', 'log_print'
     LANGUAGE C STRICT;

     