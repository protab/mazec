#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "common.h"
#include "db.h"

static char *prg_path;

int spawn_init(int __unused argc, char **argv)
{
	prg_path = strdup(argv[0]);
	if (!prg_path)
		return -errno;
	return 0;
}

/* The caller is responsible for checking that the given login exists. */
int spawn(const char *login)
{
	pid_t pid;
	int fd[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0)
		return -errno;
	if (fcntl(fd[0], F_SETFL, O_NONBLOCK) < 0)
		goto error;

	pid = fork();
	if (pid < 0)
		goto error;
	if (pid > 0) {
		/* parent */
		close(fd[1]);
		db_start_process(login, pid, fd[0]);
		return 0;
	}
	close(fd[0]);
	if (dup2(fd[1], 2) < 0)
		exit(10);
	close(fd[1]);
	execlp(prg_path, prg_path, login, NULL);
	exit(11);

error: ;
	int ret = -errno;

	close(fd[0]);
	close(fd[1]);
	return ret;
}
