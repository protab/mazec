#ifndef PROTO_H
#define PROTO_H
#include <stdbool.h>

/* called when the last app socket is closed or when a bound app socket with
 * protocol error is closed */
typedef void (*proto_close_cb_t)(void);

int proto_server_init(unsigned port);
void proto_client_init(char *login, proto_close_cb_t close_cb);

/* Closes the fd even if unsuccessful. */
int proto_client_add(int fd, bool crlf);

/* Calls the close callback if there is no app socket open. */
void proto_cond_close(void);

void proto_resume(void);

#endif
