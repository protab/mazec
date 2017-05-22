#include "proto.h"
#include "proto_msg.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app.h"
#include "common.h"
#include "db.h"
#include "ipc.h"
#include "log.h"
#include "socket.h"

#define BUF_SIZE	1024
#define CMD_LEN		4
#define VAL_LEN		LOGIN_LEN

struct p_data;
typedef char *(*cmd_process_t)(struct p_data *pd);

struct p_data {
	struct socket *s;
	char cmd[CMD_LEN + 1];
	char val[VAL_LEN + 1];
	unsigned int cmd_len;
	unsigned int val_len;
	bool msg_complete;
	bool is_val;
	bool crlf;
	bool expect_lf;
	cmd_process_t process;
	void *data;
	bool bound;
	// TODO: timeout when nothing received for some time
};

static proto_close_cb_t p_close_cb;
static int p_count;
static int p_bound_count;
static int p_bound_max;
static char *p_code;
static const struct app_ops *p_level;

/* Deletes the socket if send fails. */
static int p_send_msg(struct p_data *pd, char *cmd, char *data)
{
	char *msg;
	size_t data_len = 0, msg_len;
	int ret;

	if (data)
		data_len = strlen(data) + 1;
	msg_len = 4 + data_len + (pd->crlf ? 2 : 1);
	msg = salloc(msg_len);
	memcpy(msg, cmd, 4);
	if (data) {
		msg[4] = ' ';
		memcpy(msg + 5, data, data_len - 1);
	}
	if (pd->crlf)
		memcpy(msg + 4 + data_len, "\r\n", 2);
	else
		msg[4 + data_len] = '\n';
	ret = socket_write(pd->s, msg, msg_len, true);
	if (ret < 0) {
		sfree(msg);
		socket_del(pd->s);
	}
	return ret;
}

static int p_send_ack(struct p_data *pd)
{
	return p_send_msg(pd, "DONE", NULL);
}

static int p_send_nope(struct p_data *pd, char *msg)
{
	return p_send_msg(pd, "NOPE", msg);
}

static int p_send_data(struct p_data *pd, int *data, int len)
{
	char *msg;
	size_t size, pos = 0;
	int ret;

	size = len * 12 + 1;
	msg = salloc(size);
	for (int i = 0; i < len; i++) {
		pos += snprintf(msg + pos, size - pos, " %d", data[i]);
		if (pos >= size)
			break;
	}
	ret = p_send_msg(pd, "DATA", msg + 1);
	sfree(msg);
	return ret;
}

static void p_report_error(struct p_data *pd, char *msg)
{
	log_info("closing app socket fd %d due to protocol error", socket_get_fd(pd->s));
	check(socket_stop_reading(pd->s));
	p_send_msg(pd, "OVER", msg);
	socket_flush_and_del(pd->s);
}

static char *process_msg_chunk(struct p_data *pd, char *buf, size_t count)
{
	char *pos;

	for (pos = buf; pos - buf < (ssize_t)count; pos++) {
		if (pd->cmd_len < CMD_LEN) {
			if (*pos < 'A' || *pos > 'Z')
				return P_MSG_CMD_NO_LETTER;
			pd->cmd[pd->cmd_len++] = *pos;
			continue;
		}
		if (pd->msg_complete)
			return P_MSG_IMPATIENT;
		if (pd->expect_lf) {
			if (*pos != '\n')
				return P_MSG_INVALID_EOL;
			pd->expect_lf = false;
			pd->crlf = true;
			pd->msg_complete = true;
			continue;
		}
		if (pd->val_len == 0) {
			pd->cmd[CMD_LEN] = '\0';
			if (*pos == ' ' || *pos == '\t') {
				/* to be robust, accept also more whitespace
				 * between command and data (i.e. don't
				 * check whether is_val was already set) */
				pd->is_val = true;
				continue;
			}
			if (!pd->is_val && (*pos != '\r' && *pos != '\n'))
				return P_MSG_CMD_EXTRA_CHARS;
		}
		if (*pos == '\r') {
			pd->expect_lf = true;
			continue;
		}
		if (*pos == '\n') {
			pd->crlf = false;
			pd->msg_complete = true;
			continue;
		}
		if (*pos == '\0')
			return P_MSG_VAL_CONTAINS_NULL;
		if (pd->val_len == VAL_LEN)
			return P_MSG_VAL_TOO_LONG;
		pd->val[pd->val_len++] = *pos;
		pd->val[pd->val_len] = '\0';
	}
	return NULL;
}

static void check_msg_complete(struct p_data *pd)
{
	char *ret;

	if (!pd->msg_complete)
		return;
	ret = pd->process(pd);
	if (ret) {
		p_report_error(pd, ret);
		return;
	}
	pd->cmd_len = 0;
	pd->val_len = 0;
	pd->msg_complete = false;
	pd->is_val = false;
	pd->expect_lf = false;
}

static void p_read(struct socket *s, void *data)
{
	struct p_data *pd = data;
	char buf[BUF_SIZE];
	size_t count;
	char *ret;

	while (true) {
		count = socket_read(s, buf, BUF_SIZE);
		if (!count)
			break;
		ret = process_msg_chunk(pd, buf, count);
		if (ret) {
			p_report_error(pd, ret);
			return;
		}
	}
	check_msg_complete(pd);
}

static bool get_2_int(char *val, int *res1, int *res2)
{
	char *ptr = val;
	long int res;

	for (int i = 0; i < 2; i++) {
		errno = 0;
		res = strtol(ptr, &ptr, 10);
		if (errno)
			return false;
		if (res < 0 || res > INT_MAX)
			return false;
		if (i == 0)
			*res1 = res;
		else
			*res2 = res;
	}
	return true;
}

static char *process_cmd(struct p_data *pd)
{
	char *nope = NULL;

	if (!strcmp(pd->cmd, "MOVE")) {
		if (pd->val_len != 1)
			return P_MSG_CHAR_EXPECTED;
		if (!p_level->move) {
			nope = A_MSG_MOVE_NOT_AVAIL;
		} else {
			nope = p_level->move(pd->val[0]);
			if (!nope)
				p_send_ack(pd);
		}
	} else if (!strcmp(pd->cmd, "WHAT")) {
		int x, y, res;

		if (!get_2_int(pd->val, &x, &y))
			return P_MSG_2INT_EXPECTED;
		nope = p_level->what(x, y, &res);
		if (!nope)
			p_send_data(pd, &res, 1);
	} else if (!strcmp(pd->cmd, "MAZE")) {
		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		if (!p_level->maze) {
			nope = A_MSG_MAZE_NOT_AVAIL;
		} else {
			int *res, len;

			nope = p_level->maze(&res, &len);
			if (!nope)
				p_send_data(pd, res, len);
		}
	} else if (!strcmp(pd->cmd, "GETX")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_x(&res);
		if (!nope)
			p_send_data(pd, &res, 1);
	} else if (!strcmp(pd->cmd, "GETY")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_y(&res);
		if (!nope)
			p_send_data(pd, &res, 1);
	} else if (!strcmp(pd->cmd, "GETW")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_w(&res);
		if (!nope)
			p_send_data(pd, &res, 1);
	} else if (!strcmp(pd->cmd, "GETH")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_h(&res);
		if (!nope)
			p_send_data(pd, &res, 1);
	} else if (!strcmp(pd->cmd, "WAIT")) {
		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		// FIXME: implement WAIT
		p_send_ack(pd);
	} else {
		return P_MSG_CMD_UNKNOWN;
	}

	if (nope)
		p_send_nope(pd, nope);
	return NULL;
}

static char *process_level(struct p_data *pd)
{
	if (strcmp(pd->cmd, "LEVL"))
		return P_MSG_LEVL_EXPECTED;

	if (p_code && strcmp(pd->val, p_code))
		return P_MSG_LEVL_NOT_MATCHING;
	if (p_bound_count >= p_bound_max)
		return P_MSG_CONN_TOO_MANY;

	if (!p_code) {
		p_level = app_get_level(pd->val);
		if (!p_level)
			return P_MSG_LEVL_UNKNOWN;
		p_code = sstrdup(pd->val);
		p_bound_max = p_level->max_conn;
	}
	if (p_level->get_data)
		pd->data = p_level->get_data();
	pd->bound = true;
	pd->process = process_cmd;
	p_bound_count++;

	p_send_ack(pd);
	return NULL;
}

static char *process_user(struct p_data *pd)
{
	if (strcmp(pd->cmd, "USER"))
		return P_MSG_USER_EXPECTED;

	if (!db_user_exists(pd->val))
		return P_MSG_USER_UNKNOWN;

	ipc_send_socket(pd->val, pd->s, pd->crlf ? IPC_FD_APP_CRLF : IPC_FD_APP_LF);
	return NULL;
}

static void *p_server_new(struct socket *s)
{
	struct p_data *pd;

	pd = szalloc(sizeof(*pd));
	pd->s = s;
	pd->process = process_user;
	return pd;
}

static void p_server_free(void *data)
{
	struct p_data *pd = data;

	sfree(pd);
}

int proto_server_init(unsigned port)
{
	return socket_listen(port, p_server_new, p_read, p_server_free);
}

void proto_client_init(proto_close_cb_t close_cb)
{
	p_close_cb = close_cb;
	p_count = 0;
	p_bound_count = 0;
	p_bound_max = 1;
	p_code = NULL;
}

/* Calls the close callback if there is no app socket open. */
void proto_cond_close(void)
{
	if (!p_count && p_close_cb)
		p_close_cb();
}

static void p_free(void *data)
{
	struct p_data *pd = data;
	bool bound = pd->bound;

	p_count--;
	if (bound)
		p_bound_count--;
	if (p_level && p_level->free_data)
		p_level->free_data(pd->data);
	p_server_free(data);
	if ((!p_count || bound) && p_close_cb)
		p_close_cb();
}

/* Closes the fd even if unsuccessful. */
int proto_client_add(int fd, bool crlf)
{
	struct p_data *pd;

	pd = szalloc(sizeof(*pd));
	pd->s = socket_add(fd, p_read, pd, p_free);
	if (!pd->s) {
		sfree(pd);
		close(fd);
		return -ENOTSOCK;
	}
	pd->process = process_level;
	pd->crlf = crlf;
	p_count++;

	return p_send_ack(pd);
}
