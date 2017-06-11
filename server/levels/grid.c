#include "simple.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "../common.h"
#include "../draw.h"
#include "../level.h"
#include "../proto_msg.h"

static int width, height, start_x, start_y;
static unsigned priv_size;
static const unsigned char *level_data;
grid_move_commit_t move_commit_cb;

struct grid_data *grid_players;

void grid_init(int width_, int height_, const unsigned char *level,
	       int start_x_, int start_y_, unsigned priv_size_)
{
	width = width_;
	height = height_;
	start_x = start_x_;
	start_y = start_y_;
	priv_size = priv_size_;
	if (!priv_size)
		priv_size = sizeof(struct grid_data);
	level_data = level;

	grid_players = NULL;
	move_commit_cb = NULL;
}

void grid_set_move_commit(grid_move_commit_t move_commit)
{
	move_commit_cb = move_commit;
}

void *grid_get_data(void)
{
	struct grid_data *d, **ptr;

	d = szalloc(priv_size);
	d->x = start_x;
	d->y = start_y;
	for (ptr = &grid_players; *ptr; ptr = &(*ptr)->next)
		;
	*ptr = d;
	return d;
}

void grid_free_data(void *data)
{
	struct grid_data *d = data, **ptr;

	for (ptr = &grid_players; *ptr != d; ptr = &(*ptr)->next)
		;
	*ptr = d->next;
	free(d);
}

bool grid_try_move(void *data, char c, bool *win, char **err,
		   int *new_x, int *new_y)
{
	struct grid_data *d = data;
	int nx, ny;
	unsigned char col;

	*win = false;
	*err = NULL;

	nx = d->x;
	ny = d->y;

	switch (c) {
	case 'w':
		ny--;
		break;
	case 's':
		ny++;
		break;
	case 'a':
		nx--;
		break;
	case 'd':
		nx++;
		break;
	default:
		*err = A_MSG_UNKNOWN_MOVE;
		return false;
	}
	if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
		*err = A_MSG_OUT_OF_MAZE;
		return false;
	}

	col = level_data[ny * width + nx];
	if (col == COLOR_NONE) {
		d->x = nx;
		d->y = ny;
		if (move_commit_cb)
			move_commit_cb(data);
		return true;
	} else if (col == COLOR_WALL) {
		*err = A_MSG_WALL_HIT;
		return false;
	} else if (col == COLOR_TREASURE) {
		*win = true;
		*err = A_MSG_WIN;
		return false;
	} else {
		*new_x = nx;
		*new_y = ny;
		return false;
	}
}

bool grid_try_o_move(void *data, char c, bool *win, char **err,
		     int *new_x, int *new_y)
{
	struct simple_data *d = data;
	unsigned char col;
	int nx, ny;

	*win = false;
	*err = NULL;

	nx = d->x;
	ny = d->y;

	switch (c) {
	case 'w':
		break;
	case 'a':
		d->angle = (d->angle + 90) % 360;
		if (move_commit_cb)
			move_commit_cb(data);
		return true;
	case 'd':
		d->angle = (d->angle + 270) % 360;
		if (move_commit_cb)
			move_commit_cb(data);
		return true;
	default:
		*err = A_MSG_UNKNOWN_MOVE;
		return false;
	}

	switch (d->angle) {
	case 0:
		ny--;
		break;
	case 90:
		nx--;
		break;
	case 180:
		ny++;
		break;
	case 270:
		nx++;
		break;
	}
	if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
		*err = A_MSG_OUT_OF_MAZE;
		return false;
	}

	col = level_data[ny * width + nx];
	if (col == COLOR_NONE) {
		d->x = nx;
		d->y = ny;
		if (move_commit_cb)
			move_commit_cb(data);
		return true;
	} else if (col == COLOR_WALL) {
		*err = A_MSG_WALL_HIT;
		return false;
	} else if (col == COLOR_TREASURE) {
		*win = true;
		*err = A_MSG_WIN;
		return false;
	} else {
		*new_x = nx;
		*new_y = ny;
		return false;
	}
}

char *grid_move(void *data, char c, bool *win)
{
	char *err;
	int new_x, new_y;

	if (grid_try_move(data, c, win, &err, &new_x, &new_y))
		return NULL;
	if (!err)
		err = A_MSG_WALL_HIT;
	return err;
}

char *grid_o_move(void *data, char c, bool *win)
{
	char *err;
	int new_x, new_y;

	if (grid_try_o_move(data, c, win, &err, &new_x, &new_y))
		return NULL;
	if (!err)
		err = A_MSG_WALL_HIT;
	return err;
}
