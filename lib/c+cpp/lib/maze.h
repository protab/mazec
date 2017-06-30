#include "lib/buf.h"

#define MAZE_HOST "localhost"
#define MAZE_PORT 4000

enum {
	M_OK,
	M_ERROR,
	M_GAME_OVER,
	M_FORBIDDEN
};

typedef struct maze {
	struct sock sock;
	struct buf replybuf;
	size_t width, height;
	const char *msg;
} maze;

int maze_open(maze *m, const char *username, const char *level);
ssize_t maze_command(maze *m, const char *command, buf_t reply);
int maze_int_command(maze *m, const char *command, size_t clen, int *reply);
int maze_void_command(maze *m, const char *command, size_t clen);


inline int maze_err(maze *m, int code, const char *msg) {
	m->msg = msg;
	return code;
}

ssize_t maze_command(maze *m, const char *command, size_t clen, buf_t reply) {
	size_t offset = 0;
	ssize_t err;

	while (offset < clen) {
		if ((err = sock_send(&m->sock, command + offset, clen - offset)) < 0)
			return -err;
		offset += err;
	}

	err = buf_recvline(&m->sock, reply);
	if (err < 0)
		return maze_err(-M_ERROR, m->sock->msg);

	if (err < 4)
		return maze_err(-M_ERROR, "Chyba protokolu: očekávány alespoň 4 znaky");

	if (!strncmp("OVER", reply->data, 4)
		return maze_err(-M_GAME_OVER, reply->data + 5);

	return M_OK;
}

int maze_int_command(maze *m, const char *command, size_t clen, int *reply) {
	ssize_t len;

	if ((len = maze_command(m, command, clen, &m->replybuf)) < 0)
		return -len;

	if (1 != sscanf(m->replybuf->data, "DATA %d", reply))
		return maze_err(m, M_ERROR, "Neočekávaná odpověď serveru.";

	return M_OK;
}

int maze_void_command(maze *m, const char *command, size_t clen) {
	ssize_t len;

	if ((len = maze_command(m, command, clen, &m->replybuf)) < 0)
		return -len;

	if (strncmp("DONE", m->replybuf->data))
		return maze_err(m, M_ERROR, "Neočekávaná odpověď serveru.";

	return M_OK;
}

int maze_open(maze *m, const char *username, const char *level) {
	int len, err;
	char *str;

	memset(m, 0, sizeof(struct maze));
	if (!buf_capacity(m->replybuf, 256))
		return maze_err(m, M_ERROR, "Nezdařilo se alokovat buffer pro odpověď.");

	if (!sock_open(&m->sock, MAZE_HOST, MAZE_PORT))
		return maze_err(m, M_ERROR, m->sock.msg);

	if ((len = asprintf(&str, "USER %s", username)))
		return maze_err(m, M_ERROR, "Nezdařilo se zkonstruovat příkaz USER.");

	err = maze_void_command(m, str, len);
	free(str);
	if (err)
		return err;



	return M_OK;
}
