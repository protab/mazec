#include "../common.h"
#include "../level.h"

#define CODE "test"

static char *move(char c __unused, bool *win)
{
	*win = false;
	return "Zdi vsude okolo.";
}

static char *what(int x __unused, int y __unused, int *res)
{
	*res = 0;
	return NULL;
}

static char *get(int *res)
{
	*res = 0;
	return NULL;
}

const struct level_ops level_ops = {
	.max_conn = 2,
	.move = move,
	.what = what,
	.get_x = get,
	.get_y = get,
	.get_w = get,
	.get_h = get,
};
