#ifndef IPC_H
#define IPC_H
#include "socket.h"

enum {
	IPC_FD_WEBSOCKET,
	IPC_FD_APP,
};

int ipc_client_init(void);
int ipc_send_fd(struct socket *s, int fd, int type);

#endif
