CFLAGS = -W -Wall -Wno-unused-result -g -std=gnu99 -D_GNU_SOURCE
LDFLAGS = -rdynamic -ldl

OBJS = app.o common.o base64.o db.o draw.o event.o ipc.o log.o main.o proto.o \
       pybindings.o sha1.o socket.o spawn.o time.o websocket_data.o websocket_http.o

pypkg = $(shell pkg-config --list-all | grep '^python-3' | cut -d ' ' -f 1 | sort -r -V | head -n 1)
PY_CFLAGS = $(shell pkg-config --cflags $(pypkg))
PY_LDFLAGS = $(shell pkg-config --libs $(pypkg))

all: mazec build_levels build_pylevels

mazec: config.h $(OBJS)
	gcc $(LDFLAGS) $(PY_LDFLAGS) -o $@ $(OBJS)

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

pybindings.o: pybindings.c pybindings.h
	gcc $(CFLAGS) $(PY_CFLAGS) -c -o $@ $<

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

config.h: config.defaults
	scripts/build_config

build_levels: levels/Makefile
	make -C levels

build_pylevels: pylevels/Makefile
	make -C pylevels

levels/Makefile:
	cd levels && ../scripts/build_makefile
.PHONY: levels/Makefile

pylevels/Makefile:
	cd pylevels && ../scripts/build_pymakefile
.PHONY: pylevels/Makefile

clean:
	rm -f $(OBJS) mazec levels/*.o levels/*.so levels/Makefile pylevels/code_* pylevels/Makefile
	rm -rf pylevels/__pycache__

distclean: clean
	rm -f config.h
