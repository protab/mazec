#include "log.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static pid_t pid;
static char *domain;

int log_init(const char *name)
{
	pid = getpid();
	domain = strdup(name);
	if (!domain)
		return -errno;
}

int log(const char *format, ...)
{
	va_list ap;
	time_t t;
	char buf[32];

	t = time(NULL);
	if (strftime(buf, sizeof(buf), "%F %T", localtime(&t)) == 0)
		buf[0] = 0;
	fprintf(stderr, "%s [%s:%ld]", buf, domain, pid);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
