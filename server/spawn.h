#ifndef SPAWN_H
#define SPAWN_H
#include <stdbool.h>

void spawn_init(char *argv0, bool use_syslog);

/* The caller is responsible for checking that the given login exists. */
int spawn(const char *login);

int exec_wait(char *out, int out_size, char *prg, ...);

#endif
