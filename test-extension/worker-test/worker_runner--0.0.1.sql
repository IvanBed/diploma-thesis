CREATE TABLE IF NOT EXISTS test_table (id INTEGER, name TEXT);
CREATE INDEX test_table_name_idx ON test_table USING btree(name);

CREATE FUNCTION get_counter_value() RETURNS integer
     AS '/usr/lib/postgresql/16/lib/worker_runner', 'get_counter_value'
     LANGUAGE C STRICT;

