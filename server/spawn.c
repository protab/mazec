#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "common.h"
#include "db.h"
#include "event.h"
#include "log.h"

static char *prg_path;

void spawn_init(int __unused argc, char **argv)
{
	prg_path = sstrdup(argv[0]);
}

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

static char **va_list_to_argv(char *prg, va_list ap)
{
	va_list copy;
	int cnt = 1;
	char **res, **cur;

	va_copy(copy, ap);
	while (va_arg(copy, char *))
		cnt++;
	va_end(copy);
	res = salloc(sizeof(char *) * (cnt + 1));
	cur = res;
	*(cur++) = prg;
	while (1) {
		char *a = va_arg(ap, char *);
		*cur++ = a;
		if (!a)
			break;
	}
	return res;
}

int exec_wait(char *out, int out_size, char *prg, ...)
{
	pid_t pid;
	int fd[2];
	va_list ap;
	char **argv;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd) < 0)
		return -errno;

	va_start(ap, prg);
	argv = va_list_to_argv(prg, ap);
	va_end(ap);

	pid = fork();
	if (pid < 0)
		goto error;
	if (pid > 0) {
		/* parent */
		ssize_t len;
		char buf[1024];
		int remains = out_size - 1;
		int res = 0;

		close(fd[1]);
		sfree(argv);
		event_ignore_pid(pid);

		log_info("spawned child pid %d", pid);
		while (true) {
			if (!out_size)
				len = read(fd[0], buf, 1024);
			else
				len = read(fd[0], out, remains);
			if (len < 0) {
				if (errno == EINTR)
					continue;
				res = -errno;
				break;
			}
			if (!len) {
				res = out_size - remains - 1;
				*out = '\0';
				break;
			}
			out += len;
			remains -= len;
		}
		close(fd[0]);
		return res;
	}
	close(fd[0]);
	if (dup2(fd[1], 1) < 0) {
		log_err("child pid %d: dup2 error: %s", getpid(), strerror(errno));
		exit(10);
	}
	close(fd[1]);

	execv(prg, argv);
	log_err("child pid %d: execv error: %s", getpid(), strerror(errno));
	exit(11);

error: ;
	int ret = -errno;

	sfree(argv);
	close(fd[0]);
	close(fd[1]);
	return ret;
}
