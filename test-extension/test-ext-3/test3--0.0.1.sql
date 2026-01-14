/*
         Заменить захардкоженые пути на переменную
*/

CREATE TYPE __retcomposite AS (f1 integer, f2 integer, f3 integer);

CREATE OR REPLACE FUNCTION retcomposite(integer, integer)
    RETURNS SETOF __retcomposite
    AS '/var/lib/postgresql/test_ext_3/test3', 'retcomposite'
    LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION make_array(anyelement, integer, integer) RETURNS anyarray
    AS '/var/lib/postgresql/test_ext_3/test3', 'make_array'
    LANGUAGE C IMMUTABLE;