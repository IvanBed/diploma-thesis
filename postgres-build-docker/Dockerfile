FROM ubuntu:22.04 as build-stage

RUN apt update -y && apt upgrade -y && apt install -y gcc make pkg-config libreadline-dev zlib1g-dev icu-devtools libicu-dev flex bison libxml2-utils

RUN useradd postgres -d /var/local/lib/postgresql/ -s /bin/bash && mkdir -p /usr/local/pgsql/data && chown postgres /usr/local/pgsql/data

COPY postgresql /var/postgresql 
COPY entrypoint.sh /var/postgresql
COPY pg_stat_monitor /var/pg_stat_monitor

WORKDIR /var/postgresql

# Сборка postgres

RUN ./configure --enable-debug --enable-cassert --enable-injection-points &&\
        make &&\
        #make check &&\
        make install &&\
        mkdir /var/log/postgresql && chown postgres /var/log/postgresql

# Сборка pg_stat_monitor 

WORKDIR /var/pg_stat_monitor

RUN make USE_PGXS=1 PG_CONFIG=/usr/local/pgsql/bin/pg_config &&\
        make USE_PGXS=1 PG_CONFIG=/usr/local/pgsql/bin/pg_config install

USER postgres

WORKDIR /var/postgresql

EXPOSE 5432

ENTRYPOINT ["./entrypoint.sh"] 


    
 