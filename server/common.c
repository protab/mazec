#include "common.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

void emerg_exit(const char *path, int line, int code)
{
	log_err("fatal error %d at %s:%d", -code, path, line);
	exit(1);
}

static bool is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void rstrip(char *s)
{
	size_t pos;

	for (pos = strlen(s); pos > 0 && is_space(s[pos - 1]); pos--)
		;
	s[pos] = '\0';
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t i = 0;

	if (size == 0)
		return strlen(src);
	while (i < size - 1 && *src) {
		*dest++ = *src++;
		i++;
	}
	*dest = '\0';
	while (*src) {
		src++;
		i++;
	}
	return i;
}
