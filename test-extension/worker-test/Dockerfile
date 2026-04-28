FROM postgres:18.3

RUN apt update -y && apt upgrade -y && apt install -y postgresql-server-dev-18 gcc make pkg-config libreadline-dev zlib1g-dev icu-devtools libicu-dev flex bison libxml2-utils

RUN mkdir /usr/lib/postgresql/18/exts &&\
    chown postgres:postgres /usr/lib/postgresql/18/exts

COPY worker /usr/lib/postgresql/18/exts/worker

WORKDIR /usr/lib/postgresql/18/exts/worker

RUN make USE_PGXS=1 &&\
        make USE_PGXS=1 install

RUN gcc -fPIC -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Wendif-labels -Wmissing-format-attribute -Wimplicit-fallthrough=3 -Wcast-function-type -Wshadow=compatible-local -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -Wno-format-truncation -Wno-stringop-truncation -g -g -O2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -flto=auto -ffat-lto-objects -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -fcf-protection -fno-omit-frame-pointer -I"/usr/include/postgresql/18/server/" -L"/usr/lib/postgresql/18/lib" -L/usr/lib/x86_64-linux-gnu -shared worker.c -o worker.so &&\        
        cp worker.so /usr/lib/postgresql/18/lib/

EXPOSE 5432



