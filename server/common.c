#include "common.h"
#include <errno.h>
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

static void alloc_err(size_t size)
{
	log_err("allocation error, size %zu, errno %d", size, errno);
	exit(1);
}

void *salloc(size_t size)
{
	void *res;

	res = malloc(size);
	if (!res)
		alloc_err(size);
	return res;
}

void *szalloc(size_t size)
{
	void *res;

	res = calloc(1, size);
	if (!res)
		alloc_err(size);
	return res;
}

void *srealloc(void *ptr, size_t size)
{
	void *res;

	res = realloc(ptr, size);
	if (!res)
		alloc_err(size);
	return res;
}

void sfree(void *ptr)
{
	free(ptr);
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

char *sstrdup(const char *s)
{
	char * res;

	res = strdup(s);
	if (!res)
		alloc_err(strlen(s));
	return res;
}
