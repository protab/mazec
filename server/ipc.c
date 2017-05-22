#include "ipc.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include "db.h"
#include "event.h"
#include "log.h"
#include "proto.h"
#include "socket.h"
#include "websocket_data.h"

#define IDLE_TIMEOUT	500
static int idle_timer;

static void cancel_idle_timer(void)
{
	if (idle_timer < 0)
		return;
	timer_del(idle_timer);
	idle_timer = -1;
}

#define BUF_SIZE	512
static void pipe_read(struct socket *s, void *data __unused)
{
	char buf[BUF_SIZE];
	union {
		char ancil[BUF_SIZE];
		struct cmsghdr cmsg;
	} u;
	size_t buf_len;
	size_t ancil_len = BUF_SIZE;

	buf_len = socket_read_ancil(s, buf, BUF_SIZE, u.ancil, &ancil_len);
	if (ancil_len) {
		int fd;
		int type;

		if (u.cmsg.cmsg_level != SOL_SOCKET || u.cmsg.cmsg_type != SCM_RIGHTS) {
			log_info("received unknown ancillary message");
			return;
		}
		if (u.cmsg.cmsg_len != CMSG_LEN(sizeof(int))) {
			log_info("received ancillary message with unexpected length");
			return;
		}
		memcpy(&fd, CMSG_DATA(&u.cmsg), sizeof(int));
		if (buf_len != sizeof(int)) {
			log_info("received fd %d with no type", fd);
			close(fd);
			return;
		}
		memcpy(&type, buf, sizeof(int));
		if (type == IPC_FD_WEBSOCKET) {
			log_info("received websocket fd %d", fd);
			if (websocket_add(fd) < 0) {
				close(fd);
				return;
			}
		} else if (type == IPC_FD_APP_CRLF || type == IPC_FD_APP_LF) {
			log_info("received app socket fd %d (type %d)", fd, type);
			if (proto_client_add(fd, type == IPC_FD_APP_CRLF) < 0)
				return;
		} else {
			log_info("received fd %d of unknown type %d", fd, type);
			close(fd);
			return;
		}
		cancel_idle_timer();
	}
}

static int idle_expired(int fd __unused, unsigned events, void *data __unused)
{
	if (events != EV_READ)
		return 1;
	log_info("no connection in %d ms, terminating", IDLE_TIMEOUT);
	event_quit();
	return 0;
}

int ipc_client_init(void)
{
	struct socket *s;

	s = socket_add(2, pipe_read, NULL, NULL);
	if (!s)
		return -ENOTSOCK;
	/* We never close fd 2. */
	socket_set_unmanaged(s);

	idle_timer = timer_new(idle_expired, NULL, NULL);
	if (idle_timer < 0)
		return idle_timer;
	timer_arm(idle_timer, IDLE_TIMEOUT);

	return 0;
}

static int ipc_send_fd(struct socket *s, int fd, int type)
{
	struct cmsghdr *cmsg;
	size_t len;
	int ret;

	len = CMSG_LEN(sizeof(int));
	cmsg = salloc(len);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = len;
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
	ret = socket_write_ancil(s, &type, sizeof(int), false, cmsg, len, true, fd);
	if (ret < 0) {
		sfree(cmsg);
		return ret;
	}
	return 0;
}

static const char *str_type(int type)
{
	if (type == IPC_FD_WEBSOCKET)
		return "websocket";
	return "app socket";
}

void ipc_send_socket(char *login, struct socket *what, int type)
{
	struct socket *pipe;
	int fd;

	socket_del(what);
	pipe = db_get_pipe(login);
	fd = socket_get_fd(what);
	if (!pipe || ipc_send_fd(pipe, fd, type) < 0) {
		log_warn("unable to send %s fd %d to child [%s]", str_type(type), fd, login);
	} else {
		socket_set_unmanaged(what);
		log_info("sending %s fd %d to child [%s]", str_type(type), fd, login);
	}
}
