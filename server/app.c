#include "app.h"
#include "common.h"

/* test code only */

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

static const struct app_ops ops = {
	.max_conn = 2,
	.what = x_what,
	.get_x = x_get,
	.get_y = x_get,
	.get_w = x_get,
	.get_h = x_get,
};

const struct app_ops *app_get_level(char *code __unused)
{
	return &ops;
}
