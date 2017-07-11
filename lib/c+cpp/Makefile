CFLAGS := -std=gnu99 -ggdb3 -I./lib
LDFLAGS := -L./lib
LIBS := -lmaze
BIN := example-callback example-procedural

%: %.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $< $(LIBS)

all: $(BIN)

lib:
	$(MAKE) -C lib/

clean:
	$(MAKE) -C lib/ clean
	rm -rf $(BIN)

example-callback: lib
example-procedural: lib

.PHONY: all lib clean

