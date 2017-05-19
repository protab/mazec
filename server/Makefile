CFLAGS = -W -Wall -Wno-unused-result -g -std=gnu99
LDFLAGS = 

OBJS = common.o db.o event.o log.o main.o spawn.o

mazec:	$(OBJS)
	gcc $(LDFLAGS) -o $@ $+

main.o: main.c
	gcc $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	gcc $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) mazec
