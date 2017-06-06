#include "event.h"
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include "db.h"
#include "log.h"
#include "spawn.h"

struct fd_data {
	int fd;
	unsigned events;
	bool enabled;
	event_callback_t cb;
	void *cb_data;
	cb_data_destructor_t cb_destructor;
	struct fd_data *next;
};

static int epfd;
static int sigfd;
static bool quit;

#define FDD_HASH_SIZE	256
static struct fd_data *fdd_hash[FDD_HASH_SIZE];

#define hash(v)		((v) % FDD_HASH_SIZE)

static struct fd_data *deleted = NULL;

static int sig_handler(int fd, unsigned events __unused, void *data __unused)
{
	struct signalfd_siginfo ssi;

	while (read(fd, &ssi, sizeof(ssi)) > 0) {
		switch (ssi.ssi_signo) {
		case SIGINT:
		case SIGTERM:
			event_quit();
			break;
		case SIGHUP:
			db_reload();
			break;
		case SIGCHLD: ;
			int status;
			pid_t pid;

			while (true) {
				pid = waitpid(-1, &status, WNOHANG);
				if (pid <= 0)
					break;
				if (WIFEXITED(status))
					log_info("child %d exited with status %d", pid, WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					log_info("child %d was killed with signal %d", pid, WTERMSIG(status));
				else
					log_info("child %d weirdly exited (%d)", pid, status);
				db_end_process(pid);
			}
			break;
		}
	}
	return 0;
}

int event_init(void)
{
	sigset_t sigs;
	int ret;

	memset(fdd_hash, 0, sizeof(fdd_hash));

	epfd = epoll_create1(EPOLL_CLOEXEC);
	if (epfd < 0)
		return -errno;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGCHLD);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGUSR1);
	sigaddset(&sigs, SIGUSR2);
	if (sigprocmask(SIG_SETMASK, &sigs, NULL) < 0)
		return -errno;
	sigfd = signalfd(-1, &sigs, SFD_NONBLOCK | SFD_CLOEXEC);
	ret = event_add_fd(sigfd, EV_READ, sig_handler, NULL, NULL);
	return ret;
}

int event_add_fd(int fd, unsigned events, event_callback_t cb, void *cb_data,
		 cb_data_destructor_t cb_destructor)
{
	struct fd_data *fdd, **ptr;
	struct epoll_event e;

	fdd = salloc(sizeof(*fdd));
	fdd->fd = fd;
	fdd->events = events;
	fdd->enabled = true;
	fdd->cb = cb;
	fdd->cb_data = cb_data;
	fdd->cb_destructor = cb_destructor;
	fdd->next = NULL;

	e.events = events;
	e.data.ptr = fdd;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e) < 0) {
		sfree(fdd);
		return -errno;
	}

	for (ptr = &fdd_hash[hash(fd)]; *ptr; ptr = &(*ptr)->next)
		;
	*ptr = fdd;
	return 0;
}

int event_del_fd(int fd)
{
	struct fd_data *fdd, **ptr;
	int ret = 0;

	if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
		ret = -errno;

	for (ptr = &fdd_hash[hash(fd)]; *ptr && (*ptr)->fd != fd; ptr = &(*ptr)->next)
		;
	fdd = *ptr;
	if (fdd) {
		*ptr = fdd->next;
		fdd->next = deleted;
		deleted = fdd;
	}
	return ret;
}

static struct fd_data *find_event(int fd)
{
	struct fd_data *fdd;

	for (fdd = fdd_hash[hash(fd)]; fdd && fdd->fd != fd; fdd = fdd->next)
		;
	return fdd;
}

static int event_change_fd_data(struct fd_data *fdd, int enable)
{
	struct epoll_event e;
	int op;

	if (!fdd->enabled) {
		if (enable > 0)
			op = EPOLL_CTL_ADD;
		else
			return 0;
	} else {
		if (enable > 0)
			return 0;
		op = enable < 0 ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
	}

	e.events = fdd->events;
	e.data.ptr = fdd;

	if (epoll_ctl(epfd, op, fdd->fd, &e) < 0)
		return -errno;
	if (enable >= 0)
		fdd->enabled = enable;
	return 0;
}

int event_enable_fd(int fd, bool enable)
{
	struct fd_data *fdd = find_event(fd);

	if (!fdd)
		return -ENOENT;
	return event_change_fd_data(fdd, (int)enable);
}

int event_change_fd(int fd, unsigned events)
{
	struct fd_data *fdd = find_event(fd);

	if (!fdd)
		return -ENOENT;
	fdd->events = events;
	return event_change_fd_data(fdd, -1);
}

int event_change_fd_add(int fd, unsigned events)
{
	struct fd_data *fdd = find_event(fd);

	if (!fdd)
		return -ENOENT;
	fdd->events |= events;
	return event_change_fd_data(fdd, -1);
}

int event_change_fd_remove(int fd, unsigned events)
{
	struct fd_data *fdd = find_event(fd);

	if (!fdd)
		return -ENOENT;
	fdd->events &= ~events;
	return event_change_fd_data(fdd, -1);
}

static void release_deleted(void)
{
	while (deleted) {
		struct fd_data *next = deleted->next;

		if (deleted->cb_destructor)
			deleted->cb_destructor(deleted->cb_data);
		sfree(deleted);
		deleted = next;
	}
}

#define EVENTS_MAX	64

int event_loop(void)
{
	struct epoll_event evbuf[EVENTS_MAX];
	int cnt;
	int ret = 0;

	quit = false;
	while (ret >= 0 && !quit) {
		release_deleted();
		/* re-check as a destructor may have called event_quit */
		if (quit)
			break;
		cnt = epoll_wait(epfd, evbuf, EVENTS_MAX, -1);
		if (cnt < 0) {
			if (errno == EINTR)
				continue;
			ret = -errno;
			break;
		}
		for (int i = 0; i < cnt; i++) {
			struct fd_data *fdd = evbuf[i].data.ptr;

			ret = fdd->cb(fdd->fd, evbuf[i].events, fdd->cb_data);
			if (ret > 0)
				event_change_fd_data(fdd, 0);
			else if (ret < 0)
				break;
		}
	}
	return ret < 0 ? ret : 0;
}

void event_quit(void)
{
	quit = true;
}

int timer_new(event_callback_t cb, void *cb_data,
	      cb_data_destructor_t cb_destructor)
{
	int fd;
	int ret;

	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0)
		return -errno;
	ret = event_add_fd(fd, EV_READ, cb, cb_data, cb_destructor);
	if (ret < 0) {
		close(fd);
		return ret;
	}
	return fd;
}

/* pass 0 in milisecs to disarm */
int timer_arm(int fd, int milisecs, bool repeat)
{
	struct itimerspec its;

	memset(&its, 0, sizeof(its));
	time_add(&its.it_value, milisecs);
	if (repeat)
		time_add(&its.it_interval, milisecs);
	if (timerfd_settime(fd, 0, &its, NULL) < 0)
		return -errno;
	return 0;
}

int timer_snooze(int fd)
{
	uint64_t val;

	val = 0;
	read(fd, &val, sizeof(val));
	if (val > INT_MAX)
		val = INT_MAX;
	return val;
}

int timer_del(int fd)
{
	close(fd);
	return event_del_fd(fd);
}
