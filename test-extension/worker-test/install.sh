#!bin/bash

rm worker.so

gcc -fPIC -Wall -Wmissing-prototypes -Wpointer-arith -Wdeclaration-after-statement -Werror=vla -Wendif-labels -Wmissing-format-attribute -Wimplicit-fallthrough=3 -Wcast-function-type -Wshadow=compatible-local -Wformat-security -fno-strict-aliasing -fwrapv -fexcess-precision=standard -Wno-format-truncation -Wno-stringop-truncation -g -g -O2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -flto=auto -ffat-lto-objects -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -fcf-protection -fno-omit-frame-pointer -I"/usr/include/postgresql/16/server/" -L"/usr/lib/postgresql/16/lib" -L/usr/lib/x86_64-linux-gnu -shared worker.c -o worker.so 

cp worker.so /usr/lib/postgresql/16/lib/

sudo systemctl restart postgresql

ps ax | grep postgres
