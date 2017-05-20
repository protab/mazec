CFLAGS = -W -Wall -Wno-unused-result -g -std=gnu99 -D_GNU_SOURCE
LDFLAGS = 

OBJS = common.o db.o event.o log.o main.o socket.o spawn.o test.o

mazec:	$(OBJS)
	gcc $(LDFLAGS) -o $@ $+

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) mazec
