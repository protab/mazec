#include "proto.h"
#include "proto_msg.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "app.h"
#include "common.h"
#include "config.h"
#include "db.h"
#include "draw.h"
#include "ipc.h"
#include "level.h"
#include "log.h"
#include "socket.h"
#include "spawn.h"
#include "time.h"

#define BUF_SIZE	1024
#define CMD_LEN		4
#define VAL_LEN		LOGIN_LEN

#define REDRAW_INTERVAL		200
#define CAN_PAUSE_INTERVAL	1000

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

	struct p_data *next;
};

static struct p_data *p_sockets;

static char *p_login;
static proto_close_cb_t p_close_cb;
static int p_count;
static int p_bound_count;
static int p_bound_max;
static bool p_end_set;
static struct timespec p_end;
static struct timespec p_can_pause_until;
static long p_paused_time;
static char *p_code;
static const struct level_ops *p_level;
static int p_draw_timer;
static bool p_waiting;

static void proto_pause(void);
static int p_draw(int fd, int count, void *data);
static void p_pause_timers();
static void p_resume_timers();

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

static int p_send_data(struct p_data *pd, void *data, int memb_size, unsigned len)
{
	char *msg;
	size_t size, pos = 0;
	int ret;

	size = len * 12 + 1;
	msg = salloc(size);
	for (unsigned i = 0; i < len; i++) {
		int m;

		switch (memb_size) {
		case sizeof(int):
			m = ((int *)data)[i];
			break;
		case 1:
			m = ((unsigned char *)data)[i];
			break;
		default:
			m = 0;
		}
		pos += snprintf(msg + pos, size - pos, " %d", m);
		if (pos >= size)
			break;
	}
	ret = p_send_msg(pd, "DATA", msg + 1);
	sfree(msg);
	return ret;
}

static int p_send_int(struct p_data *pd, int what)
{
	return p_send_data(pd, &what, sizeof(int), 1);
}

static void p_report_and_close(struct p_data *pd, char *cmd, char *msg)
{
	check(socket_stop_reading(pd->s));
	p_send_msg(pd, cmd, msg);
	socket_flush_and_del(pd->s);
}

static void p_report_error(struct p_data *pd, char *msg)
{
	log_info("closing app socket fd %d due to protocol error", socket_get_fd(pd->s));
	p_report_and_close(pd, "OVER", msg);
}

static void p_report_win(struct p_data *pd, char *msg)
{
	char buf[BUF_SIZE + 1];
	int res;

	log_info("winner, running: %s %s %s", PLUMBING, p_login, p_code);
	res = exec_wait(buf, BUF_SIZE + 1, PLUMBING, p_login, p_code, NULL);
	if (res < 0) {
		log_err("error recording the result (%d)", res);
	} else if (!strncmp(buf, "ERROR: ", 7)) {
		log_err("returned %s", buf);
		res = -1;
	} else if (!strncmp(buf, "REPORT: ", 8)) {
		if (buf[res - 1] == '\n')
			buf[--res] = '\0';
		if (res == 8) {
			log_err("error: empty report received");
			res = -1;
		} else {
			msg = buf + 8;
			log_info("reporting %s", msg);
		}
	} else if (strncmp(buf, "OK", 2)) {
		log_err("unexpected output: %s", buf);
		res = -1;
	}
	if (res < 0)
		msg = "Vyskytla se neocekavana chyba pri zaznamenavani vysledku.";
	log_info("closing app socket fd %d", socket_get_fd(pd->s));
	p_report_and_close(pd, "OVER", msg);
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

	if (p_end_set && time_after(&p_end))
		return P_MSG_TIMEOUT;
	if (!strcmp(pd->cmd, "MOVE")) {
		int res;

		if (pd->val_len != 1)
			return P_MSG_CHAR_EXPECTED;
		res = p_level->move(pd->data, pd->val[0], &nope);
		switch (res) {
		case MOVE_OKAY:
			nope = NULL;
			p_send_ack(pd);
			break;
		case MOVE_BAD:
			break;
		case MOVE_WIN:
			p_report_win(pd, nope);
			return NULL;
		case MOVE_LOSE:
			p_report_error(pd, nope);
			return NULL;
		}
	} else if (!strcmp(pd->cmd, "WHAT")) {
		int x, y, res;

		if (!get_2_int(pd->val, &x, &y))
			return P_MSG_2INT_EXPECTED;
		nope = p_level->what(pd->data, x, y, &res);
		if (!nope)
			p_send_int(pd, res);
	} else if (!strcmp(pd->cmd, "MAZE")) {
		unsigned char *res;
		unsigned len;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		if (!p_level->maze)
			return P_MSG_MAZE_NOT_AVAIL;
		nope = p_level->maze(pd->data, &res, &len);
		if (!nope)
			p_send_data(pd, res, 1, len);
	} else if (!strcmp(pd->cmd, "GETX")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_x(pd->data, &res);
		if (!nope)
			p_send_int(pd, res);
	} else if (!strcmp(pd->cmd, "GETY")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_y(pd->data, &res);
		if (!nope)
			p_send_int(pd, res);
	} else if (!strcmp(pd->cmd, "GETW")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_w(pd->data, &res);
		if (!nope)
			p_send_int(pd, res);
	} else if (!strcmp(pd->cmd, "GETH")) {
		int res;

		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;
		nope = p_level->get_h(pd->data, &res);
		if (!nope)
			p_send_int(pd, res);
	} else if (!strcmp(pd->cmd, "WAIT")) {
		if (pd->is_val)
			return P_MSG_EXTRA_PARAM;

		/* The response will be queued but won't be send until the
		 * socket is enabled. */
		p_send_ack(pd);

		proto_pause();
	} else {
		return P_MSG_CMD_UNKNOWN;
	}

	if (nope)
		p_send_nope(pd, nope);
	return NULL;
}

static bool valid_identifier(char *val)
{
	for (; *val; val++) {
		if (*val >= 'a' && *val <= 'z')
			continue;
		if (*val >= '0' && *val <= '9')
			continue;
		return false;
	}
	return true;
}

static char *start_level(char *code)
{
	p_level = app_get_level(code);
	if (!p_level) {
		log_info("unknown level \"%s\"", code);
		return P_MSG_LEVL_UNKNOWN;
	}
	log_info("level \"%s\"", code);
	p_code = sstrdup(code);
	p_bound_max = p_level->max_conn;
	time_from_now(&p_can_pause_until, CAN_PAUSE_INTERVAL);
	if (p_level->max_time) {
		time_from_now(&p_end, p_level->max_time * 1000);
		p_end_set = true;
	}
	p_draw_timer = timer_new(p_draw, NULL, NULL);
	check(p_draw_timer);
	timer_arm(p_draw_timer, REDRAW_INTERVAL, true);
	draw_button(BUTTON_KILL, true);
	return NULL;
}

static char *process_level(struct p_data *pd)
{
	if (strcmp(pd->cmd, "LEVL"))
		return P_MSG_LEVL_EXPECTED;
	if (!valid_identifier(pd->val))
		return P_MSG_LEVL_BAD_CHARS;

	if (p_code && strcmp(pd->val, p_code))
		return P_MSG_LEVL_NOT_MATCHING;
	if (p_bound_count >= p_bound_max)
		return P_MSG_CONN_TOO_MANY;

	if (!p_code) {
		char *res;

		res = start_level(pd->val);
		if (res)
			return res;
	}
	if (p_level->get_data)
		pd->data = p_level->get_data();
	pd->bound = true;
	pd->process = process_cmd;
	p_bound_count++;
	level_dirty();

	p_send_ack(pd);
	if (p_waiting)
		socket_pause(pd->s, true);
	return NULL;
}

static char *process_user(struct p_data *pd)
{
	if (strcmp(pd->cmd, "USER"))
		return P_MSG_USER_EXPECTED;

	if (!db_user_exists(pd->val)) {
		log_info("user \"%s\" not found", pd->val);
		return P_MSG_USER_UNKNOWN;
	}

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

void proto_client_init(char *login, proto_close_cb_t close_cb)
{
	p_sockets = NULL;
	p_login = login;
	p_close_cb = close_cb;
	p_count = 0;
	p_bound_count = 0;
	p_bound_max = 1;
	p_end_set = false;
	p_code = NULL;
	p_level = NULL;
	p_waiting = false;
}

void proto_cond_close(void)
{
	if (!p_count && p_close_cb)
		p_close_cb();
}

static void p_free(void *data)
{
	struct p_data *pd = data;
	struct p_data **ptr;
	bool bound = pd->bound;

	for (ptr = &p_sockets; *ptr && *ptr != pd; ptr = &(*ptr)->next)
		;
	if (*ptr)
		*ptr = pd->next;
	p_count--;

	if (bound)
		p_bound_count--;
	if (p_level && p_level->free_data)
		p_level->free_data(pd->data);
	p_server_free(data);
	if ((!p_count || bound) && p_close_cb)
		p_close_cb();
}

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
	socket_set_ratelimit(pd->s);

	pd->process = process_level;
	pd->crlf = crlf;

	pd->next = p_sockets;
	p_sockets = pd;
	p_count++;

	return p_send_ack(pd);
}

static void p_pause_sockets(bool pause)
{
	struct p_data *pd;

	for (pd = p_sockets; pd; pd = pd->next)
		socket_pause(pd->s, pause);
}

static void proto_pause(void)
{
	if (p_waiting)
		/* Could have two concurrent wait commands, activate
		 * things only on the first one. */
		return;
	p_pause_timers();
	p_pause_sockets(true);
	draw_button(BUTTON_WAIT, true);
	if (p_end_set) {
		if (time_after(&p_can_pause_until))
			p_paused_time = 0;
		else
			p_paused_time = time_left(&p_end);
	}
	p_waiting = true;
}

void proto_resume(void)
{
	if (!p_waiting)
		return;
	p_waiting = false;
	p_pause_sockets(false);
	draw_button(BUTTON_WAIT, false);
	if (p_end_set && p_paused_time)
		time_from_now(&p_end, p_paused_time);
	p_resume_timers();
}

static int p_draw(int fd __unused, int count __unused, void *data __unused)
{
	if (p_end_set) {
		long left;

		if (p_waiting && p_paused_time)
			left = p_paused_time;
		else
			left = time_left(&p_end);
		draw_seconds((left + 999) / 1000);
	}
	app_redraw(p_level);
	return 0;
}

/*** Level timers ***/

#define MAX_TIMERS	16
static int timers[MAX_TIMERS];
static int timers_cnt = 0;

int level_timer_new(timer_callback_t cb, void *cb_data,
		    cb_data_destructor_t cb_destructor)
{
	int res;

	if (timers_cnt >= MAX_TIMERS)
		return -ENOBUFS;
	res = timer_new(cb, cb_data, cb_destructor);
	if (res < 0)
		return res;
	timers[timers_cnt++] = res;
	return res;
}

int level_timer_arm(int fd, int milisec, bool repeat)
{
	return timer_arm(fd, milisec, repeat);
}

int level_timer_disarm(int fd)
{
	return timer_disarm(fd);
}

int level_timer_del(int fd)
{
	for (int i = 0; i < timers_cnt; i++)
		if (fd == timers[i]) {
			memmove(timers + i, timers + i + 1,
				sizeof(timers[0]) * (timers_cnt - i - 1));
			timers_cnt--;
			break;
		}
	return timer_del(fd);
}

static void p_pause_timers()
{
	for (int i = 0; i < timers_cnt; i++)
		timer_pause(timers[i]);
}

static void p_resume_timers()
{
	for (int i = 0; i < timers_cnt; i++)
		timer_resume(timers[i]);
}
