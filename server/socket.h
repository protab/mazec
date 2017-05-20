#ifndef SOCKET_H
#define SOCKET_H
#include <stdbool.h>
#include "event.h"

struct socket;

typedef void (*socket_cb_read_t)(struct socket *s, void *data);
/* Returns cb_data. */
typedef void *(*socket_cb_new_t)(struct socket *s);

struct socket *socket_add(int fd, socket_cb_read_t cb_read, void *cb_data,
			  cb_data_destructor_t cb_destructor);
void socket_set_write_done_cb(struct socket *s, socket_cb_read_t cb_write_done);
int socket_set_unmanaged(struct socket *s);
int socket_stop_reading(struct socket *s);
void socket_del(struct socket *s);
void socket_flush_and_del(struct socket *s);
void socket_ref(struct socket *s);
void socket_unref(struct socket *s);
size_t socket_read(struct socket *s, void *buf, size_t size);
int socket_write(struct socket *s, void *buf, size_t size, bool steal);
int socket_get_fd(struct socket *s);

int socket_listen(unsigned port, socket_cb_new_t cb_new,
		  socket_cb_read_t cb_read,
		  cb_data_destructor_t cb_destructor);

#endif
