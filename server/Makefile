CFLAGS = -W -Wall -Wno-unused-result -g -std=gnu99 -D_GNU_SOURCE
LDFLAGS = 

OBJS = common.o base64.o db.o event.o ipc.o log.o main.o sha1.o socket.o spawn.o \
       websocket_data.o websocket_http.o

mazec:	$(OBJS)
	gcc $(LDFLAGS) -o $@ $+

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) mazec
