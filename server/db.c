#include "db.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "event.h"
#include "log.h"

#define LOGIN_LEN	30

struct user {
	char login[LOGIN_LEN + 1];
	pid_t pid;
	int pipefd;
	struct user *next;
};

static struct user *users = NULL;
static struct user *inactive = NULL;

static struct user **get_last_ptr(struct user **list, int *count)
{
	struct user **ptr;

	for (ptr = list; *ptr; ptr = &(*ptr)->next)
		if (count)
			(*count)++;
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
		s[len - 2] = '\0';
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
				u = malloc(sizeof(*u));
				if (!u) {
					fclose(f);
					return -ENOMEM;
				}
				strcpy(u->login, login);
				u->pid = 0;
				u->pipe_fd = -1;
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

void db_start_process(const char *login, pid_t pid, int pipefd)
{
	struct user *u;

	u = find_login(users, pid);
	if (!u) {
		log_err("unknown login '%s' reported as started", login);
		return;
	}
	u->pid = pid;
	u->pipefd = pipefd;
	event_add_fd(pipefd, false, log_remote, NULL, NULL);
	log_info("child %s:%d started", login, pid);
}

void db_end_process(pid_t pid)
{
	struct user *u;

	u = find_pid(users, pid);
	if (!u)
		u = find_pid(inactive, pid);
	if (!u) {
		log_err("unknown pid %ld reported as killed", pid);
		return;
	}
	event_del_fd(u->pipefd);
	u->pid = 0;
	u->pipefd = -1;
	log_info("child %s:%d terminated", u->login, pid);
}

/* Returns fd or -ENOENT if user process is not running or -EINVAL if unkown
 * login. */
int db_get_pipe(const char *login)
{
	struct user *u;

	u = find_login(users, pid);
	if (!u)
		return -EINVAL;
	if (u->pipefd < 0)
		return -ENOENT;
	return u->pipefd;
}
