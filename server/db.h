#ifndef DB_H
#define DB_H
#include <sys/types.h>

#define DB_PATH	"./users"

int db_reload(void);
void db_start_process(const char *login, pid_t pid, int pipefd);
void db_end_process(pid_t pid);
int db_get_pipe(const char *login);

#endif