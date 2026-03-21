FROM postgres:18.3

RUN apt update -y && apt upgrade -y && apt install -y postgresql-server-dev-18 gcc make pkg-config libreadline-dev zlib1g-dev icu-devtools libicu-dev flex bison libxml2-utils

RUN mkdir /usr/lib/postgresql/18/exts &&\
    chown postgres:postgres /usr/lib/postgresql/18/exts

COPY custom_executor /usr/lib/postgresql/18/exts/custom_executor

EXPOSE 5432



