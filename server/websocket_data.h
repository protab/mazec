#ifndef WEBSOCKET_DATA_H
#define WEBSOCKET_DATA_H
#include <stdbool.h>
#include "socket.h"

/* called when a message is received on a websocket */
typedef void (*websocket_cb_t)(struct socket *s, void *buf, size_t len);
/* called when the last websocket is closed */
typedef void (*websocket_close_cb_t)(void);

void websocket_init(websocket_cb_t cb, websocket_close_cb_t close_cb);
int websocket_add(int fd);
void websocket_broadcast(void *buf, size_t size);
bool websocket_connected(void);

#endif
