#ifndef WEBSOCKET_DATA_H
#define WEBSOCKET_DATA_H
#include <stdbool.h>
#include "socket.h"

typedef void (*websocket_cb_t)(struct socket *s, void *buf, size_t len);

void websocket_init(websocket_cb_t cb);
int websocket_add(int fd);
void websocket_write(struct socket *s, void *buf, size_t size, bool steal);
void websocket_broadcast(void *buf, size_t size);

#endif
