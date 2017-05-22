#ifndef IPC_H
#define IPC_H
#include "socket.h"

enum {
	IPC_FD_WEBSOCKET,
	IPC_FD_APP_LF,
	IPC_FD_APP_CRLF,
};

int ipc_client_init(void);
void ipc_send_socket(char *login, struct socket *what, int type);

#endif
