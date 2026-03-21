#!/bin/bash
PG_BIN_PATH="/usr/local/pgsql/bin"
PG_BIN_DATA="/usr/local/pgsql/data"
PG_LOG_PATH="/var/log/postgresql/logfile"
# Добавление библиотеки custom_executor

"$PG_BIN_PATH/psql" -c 'ALTER SYSTEM SET shared_preload_libraries = 'custom_executor';'

# Запуск postgres
"$PG_BIN_PATH/initdb" -D "$PG_BIN_DATA"
"$PG_BIN_PATH/pg_ctl" -D "$PG_BIN_DATA" -l "$PG_LOG_PATH" start
"$PG_BIN_PATH/createdb" test

# Добавление расширения custom_executor

"$PG_BIN_PATH/psql" -c 'CREATE EXTENSION custom_executor;'


"$PG_BIN_PATH/pg_ctl" -D "$PG_BIN_DATA" -l "$PG_LOG_PATH" restart

while true
do
    sleep 1
done
