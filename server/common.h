#ifndef COMMON_H
#define COMMON_H

#define __unused	__attribute__((unused))

#define check(v)	do { int _ret = (v); if (_ret < 0) emerg_exit(__FILE__, __LINE__, _ret);  } while (0)

void emerg_exit(const char *path, int line, int code);

#endif
