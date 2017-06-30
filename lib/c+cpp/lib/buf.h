/**
 * Buffered IO.
 *
 * Abstrahuje recv řádku a send.
 */

typedef struct buf {
	void *data;
	size_t size;
} *buf_t;


int buf_realloc(buf_t buf, size_t size) {
	void *ndata = realloc(buf->data, size);

	if (!ndata)
		return errno;

	buf->data = ndata;
	buf->size = size;
	return 0;
}


int buf_capacity(buf_t buf, size_t size) {
	if (buf->size >= size)
		return;

	if (size < buf->size * 2)
		size = buf->size * 2;

	return buf_realloc(buf, size);
}


/**
 * Předpokládáme (ze znalosti protokolu), že nikdy nepřijdou dvě zprávy během
 * jednoho volání recv. Jinak bychom museli bufferovat zbytky pro příští
 * volání.
 */
int buf_recvline(sock_t sock, buf_t buf) {
	ssize_t err;
	size_t offset = 0;

	while (true) {
		if ((err = sock_recv(sock, buf->data + len, buf->size - len)) < 0)
			return -err;

		len += err;

		if (buf->data[len - 1] == '\n')
			break;

		if (len == buf->size)
			buf_realloc(buf, buf->size * 2);
	}

	return 0;

}
