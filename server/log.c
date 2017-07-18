#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include "common.h"
#include "time.h"

const char *level_desc[__LOG_LEVEL_MAX] = { "INFO", "WARN", "ERR!" };
const int level_syslog[__LOG_LEVEL_MAX] = { LOG_INFO, LOG_WARNING, LOG_ERR };

static pid_t pid;
static char *domain = NULL;
static bool to_syslog;

void log_init(const char *name, bool to_syslog_)
{
	pid = getpid();
	domain = sstrdup(name);
	to_syslog = to_syslog_;

	if (to_syslog_)
		openlog(name, LOG_PID, LOG_USER);
}

#define MAX_LOG	1024

int log_stderr(int level, const char *format, va_list ap)
{
	time_t t;
	char buf[MAX_LOG];
	int pos;

	t = time(NULL);
	pos = strftime(buf, sizeof(buf), "%F %T", localtime(&t));

	pos += snprintf(buf + pos, MAX_LOG - pos,
			" %s [%s:%d] ", level_desc[level], domain, pid);
	if (pos >= MAX_LOG)
		pos = MAX_LOG - 1;

	pos += vsnprintf(buf + pos, MAX_LOG - pos, format, ap);
	if (pos >= MAX_LOG)
		pos = MAX_LOG - 1;

	if (pos == MAX_LOG - 1)
		pos--;
	if (pos && buf[pos - 1] != '\n')
		buf[pos++] = '\n';
	write(2, buf, pos);
	/* Don't care about incomplete writes. */
	return 0;
}

int log_msg(int level, const char *format, ...)
{
	va_list ap;
	int res = 0;

	if (level < 0 || level >= __LOG_LEVEL_MAX)
		level = 0;

	va_start(ap, format);
	if (to_syslog)
		vsyslog(level_syslog[level], format, ap);
	else
		res = log_stderr(level, format, ap);
	va_end(ap);
	return res;
}

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
