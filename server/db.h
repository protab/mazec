#ifndef DB_H
#define DB_H
#include <sys/types.h>

#define DB_PATH	"./users"

int db_reload(void);
void db_kill_pid(pid_t pid);
int db_get_pipe(const char *login);

#endif
