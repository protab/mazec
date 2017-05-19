#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

const char *level_desc[__LOG_LEVEL_MAX] = { "INFO", "WARN", "ERR!" };

static pid_t pid;
static char *domain;

int log_init(const char *name)
{
	pid = getpid();
	domain = strdup(name);
	if (!domain)
		return -errno;
}

#define MAX_LOG	1024

int log(int level, const char *format, ...)
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
			" %s [%s:%ld]", buf, level_desc[level], domain, pid);
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
	buf[pos++] = '\0';
	write(2, buf, pos);
	/* Don't care about incomplete writes. */
}
