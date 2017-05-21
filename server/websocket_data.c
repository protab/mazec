#include "websocket_data.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "socket.h"

#define BUF_SIZE		1024
#define MAX_PAYLOAD_SIZE	4096

#define OP_CONT		0x00
#define OP_TEXT		0x01
#define OP_BINARY	0x02
#define OP_CLOSE	0x08
#define OP_PING		0x09
#define OP_PONG		0x0a

struct ws_data {
	struct socket *s;
	unsigned offset;
	char *payload;
	size_t payload_size;
	size_t payload_len;

	char mask[4];
	bool fin;
	unsigned char opcode;
	unsigned char payload_enc_len;

	unsigned char reasm_opcode;
	char *reassembled;
	size_t reassembled_len;

	struct ws_data *next;
};

static websocket_cb_t ws_cb;
static struct ws_data *websockets;

static void ws_free(void *data)
{
	struct ws_data *wsd = data;
	struct ws_data **ptr;

	for (ptr = &websockets; *ptr && *ptr != wsd; ptr = &(*ptr)->next)
		;
	if (*ptr)
		*ptr = wsd->next;
	free(wsd);
}

static void ws_write(struct socket *s, unsigned opcode, void *buf, size_t size, bool steal)
{
	char hdr[6];
	unsigned payload_enc_len;
	size_t tmp;

	hdr[0] = opcode | 0x80;
	if (size <= 125) {
		hdr[1] = size;
		payload_enc_len = 0;
	} else if (size <= 0xffff) {
		hdr[1] = 126;
		payload_enc_len = 2;
	} else {
		hdr[2] = 127;
		payload_enc_len = 4;
	}
	tmp = size;
	for (int i = payload_enc_len - 1; i >= 0; i--) {
		hdr[3 + i] = tmp & 0xff;
		tmp >>= 8;
	}
	if (socket_write(s, hdr, 2 + payload_enc_len, false) < 0)
		goto error;
	if (size && socket_write(s, buf, size, steal) < 0)
		goto error;
	return;

error:
	if (steal)
		free(buf);
}

static void ws_error(struct socket *s, uint16_t code)
{
	code = htons(code);
	ws_write(s, OP_CLOSE, &code, 2, false);
	socket_flush_and_del(s);
}

static void reset_message(struct ws_data *wsd)
{
	if (wsd->payload)
		free(wsd->payload);
	wsd->payload = NULL;
	wsd->offset = 0;
}

static void reset_reassembly(struct ws_data *wsd)
{
	if (wsd->reassembled)
		free(wsd->reassembled);
	wsd->reassembled = NULL;
	wsd->reassembled_len = 0;
	wsd->reasm_opcode = 0;
}

static void reassembly_message(struct ws_data *wsd)
{
	if (!wsd->reasm_opcode) {
		wsd->reasm_opcode = wsd->opcode;
		wsd->reassembled = wsd->payload;
		wsd->reassembled_len = wsd->payload_len;
		wsd->payload = NULL;
	} else if (wsd->payload_len > 0) {
		if (!wsd->reassembled) {
			wsd->reassembled = wsd->payload;
			wsd->reassembled_len = wsd->payload_len;
			wsd->payload = NULL;
		} else {
			char *new;

			if (wsd->reassembled_len + wsd->payload_len > MAX_PAYLOAD_SIZE)
				goto error;
			new = realloc(wsd->reassembled, wsd->reassembled_len + wsd->payload_len);
			if (!new)
				goto error;
			wsd->reassembled = new;
			memcpy(new + wsd->reassembled_len, wsd->payload, wsd->payload_len);
			wsd->reassembled_len += wsd->payload_len;
		}
	}
	if (!wsd->fin)
		return;

	if (wsd->reassembled_len)
		/* ignore empty messages */
		ws_cb(wsd->s, wsd->reassembled, wsd->reassembled_len);

error:
	reset_reassembly(wsd);
}

static int consume_message(struct ws_data *wsd)
{
	int ret = 0;

	switch (wsd->opcode) {
	case OP_CONT:
		if (!wsd->reasm_opcode) {
			ret = -EINVAL;
			goto error;
		}
		reassembly_message(wsd);
		break;
	case OP_TEXT:
		if (wsd->reasm_opcode)
			reset_reassembly(wsd);
		reassembly_message(wsd);
		break;
	case OP_BINARY:
		ret = -EOPNOTSUPP;
		goto error;
	case OP_CLOSE:
		ret = -EPIPE;
		goto error;
	case OP_PING:
		ws_write(wsd->s, OP_PONG, wsd->payload, wsd->payload_len, true);
		wsd->payload = NULL;
		break;
	case OP_PONG:
		/* ignore */ ;
	}

error:
	reset_message(wsd);
	return ret;
}

static int process_chunk(struct ws_data *wsd, char *buf, size_t len)
{
	int ret;

	for (size_t i = 0; i < len; i++) {
		char c = buf[i];

		if (!wsd->payload) {
			/* in header */
			if (wsd->offset == 0) {
				/* first byte */
				wsd->fin = !!(c & 0x80);
				if (c & 0x70)
					/* reserved bits */
					return -EINVAL;
				wsd->opcode = c & 0x0f;
				if ((c & 0x07) > 2)
					/* unknown opcode */
					return -EINVAL;
			} else if (wsd->offset == 1) {
				if (!(c & 0x80))
					/* mask bit, mandatory for client */
					return -EINVAL;
				c &= 0x7f;
				if (c <= 125) {
					wsd->payload_enc_len = 0;
					wsd->payload_size = c;
				} else if (c == 126) {
					wsd->payload_enc_len = 2;
					wsd->payload_size = 0;
				} else {
					wsd->payload_enc_len = 4;
					wsd->payload_size = 0;
				}
			} else if (wsd->offset < 2 + (unsigned)wsd->payload_enc_len) {
				wsd->payload_size <<= 8;
				wsd->payload_size |= c;
			} else if (wsd->offset < 6 + (unsigned)wsd->payload_enc_len) {
				wsd->mask[wsd->offset - 2 -  wsd->payload_enc_len] = c;
			}
			wsd->offset++;

			if (wsd->offset == 6 + (unsigned)wsd->payload_enc_len) {
				if (wsd->payload_size > MAX_PAYLOAD_SIZE)
					return -ENOSPC;
				wsd->payload_len = 0;
				if (wsd->payload_size == 0) {
					ret = consume_message(wsd);
					if (ret < 0)
						return ret;
				} else {
					wsd->payload = malloc(wsd->payload_size);
					if (!wsd->payload)
						return -ENOMEM;
				}
			}
		} else {
			/* payload */
			wsd->payload[wsd->payload_len] = c ^ wsd->mask[wsd->payload_len % 4];
			wsd->payload_len++;
			if (wsd->payload_len == wsd->payload_size) {
				ret = consume_message(wsd);
				if (ret < 0)
					return ret;
			}
		}
	}
	return 0;
}

static void ws_read(struct socket *s, void *data)
{
	struct ws_data *wsd = data;
	char buf[BUF_SIZE];
	size_t count;
	int ret;

	while (true) {
		count = socket_read(s, buf, BUF_SIZE);
		if (!count)
			break;
		ret = process_chunk(wsd, buf, count);
		if (ret < 0) {
			log_info("closing websocket fd %d (reason %d)", socket_get_fd(s), ret);
			switch (ret) {
			case ENOMEM:
				socket_del(s);
				break;
			case EINVAL:
				ws_error(s, 1002);
				break;
			case EOPNOTSUPP:
				ws_error(s, 1003);
				break;
			case EPIPE:
				ws_error(s, 1000);
				break;
			case ENOSPC:
				ws_error(s, 1009);
				break;
			default:
				ws_error(s, 1008);
				break;
			}
			return;
		}
	}
}

int websocket_add(int fd)
{
	struct ws_data *wsd;

	wsd = calloc(1, sizeof(*wsd));
	if (!wsd)
		return -errno;
	wsd->s = socket_add(fd, ws_read, wsd, ws_free);
	if (!wsd->s) {
		free(wsd);
		return -ENOMEM;
	}
	wsd->next = websockets;
	websockets = wsd;
	return 0;
}

void websocket_write(struct socket *s, void *buf, size_t size, bool steal)
{
	ws_write(s, OP_TEXT, buf, size, steal);
}

void websocket_broadcast(void *buf, size_t size)
{
	struct ws_data *wsd;

	for (wsd = websockets; wsd; wsd = wsd->next)
		websocket_write(wsd->s, buf, size, false);
}

void websocket_init(websocket_cb_t cb)
{
	ws_cb = cb;
}
