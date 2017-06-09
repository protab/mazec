#ifndef SOCKET_H
#define SOCKET_H
#include <stdbool.h>
#include "event.h"

struct socket;

typedef void (*socket_cb_read_t)(struct socket *s, void *data);
/* Returns cb_data. */
typedef void *(*socket_cb_new_t)(struct socket *s);

/* Add the socket to the socket management core. The file descriptor is
 * stolen and the caller must not use it after this call, unless it calls
 * socket_set_unmanaged. */
struct socket *socket_add(int fd, socket_cb_read_t cb_read, void *cb_data,
			  cb_data_destructor_t cb_destructor);

/* This is one shot callback! */
void socket_set_write_done_cb(struct socket *s, socket_cb_read_t cb_write_done);

/* Sets the socket as unmanaged. The caller is responsible for closing the
 * fd. After this call, the caller may choose not to use managed writes
 * (socket_write). However, managed and unmanaged writes must not be mixed.
 * Reads are always managed.
 * Returns the file descriptor. */
int socket_set_unmanaged(struct socket *s);

int socket_stop_reading(struct socket *s);
int socket_pause(struct socket *s, bool pause);

/* May be called multiple times, may be called even when the socket is dead.
 * However, must not be called on a socket that was freed (i.e. has zero
 * reference count). It is safe to call socket_del from that socket's
 * callbacks without taking a reference, as socket is freed only after the
 * current round of callbacks is finished. It's also safe to keep sockets on
 * a list, provided that the socket is removed from that list by the socket
 * destructor and no other pointers to the socket are kept. It is NOT safe
 * to keep socket pointers around (such as a list unsynchronized with the
 * socket destructor) without having a reference. */
void socket_del(struct socket *s);

void socket_flush_and_del(struct socket *s);
void socket_ref(struct socket *s);
void socket_unref(struct socket *s);
size_t socket_read(struct socket *s, void *buf, size_t size);
size_t socket_read_ancil(struct socket *s, void *buf, size_t size,
			 void *ancil_buf, size_t *ancil_size);
int socket_write(struct socket *s, void *buf, size_t size, bool steal);
int socket_write_ancil(struct socket *s, void *buf, size_t size, bool steal,
		       void *ancil_buf, size_t ancil_size, bool ancil_steal,
		       int fd_to_close);

/* Use carefully. Do not store the returned fd anywhere, do not perform I/O
 * that could interfere with the socket management core operations. */
int socket_get_fd(struct socket *s);

/* Starts listening on the given port. All new connections are automatically
 * accepted and added to the socket managament core. The cb_new callback is
 * called to provide cb_data for the new connection. That data is destructed
 * by the cb_destructor. The cb_read and cb_destructor parameters are
 * related to the spawned connection sockets, not to the listening socket! */
int socket_listen(unsigned port, socket_cb_new_t cb_new,
		  socket_cb_read_t cb_read,
		  cb_data_destructor_t cb_destructor);

#endif
