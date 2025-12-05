CC = gcc  # C compiler
CFLAGS = -I/usr/include/postgresql/15/server/ -fPIC -Wall -Wextra -O2 -g # C flags
LDFLAGS = -shared   # linking flags
RM = rm -f   # rm command
TARGET_LIB = test-ext.so  # target lib

PG_CONFIG ?= /usr/bin/pg_config

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

SRCS = test-ext.c  # source files
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)
