PG_INCLUDE_DIR=/usr/include/postgresql/16/server/

all: worker worker_runner

worker_runner:
	EXTENSION = worker_runner
	MODULE_big = worker_runner
	DATA = worker_runner--0.0.1.sql
	OBJS = worker_runner.o
	PG_CONFIG = pg_config
	PGXS := $(shell $(PG_CONFIG) --pgxs)
	include $(PGXS)

worker:
	gcc -fPIC -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Wendif-labels -Wmissing-format-attribute -Wimplicit-fallthrough=3 -Wcast-function-type -Wshadow=compatible-local -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -Wno-format-truncation -Wno-stringop-truncation -g -g -O2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -flto=auto -ffat-lto-objects -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -fcf-protection -fno-omit-frame-pointer -I"/usr/include/postgresql/16/server/" -L"/usr/lib/postgresql/16/lib" -L/usr/lib/x86_64-linux-gnu -shared worker.c -o worker.so 

clean: clean_worker clean_worker_runner

clean_worker:
    rm -f worker.so 

clean_worker_runner:
    rm -f worker_runner.so worker_runner.o worker_runner.bc