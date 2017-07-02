#ifndef SPAWN_H
#define SPAWN_H

void spawn_init(int argc, char **argv);

/* The caller is responsible for checking that the given login exists. */
int spawn(const char *login);

int exec_wait(char *out, int out_size, char *prg, ...);

#endif
