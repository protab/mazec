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

int grid_try_move(void *data, char c, char **msg, int *new_x, int *new_y)
{
	struct grid_data *d = data;
	int nx, ny;
	unsigned char col;

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
		*msg = A_MSG_UNKNOWN_MOVE;
		return MOVE_BAD;
	}
	if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
		*msg = A_MSG_OUT_OF_MAZE;
		return MOVE_BAD;
	}

	col = level_data[ny * width + nx];
	if (col == COLOR_NONE) {
		d->x = nx;
		d->y = ny;
		if (move_commit_cb)
			move_commit_cb(data);
		return MOVE_OKAY;
	} else if (col == COLOR_WALL) {
		*msg = A_MSG_WALL_HIT;
		return MOVE_BAD;
	} else if (col == COLOR_TREASURE) {
		*msg = A_MSG_WIN;
		return MOVE_WIN;
	} else {
		*new_x = nx;
		*new_y = ny;
		return -1;
	}
}

int grid_try_o_move(void *data, char c, char **msg, int *new_x, int *new_y)
{
	struct simple_data *d = data;
	unsigned char col;
	int nx, ny;

	nx = d->x;
	ny = d->y;

	switch (c) {
	case 'w':
		break;
	case 'a':
		d->angle = (d->angle + 90) % 360;
		if (move_commit_cb)
			move_commit_cb(data);
		return MOVE_OKAY;
	case 'd':
		d->angle = (d->angle + 270) % 360;
		if (move_commit_cb)
			move_commit_cb(data);
		return MOVE_OKAY;
	default:
		*msg = A_MSG_UNKNOWN_MOVE;
		return MOVE_BAD;
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
		*msg = A_MSG_OUT_OF_MAZE;
		return MOVE_BAD;
	}

	col = level_data[ny * width + nx];
	if (col == COLOR_NONE) {
		d->x = nx;
		d->y = ny;
		if (move_commit_cb)
			move_commit_cb(data);
		return MOVE_OKAY;
	} else if (col == COLOR_WALL) {
		*msg = A_MSG_WALL_HIT;
		return MOVE_BAD;
	} else if (col == COLOR_TREASURE) {
		*msg= A_MSG_WIN;
		return MOVE_WIN;
	} else {
		*new_x = nx;
		*new_y = ny;
		return -1;
	}
}

int grid_move(void *data, char c, char **msg)
{
	int new_x, new_y;
	int res;

	res = grid_try_move(data, c, msg, &new_x, &new_y);
	if (res < 0) {
		*msg = A_MSG_WALL_HIT;
		res = MOVE_BAD;
	}
	return res;
}

int grid_o_move(void *data, char c, char **msg)
{
	int new_x, new_y;
	int res;

	res = grid_try_o_move(data, c, msg, &new_x, &new_y);
	if (res < 0) {
		*msg = A_MSG_WALL_HIT;
		res = MOVE_BAD;
	}
	return res;
}
