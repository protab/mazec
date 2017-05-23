#include "app.h"
#include <stdbool.h>
#include "common.h"

/* test code only */

static char *x_move(char c __unused, bool *win)
{
	*win = false;
	return "Zdi vsude okolo.";
}

static char *x_what(int x __unused, int y __unused, int *res)
{
	*res = 0;
	return NULL;
}

static char *x_get(int *res)
{
	*res = 0;
	return NULL;
}

static const struct level_ops ops = {
	.max_conn = 2,
	.move = x_move,
	.what = x_what,
	.get_x = x_get,
	.get_y = x_get,
	.get_w = x_get,
	.get_h = x_get,
};

const struct level_ops *app_get_level(char *code __unused)
{
	return &ops;
}
