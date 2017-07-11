CFLAGS := -std=gnu99 -ggdb3 -I.
OBJS := libmaze socket_unix
HEADER := maze maze_socket

all: libmaze.a #libmaze.so

%-shared.o: %.c $(addsuffix .h,$(HEADER))
	$(CC) $(CFLAGS) -fPIC -o $@ -c $<

%-static.o: %.c $(addsuffix .h,$(HEADER))
	$(CC) $(CFLAGS) -o $@ -c $<

libmaze.a: $(addsuffix -static.o,$(OBJS))
	ar rcs $@ $^

libmaze.so: $(addsuffix -shared.o,$(OBJS))
	$(CC) -shared -Wl,-soname,libmaze.so.1 -o $@ $^

clean:
	rm -rf libmaze.a libmaze.so *.o

.PHONY: all clean
