CFLAGS = -fPIC -W -Wall -Wno-unused-result -g -std=gnu99 -D_GNU_SOURCE
LDFLAGS = -rdynamic -shared

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

all: all2

