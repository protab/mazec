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
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include "db.h"
#include "log.h"
#include "spawn.h"
#include "time.h"

struct fd_data {
	int fd;
	unsigned events;
	bool enabled;
	event_callback_t cb;
	void *cb_data;
	cb_data_destructor_t cb_destructor;
	struct fd_data *next;
};

struct timer_data {
	timer_callback_t cb;
	void *cb_data;
	cb_data_destructor_t cb_destructor;
	struct itimerspec resume_time;
	int resume_backlog;
	bool armed;
	bool paused;
};

static int epfd;
static int sigfd;
static pid_t ignored_pid;
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
				if (pid == ignored_pid)
					ignored_pid = 0;
				else
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
	ignored_pid = 0;

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

static void *find_cb_data(int fd)
{
	struct fd_data *fdd = find_event(fd);

	if (fdd)
		return fdd->cb_data;
	return NULL;
}

/* enable values: 1 to add to the poll set, 0 to remove, -1 to change, -2 to
 * change to listen for errors only */
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
	if (enable == -2)
		e.events &= EV_ERROR;
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

int event_pause_fd(int fd, bool pause)
{
	struct fd_data *fdd = find_event(fd);

	if (!fdd)
		return -ENOENT;
	return event_change_fd_data(fdd, pause ? -2 : -1);
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

void event_ignore_pid(pid_t pid)
{
	ignored_pid = pid;
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
	while (!ret && !quit) {
		release_deleted();
		/* re-check as a destructor may have called event_quit */
		if (quit)
			break;
		time_flush_cache();
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
			if (ret < 0)
				break;
		}
	}
	return ret;
}

void event_quit(void)
{
	quit = true;
}

static int timer_get_count(int fd)
{
	uint64_t val;

	val = 0;
	read(fd, &val, sizeof(val));
	if (val > INT_MAX)
		val = INT_MAX;
	return val;
}

static int timer_cb(int fd, unsigned events, void *data)
{
	struct timer_data *td = data;
	int count;

	if (!(events & EV_READ))
		return 0;
	count = timer_get_count(fd);
	if (!td->armed)
		return 0;
	if (td->paused) {
		td->resume_backlog += count;
		return 0;
	}
	return td->cb(fd, count, td->cb_data);
}

static void timer_destructor(void *data)
{
	struct timer_data *td = data;

	if (td->cb_destructor)
		td->cb_destructor(td->cb_data);
	free(td);
}

int timer_new(timer_callback_t cb, void *cb_data,
	      cb_data_destructor_t cb_destructor)
{
	struct timer_data *td;
	int fd;
	int ret;

	td = szalloc(sizeof(*td));
	td->cb = cb;
	td->cb_data = cb_data;
	td->cb_destructor = cb_destructor;
	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (fd < 0) {
		free(td);
		return -errno;
	}
	ret = event_add_fd(fd, EV_READ, timer_cb, td, timer_destructor);
	if (ret < 0) {
		free(td);
		close(fd);
		return ret;
	}
	return fd;
}

int timer_arm(int fd, int milisecs, bool repeat)
{
	struct timer_data *td = find_cb_data(fd);
	struct itimerspec its;

	if (td) {
		td->armed = !!milisecs;
		td->paused = false;
	}

	memset(&its, 0, sizeof(its));
	time_add(&its.it_value, milisecs);
	if (repeat)
		time_add(&its.it_interval, milisecs);
	if (timerfd_settime(fd, 0, &its, NULL) < 0)
		return -errno;
	return 0;
}

int timer_disarm(int fd)
{
	return timer_arm(fd, 0, false);
}

int timer_pause(int fd)
{
	struct timer_data *td = find_cb_data(fd);
	struct itimerspec its;

	if (!fd)
		return -EINVAL;
	if (td->paused)
		return 0;
	memset(&its, 0, sizeof(its));
	if (timerfd_settime(fd, 0, &its, &td->resume_time) < 0)
		return -errno;
	td->resume_backlog = 0;
	td->paused = true;
	return 0;
}

int timer_resume(int fd)
{
	struct timer_data *td = find_cb_data(fd);

	if (!fd)
		return -EINVAL;
	if (!td->paused)
		return 0;
	if (timerfd_settime(fd, 0, &td->resume_time, NULL) < 0)
		return -errno;
	td->paused = false;
	if (td->resume_backlog)
		return td->cb(fd, td->resume_backlog, td->cb_data);
	return 0;
}

int timer_del(int fd)
{
	int res;

	timer_disarm(fd);
	res = event_del_fd(fd);
	close(fd);
	return res;
}
