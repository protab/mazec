#include "socket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
#include "event.h"
#include "log.h"

struct msg {
	void *buf;
	size_t size, start;
	void *ancil_buf;
	size_t ancil_size;
	int fd_to_close;
	struct msg *next;
};

struct socket {
	int refs;
	int fd;
	bool dead;
	bool should_close;
	socket_cb_read_t cb_read;
	socket_cb_read_t cb_write_done;
	void *cb_data;
	cb_data_destructor_t cb_destructor;
	struct msg *wqueue;
	int wqueue_len;
};

#define WQUEUE_MAX_LEN	20

static void socket_process_wqueue(struct socket *s);
static void socket_del_wqueue(struct socket *s);

static int socket_cb(int fd, unsigned events, void *data)
{
	struct socket *s = data;

	if (events & EV_READ)
		s->cb_read(s, s->cb_data);
	if (events & EV_ERROR) {
		log_info("socket %d was closed by the other side", fd);
		socket_del(s);
		return 0;
	}
	if (events & EV_WRITE)
		socket_process_wqueue(s);
	return 0;
}

static void socket_kill(void *data)
{
	struct socket *s = data;

	s->dead = true;
	socket_unref(s);
}

struct socket *socket_add(int fd, socket_cb_read_t cb_read, void *cb_data,
			  cb_data_destructor_t cb_destructor)
{
	struct socket *s;

	s = salloc(sizeof(*s));
	s->refs = 1;
	s->fd = fd;
	s->dead = false;
	s->should_close = true;
	s->cb_read = cb_read;
	s->cb_write_done = NULL;
	s->cb_data = cb_data;
	s->cb_destructor = cb_destructor;
	s->wqueue = NULL;
	s->wqueue_len = 0;
	if (event_add_fd(fd, EV_SOCK | EV_READ, socket_cb, s, socket_kill) < 0) {
		sfree(s);
		return NULL;
	}
	return s;
}

void socket_set_write_done_cb(struct socket *s, socket_cb_read_t cb_write_done)
{
	s->cb_write_done = cb_write_done;
}

int socket_set_unmanaged(struct socket *s)
{
	s->should_close = false;
	return s->fd;
}

int socket_get_fd(struct socket *s)
{
	return s->fd;
}

int socket_stop_reading(struct socket *s)
{
	if (s->dead)
		return 0;
	return event_change_fd_remove(s->fd, EV_READ);
}

int socket_pause(struct socket *s, bool pause)
{
	if (s->dead)
		return 0;
	return event_pause_fd(s->fd, pause);
}

void socket_del(struct socket *s)
{
	event_del_fd(s->fd);
}

static void socket_del_cb(struct socket *s, void *data __unused)
{
	socket_del(s);
}

void socket_flush_and_del(struct socket *s)
{
	if (!s->wqueue)
		socket_del(s);
	else
		socket_set_write_done_cb(s, socket_del_cb);
}

void socket_ref(struct socket *s)
{
	s->refs++;
}

void socket_unref(struct socket *s)
{
	if (--s->refs == 0) {
		socket_del_wqueue(s);
		if (s->cb_destructor)
			s->cb_destructor(s->cb_data);
		if (s->should_close)
			close(s->fd);
		sfree(s);
	}
}

size_t socket_read_ancil(struct socket *s, void *buf, size_t size,
			 void *ancil_buf, size_t *ancil_size)
{
	struct msghdr mh;
	struct iovec iov;
	ssize_t ret;

	if (s->dead) {
		*ancil_size = 0;
		return 0;
	}

	iov.iov_base = buf;
	iov.iov_len = size;

	mh.msg_name = NULL;
	mh.msg_namelen = 0;
	mh.msg_iov = &iov;
	mh.msg_iovlen = 1;
	mh.msg_control = ancil_buf;
	mh.msg_controllen = *ancil_size;
	mh.msg_flags = 0;

	ret = recvmsg(s->fd, &mh, 0);
	if (ret < 0) {
		ret = 0;
		*ancil_size = 0;
	} else {
		*ancil_size = mh.msg_controllen;
	}
	return ret;
}

size_t socket_read(struct socket *s, void *buf, size_t size)
{
	size_t ancil_size = 0;

	return socket_read_ancil(s, buf, size, NULL, &ancil_size);
}

static void socket_queue_data(struct socket *s, void *buf, size_t size,
			      void *ancil_buf, size_t ancil_size,
			      int fd_to_close)
{
	struct msg *m, **ptr;

	m = salloc(sizeof(*m));
	m->buf = buf;
	m->size = size;
	m->start = 0;
	m->ancil_buf = ancil_buf;
	m->ancil_size = ancil_size;
	m->fd_to_close = fd_to_close;
	m->next = NULL;

	for (ptr = &s->wqueue; *ptr; ptr = &(*ptr)->next)
		;
	*ptr = m;

	s->wqueue_len++;
}

int socket_write_ancil(struct socket *s, void *buf, size_t size, bool steal,
		       void *ancil_buf, size_t ancil_size, bool ancil_steal,
		       int fd_to_close)
{
	int ret;
	void *copied;
	void *ancil_copied;

	if (s->dead) {
		if (steal)
			sfree(buf);
		if (ancil_steal)
			sfree(ancil_buf);
		return 0;
	}

	if (s->wqueue_len >= WQUEUE_MAX_LEN)
		return -ENOBUFS;

	ret = event_change_fd_add(s->fd, EV_WRITE);
	if (ret < 0)
		return ret;

	if (steal)
		copied = buf;
	else {
		copied = salloc(size);
		memcpy(copied, buf, size);
	}
	if (!ancil_buf || ancil_steal) {
		ancil_copied = ancil_buf;
	} else {
		ancil_copied = salloc(ancil_size);
		memcpy(ancil_copied, ancil_buf, ancil_size);
	}

	socket_queue_data(s, copied, size, ancil_copied, ancil_size, fd_to_close);
	return 0;
}

int socket_write(struct socket *s, void *buf, size_t size, bool steal)
{
	return socket_write_ancil(s, buf, size, steal, NULL, 0, false, 0);
}

static void socket_process_wqueue(struct socket *s)
{
	while (s->wqueue) {
		struct msg *m = s->wqueue;
		struct msghdr mh;
		struct iovec iov;
		ssize_t written;

		iov.iov_base = m->buf + m->start;
		iov.iov_len = m->size;

		mh.msg_name = NULL;
		mh.msg_namelen = 0;
		mh.msg_iov = &iov;
		mh.msg_iovlen = 1;
		mh.msg_control = m->ancil_buf;
		mh.msg_controllen = m->ancil_size;
		mh.msg_flags = 0;

		written = sendmsg(s->fd, &mh, 0);
		if (written < 0 && (errno == EAGAIN || errno == EINTR))
			return;
		if (written != 0) {
			if (m->ancil_buf) {
				sfree(m->ancil_buf);
				m->ancil_buf = NULL;
				m->ancil_size = 0;
			}
			if (m->fd_to_close) {
				close(m->fd_to_close);
				m->fd_to_close = 0;
			}
		}
		if (written < 0 || (size_t)written == m->size) {
			s->wqueue = m->next;
			s->wqueue_len--;
			sfree(m->buf);
			sfree(m);
		} else {
			m->size -= written;
			m->start += written;
			return;
		}
	}

	/* no more messages queued */
	event_change_fd_remove(s->fd, EV_WRITE);
	if (s->cb_write_done) {
		s->cb_write_done(s, s->cb_data);
		s->cb_write_done = NULL;
	}
}

static void socket_del_wqueue(struct socket *s)
{
	while (s->wqueue) {
		struct msg *m = s->wqueue;

		s->wqueue = m->next;
		if (m->ancil_buf)
			sfree(m->ancil_buf);
		if (m->fd_to_close)
			close(m->fd_to_close);
		sfree(m->buf);
		sfree(m);
	}
	s->wqueue_len = 0;
}

struct listen_data {
	unsigned port;
	socket_cb_new_t cb_new;
	socket_cb_read_t cb_read;
	cb_data_destructor_t cb_destructor;
};

static void socket_accept(struct socket *s, void *data)
{
	struct listen_data *ldata = data;
	struct sockaddr_in6 sin6;
	socklen_t sin6_len;
	int fd;
	char buf[320];
	struct socket *conn_s;

	while (true) {
		sin6_len = sizeof(sin6);
		fd = accept4(s->fd, (struct sockaddr *)&sin6, &sin6_len,
			     SOCK_NONBLOCK | SOCK_CLOEXEC);
		if (fd < 0)
			return;
		// FIXME: convert IPv4-mapped IPv6 address into IPv4 format
		log_info("accepting connection on port %u from %s with fd %d", ldata->port,
			inet_ntop(sin6.sin6_family, &sin6.sin6_addr, buf, sizeof(buf)), fd);
		conn_s = socket_add(fd, ldata->cb_read, NULL, ldata->cb_destructor);
		if (!conn_s) {
			close(fd);
			return;
		}
		if (ldata->cb_new)
			conn_s->cb_data = ldata->cb_new(conn_s);
	}
}

int socket_listen(unsigned port, socket_cb_new_t cb_new,
		  socket_cb_read_t cb_read,
		  cb_data_destructor_t cb_destructor)
{
	struct listen_data *ldata;
	int fd;
	struct sockaddr_in6 sin6;
	int tmp;

	ldata = salloc(sizeof(*ldata));
	ldata->port = port;
	ldata->cb_new = cb_new;
	ldata->cb_read = cb_read;
	ldata->cb_destructor = cb_destructor;

	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0) {
		sfree(ldata);
		return -errno;
	}
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
		goto error;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		goto error;
	tmp = 0;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &tmp, sizeof(tmp)) < 0)
		goto error;
	tmp = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp)) < 0)
		goto error;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(port);
	if (bind(fd, (struct sockaddr *)&sin6, sizeof(sin6)) < 0)
		goto error;

	if (listen(fd, 128) < 0)
		goto error;

	if (!socket_add(fd, socket_accept, ldata, free)) {
		errno = -EBADF;
		goto error;
	}

	return 0;

error: ;
	int ret = errno;

	close(fd);
	sfree(ldata);
	return -ret;
}
