#include "socket.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "common.h"
#include "event.h"
#include "log.h"

struct msg {
	void *buf;
	size_t size, start;
	struct msg *next;
};

struct socket {
	int fd;
	bool should_close;
	socket_cb_read_t cb_read;
	socket_cb_read_t cb_write_done;
	void *cb_data;
	cb_data_destructor_t cb_destructor;
	struct msg *wqueue;
};

static void socket_process_wqueue(struct socket *s);

static int socket_cb(int fd, unsigned events, void *data)
{
	struct socket *s = data;

	if (events & EV_ERROR) {
		log_info("socket %d was closed by the other side", fd);
		socket_del(s);
		return 0;
	}
	if (events & EV_READ)
		s->cb_read(s, s->cb_data);
	if (events & EV_WRITE)
		socket_process_wqueue(s);
	return 0;
}

static void socket_destroy(void *data)
{
	struct socket *s = data;

	if (s->cb_destructor)
		s->cb_destructor(s->cb_data);
	if (s->should_close)
		close(s->fd);
	free(s);
}

/* Add the socket to the socket management core. The file descriptor is
 * stolen and the caller must not use it after this call, unless it calls
 * socket_set_unmanaged. */
struct socket *socket_add(int fd, socket_cb_read_t cb_read, void *cb_data,
			  cb_data_destructor_t cb_destructor)
{
	struct socket *s;

	s = malloc(sizeof(*s));
	if (!s)
		return NULL;
	s->fd = fd;
	s->should_close = true;
	s->cb_read = cb_read;
	s->cb_write_done = NULL;
	s->cb_data = cb_data;
	s->cb_destructor = cb_destructor;
	if (event_add_fd(fd, EV_SOCK | EV_READ, socket_cb, s, socket_destroy) < 0) {
		free(s);
		return NULL;
	}
	return s;
}

/* This is one shot callback! */
void socket_set_write_done_cb(struct socket *s, socket_cb_read_t cb_write_done)
{
	s->cb_write_done = cb_write_done;
}

/* Sets the socket as unmanaged. The caller is responsible for closing the
 * fd. After this call, the caller may choose not to use managed writes
 * (socket_write). However, managed and unmanaged writes must not be mixed.
 * Reads are always managed.
 * Returns the file descriptor. */
int socket_set_unmanaged(struct socket *s)
{
	s->should_close = false;
	return s->fd;
}

/* Use only for logging purposes. */
int socket_get_fd(struct socket *s)
{
	return s->fd;
}

int socket_stop_reading(struct socket *s)
{
	return event_change_fd_remove(s->fd, EV_READ);
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

size_t socket_read(struct socket *s, void *buf, size_t size)
{
	ssize_t ret;

	ret = read(s->fd, buf, size);
	if (ret < 0)
		ret = 0;
	return ret;
}

static int socket_queue_data(struct socket *s, void *buf, size_t size)
{
	struct msg *m, **ptr;

	m = malloc(sizeof(*m));
	if (!m)
		return -errno;
	m->buf = buf;
	m->size = size;
	m->start = 0;
	m->next = NULL;

	for (ptr = &s->wqueue; *ptr; ptr = &(*ptr)->next)
		;
	*ptr = m;
	return 0;
}

int socket_write(struct socket *s, void *buf, size_t size, bool steal)
{
	int ret;
	void *copied;

	ret = event_change_fd_add(s->fd, EV_WRITE);
	if (ret < 0)
		return ret;

	if (steal)
		copied = buf;
	else {
		copied = malloc(size);
		if (!copied)
			return -errno;
		memcpy(copied, buf, size);
	}
	ret = socket_queue_data(s, copied, size);
	if (ret < 0 && !steal)
		free(copied);
	return ret;
}

static void socket_process_wqueue(struct socket *s)
{
	ssize_t written;

	while (s->wqueue) {
		struct msg *m = s->wqueue;

		written = write(s->fd, m->buf + m->start, m->size);
		if (written < 0)
			/* We might just get EAGAIN. Real errors are handled in
			* socket_cb via EV_ERROR. */
			return;
		if ((size_t)written == m->size) {
			s->wqueue = m->next;
			free(m->buf);
			free(m);
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

struct listen_data {
	unsigned port;
	socket_cb_new_t cb_new;
	socket_cb_read_t cb_read;
	cb_data_destructor_t cb_destructor;
};

void socket_accept(struct socket *s, void *data)
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
		log_info("accepting connection on port %u from %s on fd %d", ldata->port,
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

/* Starts listening on the given port. All new connections are automatically
 * accepted and added to the socket managament core. The cb_new callback is
 * called to provide cb_data for the new connection. That data is destructed
 * by the cb_destructor. The cb_read and cb_destructor parameters are
 * related to the spawned connection sockets, not to the listening socket! */
int socket_listen(unsigned port, socket_cb_new_t cb_new,
		  socket_cb_read_t cb_read,
		  cb_data_destructor_t cb_destructor)
{
	struct listen_data *ldata;
	int fd;
	struct sockaddr_in6 sin6;
	int tmp;

	ldata = malloc(sizeof(*ldata));
	if (!ldata)
		return -errno;
	ldata->port = port;
	ldata->cb_new = cb_new;
	ldata->cb_read = cb_read;
	ldata->cb_destructor = cb_destructor;

	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0) {
		free(ldata);
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
		errno = -ENOMEM;
		goto error;
	}

	return 0;

error: ;
	int ret = errno;

	close(fd);
	free(ldata);
	return -ret;
}
