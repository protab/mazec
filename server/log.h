#ifndef LOG_H
#define LOG_H
#include <stdbool.h>
#include <stdlib.h>

enum {
	INFO,
	WARNING,
	ERROR,
	__LOG_LEVEL_MAX
};

int log_init(const char *name);
int log_msg(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
void log_raw(char *buf, size_t size);

#define log_info(...)	log_msg(INFO, __VA_ARGS__)
#define log_warn(...)	log_msg(WARNING, __VA_ARGS__)
#define log_err(...)	log_msg(ERROR, __VA_ARGS__)

#endif
