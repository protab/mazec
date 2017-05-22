#include "websocket_http.h"
#include <errno.h>
#include <string.h>
#include "base64.h"
#include "common.h"
#include "db.h"
#include "ipc.h"
#include "log.h"
#include "sha1.h"
#include "socket.h"

#define BUF_SIZE	1024
#define FIELD_MAX_SIZE	32

struct ws_http_data;
typedef int (*hdr_process_t)(struct ws_http_data *wsd);

/* ws_http_data->token_end meaning:
 *	' '  space
 *	'\t' one or more spaces or tabs
 *	':'  colon followed by any number of spaces or tabs
 *	'\n' LF or CRLF
 *	'N'  LF or CRLF, line continuation not allowed
 * ws_http_data->future meaning:
 *	'\t' skip any number of spaces or tabs
 *	'\n' expect mandatory LF
 *	'N'  expect mandatory LF, line continuation not allowed
 *	'E'  expect mandatory LF, end of headers
 *	'\\' start of line, allow optional continuation of previous line
 *	'_'  start of line, line continuation not allowed
 */
struct ws_http_data {
	struct socket *s;
	char token_end;
	char future;
	char saved;
	hdr_process_t process;
	int buf_len;
	char buf[BUF_SIZE];
	char field[FIELD_MAX_SIZE];

	char *path;
	char *key;
	unsigned handshake;
};

#define HANDSHAKE_UPGRADE	(1 << 0)
#define HANDSHAKE_CONNECTION	(1 << 1)
#define HANDSHAKE_VERSION	(1 << 2)

#define HANDSHAKE_OK		(HANDSHAKE_UPGRADE | HANDSHAKE_CONNECTION | HANDSHAKE_VERSION)

/* Returns character to add to buffer, 0 for token end, 0x100 for token end and
 * not consume character, 0x200 for token end and end of headers, -1 to skip,
 * -2 to abort. */
static int tokenizer(struct ws_http_data *wsd, char c)
{
	switch (wsd->future) {
	case '\t':
		if (c == ' ' || c == '\t')
			return -1;
		wsd->future = 0;
		break;
	case '\n':
		wsd->future = '\\';
		if (c == '\n')
			return -1;
		return -2;
	case 'N':
		wsd->future = '_';
		if (c == '\n')
			return -1;
		return -2;
	case 'E':
		wsd->future = 0;
		if (c == '\n')
			return 0x200;
		return -2;
	case '\\':
		if (c == ' ' || c == '\t') {
			wsd->future = '\t';
			return ' ';
		}
		wsd->future = 0;
		if (c == '\n')
			return 0x200;
		if (c == '\r') {
			wsd->future = 'E';
			return -1;
		}
		return 0x100;
	case '_':
		if (c == ' ' || c == '\t')
			return -2;
		wsd->future = 0;
		if (c == '\n')
			return 0x200;
		if (c == '\r') {
			wsd->future = 'E';
			return -1;
		}
		return 0x100;
	}

	switch (wsd->token_end) {
	case ' ':
		if (c == ' ')
			return 0;
		break;
	case '\t':
		if (c == ' ' || c == '\t') {
			wsd->future = '\t';
			return 0;
		}
		break;
	case ':':
		if (c == ':') {
			wsd->future = '\t';
			return 0;
		}
		break;
	case '\n':
		if (c == '\r') {
			wsd->future = '\n';
			return -1;
		}
		if (c =='\n') {
			wsd->future = '\\';
			return -1;
		}
		break;
	case 'N':
		if (c == '\r') {
			wsd->future = 'N';
			return -1;
		}
		if (c =='\n') {
			wsd->future = '_';
			return -1;
		}
		break;
	}
	if (c == '\n' || c == '\r')
		return -2;
	return c;
}

static int process_header_chunk(struct ws_http_data *wsd, char *buf, size_t count)
{
	char *pos;

	pos = buf;
	while (pos - buf < (ssize_t)count) {
		int res = tokenizer(wsd, *pos);
		if (res == -2)
			return -EINVAL;
		if (res == -1) {
			pos++;
			continue;
		}
		if ((res & 0xff) == 0) {
			int ret;

			wsd->buf[wsd->buf_len] = '\0';
			ret = wsd->process(wsd);
			if (ret < 0)
				return ret;
			wsd->buf_len = 0;
		} else {
			/* add to buffer */
			if (wsd->buf_len >= BUF_SIZE - 1)
				return -ENOSPC;
			wsd->buf[wsd->buf_len++] = res;
		}
		if (res == 0x200)
			return 1;
		if (res != 0x100)
			pos++;
	}
	return 0;
}

static bool find_value_token(char *buf, const char *needle)
{
	char *next;
	bool finished = false;

	while (!finished) {
		while (*buf == ' ' || *buf == '\t')
			buf++;
		for (next = buf; *next && *next != ','; next++)
			;
		if (*next)
			*next = '\0';
		else
			finished = true;
		if (!strcasecmp(buf, needle))
			return true;
		buf = next + 1;
	}
	return false;
}

static int process_field(struct ws_http_data *wsd);

static int process_value(struct ws_http_data *wsd)
{
	rstrip(wsd->buf);
	if (!strcasecmp(wsd->field, "upgrade")) {
		if (strcasecmp(wsd->buf, "websocket"))
			return -EOPNOTSUPP;
		wsd->handshake |= HANDSHAKE_UPGRADE;
	} else if (!strcasecmp(wsd->field, "connection")) {
		if (!find_value_token(wsd->buf, "upgrade"))
			return -EOPNOTSUPP;
		wsd->handshake |= HANDSHAKE_CONNECTION;
	} else if (!strcasecmp(wsd->field, "sec-websocket-version")) {
		if (strcmp(wsd->buf, "13"))
			return -EOPNOTSUPP;
		wsd->handshake |= HANDSHAKE_VERSION;
	} else if (!strcasecmp(wsd->field, "sec-websocket-key")) {
		if (wsd->key)
			return -EEXIST;
		wsd->key = sstrdup(wsd->buf);
	}
	wsd->process = process_field;
	wsd->token_end = ':';
	return 0;
}

static int process_field(struct ws_http_data *wsd)
{
	rstrip(wsd->buf);
	strlcpy(wsd->field, wsd->buf, FIELD_MAX_SIZE);
	wsd->process = process_value;
	wsd->token_end = '\n';
	return 0;
}

static int process_http_ver(struct ws_http_data *wsd)
{
	if (strcmp(wsd->buf, "HTTP/1.1"))
		return -ENOPROTOOPT;
	wsd->process = process_field;
	wsd->token_end = ':';
	return 0;
}

static int process_path(struct ws_http_data *wsd)
{
	wsd->path = sstrdup(wsd->buf);
	wsd->process = process_http_ver;
	wsd->token_end = 'N';
	return 0;
}

static int process_method(struct ws_http_data *wsd)
{
	if (strcmp(wsd->buf, "GET"))
		return -ENOTBLK;
	wsd->process = process_path;
	return 0;
}

static void ws_report_error(struct ws_http_data *wsd, int err)
{
	char *msg;

	log_warn("websocket handshake error (%d) on fd %d", err, socket_get_fd(wsd->s));
	if (err == -ENOTSOCK) {
		/* just close the socket */
		socket_del(wsd->s);
		return;
	}

	switch (err) {
	case -EINVAL: /* misformatted or missing headers */
	case -ENOSPC: /* field or value is too long */
	case -EOPNOTSUPP: /* unsupported header value */
	case -EEXIST: /* repeated header */
		msg = "HTTP/1.1 400 Bad Request\r\n\r\n";
		break;
	case -ENOTBLK: /* bad method */
		msg = "HTTP/1.1 501 Not Implemented\r\n\r\n";
		break;
	case -ENOPROTOOPT: /* unsupported HTTP version */
		msg = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
		break;
	case -ENOENT: /* user (path) not found */
		msg = "HTTP/1.1 404 Not Found\r\n\r\n";
		break;
	default:
		msg = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
		break;
	}
	check(socket_stop_reading(wsd->s));
	socket_write(wsd->s, msg, strlen(msg), false);
	socket_flush_and_del(wsd->s);
}

static void ws_headers_sent(struct socket *s __unused, void *data)
{
	struct ws_http_data *wsd = data;
	int fd;
	struct socket *pipe;

	socket_del(wsd->s);
	pipe = db_get_pipe(wsd->path + 1);
	fd = socket_get_fd(wsd->s);
	if (!pipe || ipc_send_fd(pipe, fd, IPC_FD_WEBSOCKET) < 0) {
		log_warn("unable to send fd %d to child [%s]", fd, wsd->path + 1);
	} else {
		socket_set_unmanaged(wsd->s);
		log_info("sending websocket fd %d to child [%s]", fd, wsd->path + 1);
	}
	return;
}

static char ws_response1[] =
	"HTTP/1.1 101 Switching Protocols\r\n"
	"Upgrade: websocket\r\n"
	"Connection: Upgrade\r\n"
	"Sec-WebSocket-Accept: ";
static char ws_response2[] =
	"\r\n\r\n";
static unsigned char ws_uuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char *construct_accept_key(char *key, size_t *out_len)
{
	SHA1_CTX sha;
	unsigned char sha_result[20];

	SHA1Init(&sha);
	SHA1Update(&sha, (unsigned char *)key, strlen(key));
	SHA1Update(&sha, ws_uuid, sizeof(ws_uuid) - 1);
	SHA1Final(sha_result, &sha);
	return (char *)base64_encode(sha_result, 20, out_len);
}

static int ws_headers_processed(struct ws_http_data *wsd)
{
	char *key;
	size_t key_len;

	if (wsd->handshake != HANDSHAKE_OK)
		return -EINVAL;
	if (wsd->path[0] != '/' || !db_user_exists(wsd->path + 1))
		return -ENOENT;

	check(socket_stop_reading(wsd->s));

	if (socket_write(wsd->s, ws_response1, sizeof(ws_response1) - 1, false) < 0)
		return -ENOTSOCK;
	key = construct_accept_key(wsd->key, &key_len);
	if (!key)
		return -EINVAL;
	if (socket_write(wsd->s, key, key_len, true) < 0)
		return -ENOTSOCK;
	if (socket_write(wsd->s, ws_response2, sizeof(ws_response2) - 1, false) < 0)
		return -ENOTSOCK;
	socket_set_write_done_cb(wsd->s, ws_headers_sent);
	return 0;
}

static void ws_header_read(struct socket *s, void *data)
{
	struct ws_http_data *wsd = data;
	char buf[BUF_SIZE];
	size_t count;
	int ret;

	while (true) {
		count = socket_read(s, buf, BUF_SIZE);
		if (!count)
			break;
		ret = process_header_chunk(wsd, buf, count);
		if (ret < 0) {
			ws_report_error(wsd, ret);
			return;
		}
		if (ret > 0) {
			ret = ws_headers_processed(wsd);
			if (ret < 0)
				ws_report_error(wsd, ret);
			break;
		}
	}
}

static void *ws_new(struct socket *s)
{
	struct ws_http_data *wsd;

	wsd = szalloc(sizeof(*wsd));
	wsd->s = s;
	wsd->process = process_method;
	wsd->token_end = ' ';
	return wsd;
}

static void ws_free(void *data)
{
	struct ws_http_data *wsd = data;

	if (wsd->path)
		sfree(wsd->path);
	if (wsd->key)
		sfree(wsd->key);
	sfree(wsd);
}

int websocket_http_init(unsigned port)
{
	return socket_listen(port, ws_new, ws_header_read, ws_free);
}
