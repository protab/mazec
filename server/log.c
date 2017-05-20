#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "common.h"

const char *level_desc[__LOG_LEVEL_MAX] = { "INFO", "WARN", "ERR!" };

static pid_t pid;
static char *domain = NULL;

int log_init(const char *name)
{
	pid = getpid();
	domain = strdup(name);
	if (!domain)
		return -errno;
	return 0;
}

#define MAX_LOG	1024

int log_msg(int level, const char *format, ...)
{
	va_list ap;
	time_t t;
	char buf[MAX_LOG];
	int pos;

	if (level < 0 || level >= __LOG_LEVEL_MAX)
		level = 0;
	t = time(NULL);
	pos = strftime(buf, sizeof(buf), "%F %T", localtime(&t));

	pos += snprintf(buf + pos, MAX_LOG - pos,
			" %s [%s:%d] ", level_desc[level], domain, pid);
	if (pos >= MAX_LOG)
		pos = MAX_LOG - 1;

	va_start(ap, format);
	pos += vsnprintf(buf + pos, MAX_LOG - pos, format, ap);
	if (pos >= MAX_LOG)
		pos = MAX_LOG - 1;
	va_end(ap);

	if (pos == MAX_LOG - 1)
		pos--;
	buf[pos++] = '\n';
	write(2, buf, pos);
	/* Don't care about incomplete writes. */
	return 0;
}

/* Needs to be terminated by calling with size == 0. */
void log_raw(char *buf, size_t size)
{
	static bool need_nl = false;

	if (size) {
		write(2, buf, size);
		need_nl = buf[size - 1] != '\n';
	} else if (need_nl) {
		char x = '\n';

		write(2, &x, 1);
		need_nl = false;
	}
}
