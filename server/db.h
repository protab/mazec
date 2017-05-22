#ifndef DB_H
#define DB_H
#include <stdbool.h>
#include <sys/types.h>
#include "socket.h"

#define DB_PATH		"./users"
#define LOGIN_LEN	30

int db_init(void);
int db_reload(void);
void db_start_process(const char *login, pid_t pid, int pipefd);
void db_end_process(pid_t pid);
bool db_user_exists(const char *login);
struct socket *db_get_pipe(const char *login);

#endif
