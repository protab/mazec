#ifndef COMMON_H
#define COMMON_H
#include <stdlib.h>

#define __unused	__attribute__((unused))

#define check(v)	do { int _ret = (v); if (_ret < 0) emerg_exit(__FILE__, __LINE__, _ret);  } while (0)

void emerg_exit(const char *path, int line, int code);

/* Safe allocations. They never fail. */
void *salloc(size_t size);
void *szalloc(size_t size);
void *srealloc(void *ptr, size_t size);
void sfree(void *ptr);

void rstrip(char *s);
size_t strlcpy(char *dest, const char *src, size_t size);
char *sstrdup(const char *s);

#endif
