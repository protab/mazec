#ifndef LEVEL_H
#define LEVEL_H
#include <stdbool.h>

struct level_ops {
	int max_conn;
	void *(*get_data)();
	void (*free_data)(void *data);

	char *(*move)(char c, bool *win);
	char *(*what)(int x, int y, int *res);
	char *(*maze)(int **res, int *len);
	char *(*get_x)(int *res);
	char *(*get_y)(int *res);
	char *(*get_w)(int *res);
	char *(*get_h)(int *res);
};

#endif
