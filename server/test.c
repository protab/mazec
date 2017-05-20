#include "test.h"
#include <stdlib.h>
#include "common.h"
#include "log.h"
#include "socket.h"

struct conn_data {
	int stream;
	size_t num_bytes;
};

static int streams = 0;

void *conn_new(struct socket *s __unused)
{
	struct conn_data *cd;

	cd = calloc(1, sizeof(struct conn_data));
	if (!cd)
		return NULL;
	cd->stream = streams++;
	return cd;
}

void conn_read(struct socket *s, void *data)
{
	struct conn_data *cd = data;
	char buf[257];
	size_t count;

	while (true) {
		count = socket_read(s, buf, 256);
		if (!count)
			break;
		buf[count] = '\0';
		cd->num_bytes += count;
		log_info("stream %d: %s", cd->stream, buf);
	}
	check(socket_write(s, "ack\n", 4, false));
}

int test_init_master(void)
{
	return socket_listen(1234, conn_new, conn_read, free);
}
