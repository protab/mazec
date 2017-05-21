#include "ipc.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include "event.h"
#include "log.h"
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
			cancel_idle_timer();
		} else {
			log_info("received fd %d of unknown type %d", fd, type);
			close(fd);
			return;
		}
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
		return -ENOMEM;
	/* We never close fd 2. */
	socket_set_unmanaged(s);

	idle_timer = timer_new(idle_expired, NULL, NULL);
	if (idle_timer < 0)
		return idle_timer;
	timer_arm(idle_timer, IDLE_TIMEOUT);

	return 0;
}

int ipc_send_fd(struct socket *s, int fd, int type)
{
	struct cmsghdr *cmsg;
	size_t len;
	int ret;

	len = CMSG_LEN(sizeof(int));
	cmsg = malloc(len);
	if (!cmsg)
		return -errno;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = len;
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
	ret = socket_write_ancil(s, &type, sizeof(int), false, cmsg, len, true, fd);
	if (ret < 0) {
		free(cmsg);
		return ret;
	}
	return 0;
}
