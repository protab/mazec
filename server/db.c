#include "db.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "event.h"
#include "log.h"
#include "socket.h"
#include "spawn.h"

#define LOGIN_LEN	30

struct user {
	char login[LOGIN_LEN + 1];
	pid_t pid;
	struct socket *pipe;
	struct user *next;
};

static struct user *users = NULL;
static struct user *inactive = NULL;

static struct user **get_last_ptr(struct user **list, int *count)
{
	struct user **ptr;
	int c = 0;

	for (ptr = list; *ptr; ptr = &(*ptr)->next)
		c++;
	if (count)
		*count = c;
	return ptr;
}

static struct user **find_login_ptr(struct user **list, const char *login)
{
	struct user **ptr;

	for (ptr = list; *ptr; ptr = &(*ptr)->next)
		if (!strcmp((*ptr)->login, login))
			break;
	return ptr;
}

static struct user *find_login(struct user *list, const char *login)
{
	struct user *u;

	for (u = list; u; u = u->next)
		if (!strcmp(u->login, login))
			break;
	return u;
}

static struct user *find_pid(struct user *list, pid_t pid)
{
	struct user *u;

	for (u = list; u; u = u->next)
		if (u->pid == pid)
			break;
	return u;
}

static bool strip_nl(char *s)
{
	int len = strlen(s);

	if (len && s[len - 1] == '\n') {
		s[len - 1] = '\0';
		return true;
	}
	return false;
}

int db_reload(void)
{
	FILE *f;
	struct user **ptr, **last;
	int cnt_before, cnt_after, cnt_new = 0;
	char login[LOGIN_LEN + 1];

	log_info("reloading db from %s", DB_PATH);
	f = fopen(DB_PATH, "r");
	if (!f)
		return -errno;

	ptr = get_last_ptr(&inactive, &cnt_before);
	*ptr = users;
	users = NULL;
	last = &users;

	while (fgets(login, sizeof(login), f)) {
		bool consume;

		consume = !strip_nl(login);
		ptr = find_login_ptr(&inactive, login);
		if (*ptr) {
			struct user *u = *ptr;

			*ptr = u->next;
			u->next = NULL;
			*last = u;
			last = &u->next;
		} else {
			struct user *u;

			u = find_login(users, login);
			if (u) {
				log_warn("duplicate login %s (db %s)", login, DB_PATH);
			} else {
				u = salloc(sizeof(*u));
				strcpy(u->login, login);
				u->pid = 0;
				u->pipe = NULL;
				u->next = NULL;
				*last = u;
				last = &u->next;
				cnt_new++;
			}
		}

		while (consume) {
			if (!fgets(login, sizeof(login), f))
				break;
			consume = !strip_nl(login);
		}
	}
	fclose(f);

	get_last_ptr(&inactive, &cnt_after);
	log_info("new users: %d, inactive users before/after: %d/%d",
		 cnt_new, cnt_before, cnt_after);
	return 0;
}

#define BUF_SIZE 1024
static void pipe_read(struct socket *s, void *data __unused)
{
	char buf[BUF_SIZE];
	size_t len;

	while (true) {
		len = socket_read(s, buf, BUF_SIZE);
		log_raw(buf, len);
		if (!len)
			break;
	}
}

void db_start_process(const char *login, pid_t pid, int pipefd)
{
	struct user *u;

	u = find_login(users, login);
	if (!u) {
		log_err("unknown login '%s' reported as started", login);
		return;
	}
	u->pid = pid;
	u->pipe = socket_add(pipefd, pipe_read, NULL, NULL);
	socket_ref(u->pipe);
	log_info("child [%s:%d] started with pipe fd %d", login, pid, pipefd);
}

void db_end_process(pid_t pid)
{
	struct user *u;

	u = find_pid(users, pid);
	if (!u)
		u = find_pid(inactive, pid);
	if (!u) {
		log_err("unknown pid %d reported as killed", pid);
		return;
	}
	socket_del(u->pipe);
	socket_unref(u->pipe);
	u->pid = 0;
	u->pipe = NULL;
	log_info("child [%s:%d] terminated", u->login, pid);
}

bool db_user_exists(const char *login)
{
	return !!find_login(users, login);
}

struct socket *db_get_pipe(const char *login)
{
	struct user *u;

	u = find_login(users, login);
	if (!u)
		return NULL;
	if (!u->pipe)
		spawn(login);
	return u->pipe;
}
