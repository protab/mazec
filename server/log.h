#ifndef LOG_H
#define LOG_H

int log_init(const char *name);
int log(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
