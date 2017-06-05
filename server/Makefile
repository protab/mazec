CFLAGS = -W -Wall -Wno-unused-result -g -std=gnu99 -D_GNU_SOURCE
LDFLAGS = -rdynamic -ldl

OBJS = app.o common.o base64.o db.o draw.o event.o ipc.o log.o main.o proto.o \
       sha1.o socket.o spawn.o websocket_data.o websocket_http.o

all: mazec build_levels

mazec: $(OBJS)
	gcc $(LDFLAGS) -o $@ $+

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

build_levels: levels/Makefile
	make -C levels

levels/Makefile:
	cd levels && ../scripts/build_makefile
.PHONY: levels/Makefile

clean:
	rm -f $(OBJS) mazec levels/*.o levels/*.so levels/Makefile
