#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include "base64.h"
#include "common.h"
#include "log.h"
#include "sha1.h"
#include "socket.h"

struct conn_data {
	int stream;
	size_t num_bytes;
};

static int streams = 0;

static char *count_sha1(void *buf, size_t size)
{
	static char output[41];
	SHA1_CTX sha;
	unsigned char result[20];

	SHA1Init(&sha);
	SHA1Update(&sha, buf, size);
	SHA1Final(result, &sha);
	for (int i = 0; i < 20; i++)
		snprintf(output + 2 * i, 3, "%02x", result[i]);
	return output;
}

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
		//buf[count] = '\0';
		cd->num_bytes += count;
		log_info("stream %d sha: %s", cd->stream, count_sha1(buf, count));

		unsigned char *b64 = base64_encode((unsigned char *)buf, count, NULL);
		log_info("stream %d base64: %s", cd->stream, b64);
		free(b64);
	}
	check(socket_write(s, "ack\n", 4, false));
}

int test_init_master(void)
{
	return socket_listen(1234, conn_new, conn_read, free);
}
