#ifndef LOG_H
#define LOG_H

enum {
	INFO,
	WARNING,
	ERROR,
	__LOG_LEVEL_MAX
};

int log_init(const char *name);
int log(int level, const char *format, ...) __attribute__((format(printf, 2, 3)));
int log_remote(int fd, void *unused);

#define log_info(...)	log(INFO, __VA_ARGS__)
#define log_warn(...)	log(WARNING, __VA_ARGS__)
#define log_err(...)	log(ERROR, __VA_ARGS__)

#endif
