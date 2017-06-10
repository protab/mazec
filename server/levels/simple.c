#include "simple.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "../common.h"
#include "../draw.h"
#include "../level.h"
#include "../proto_msg.h"

static int width, height, start_x, start_y;
static int origin_x, origin_y;
static unsigned priv_size;
static const unsigned char *level_data;
static unsigned char *level_copy;
static struct simple_data *first;

#define BORDER	5
static void update_viewport(void);

void simple_init(int width_, int height_, const unsigned char *level,
		 int start_x_, int start_y_, unsigned priv_size_)
{
	width = width_;
	height = height_;
	start_x = start_x_;
	start_y = start_y_;
	priv_size = priv_size_;
	level_data = level;

	level_copy = salloc(width * height);
	first = NULL;

	origin_x = start_x - (DRAW_MOD_WIDTH + 1) / 2;
	origin_y = start_y - (DRAW_MOD_HEIGHT + 1) / 2;
}

void *simple_get_data(void)
{
	struct simple_data *d, **ptr;

	d = szalloc(priv_size);
	d->x = start_x;
	d->y = start_y;
	for (ptr = &first; *ptr; ptr = &(*ptr)->next)
		;
	*ptr = d;
	update_viewport();
	level_dirty();
	return d;
}

void simple_free_data(void *data)
{
	struct simple_data *d = data, **ptr;

	for (ptr = &first; *ptr != d; ptr = &(*ptr)->next)
		;
	*ptr = d->next;
	free(d);
}

char *simple_what(void *data __unused, int x, int y, int *res)
{
	struct simple_data *d;

	if (x < 0 || x >= width || y < 0 || y >= height)
		return A_MSG_OUT_OF_MAZE;

	for (d = first; d; d = d->next) {
		if (d->x == x && d->y == y) {
			*res = COLOR_PLAYER;
			return NULL;
		}
	}

	*res = level_data[y * width + x];
	return NULL;
}

char *simple_maze(void *data __unused, unsigned char **res, unsigned *len)
{
	struct simple_data *d;

	memcpy(level_copy, level_data, width * height);
	for (d = first; d; d = d->next)
		level_copy[d->y * width + d->x] = COLOR_PLAYER;

	*res = level_copy;
	*len = width * height;
	return NULL;
}

char *simple_get_x(void *data, int *res)
{
	struct simple_data *d = data;

	*res = d->x;
	return NULL;
}

char *simple_get_y(void *data, int *res)
{
	struct simple_data *d = data;

	*res = d->y;
	return NULL;
}

char *simple_get_w(void *data __unused, int *res)
{
	*res = width;
	return NULL;
}

char *simple_get_h(void *data __unused, int *res)
{
	*res = height;
	return NULL;
}

void simple_set_xy(void *data, int x, int y, int angle)
{
	struct simple_data *d = data;

	d->x = x;
	d->y = y;
	d->angle = angle;
	update_viewport();
	level_dirty();
}

bool simple_try_move(void *data, char c, bool *win, char **err,
		     int *new_x, int *new_y)
{
	struct simple_data *d = data;
	int sx = 0, sy = 0;
	unsigned char col;

	*win = false;

	switch (c) {
	case 'w':
		sy = -1;
		break;
	case 's':
		sy = 1;
		break;
	case 'a':
		sx = -1;
		break;
	case 'd':
		sx = 1;
		break;
	default:
		*err = A_MSG_UNKNOWN_MOVE;
		return false;
	}
	col = level_data[((d->y + sy) * width) + (d->x + sx)];
	if (col == COLOR_NONE) {
		simple_set_xy(data, d->x + sx, d->y + sy, 0);
		*err = NULL;
		return true;
	} else if (col == COLOR_WALL) {
		*err = A_MSG_WALL_HIT;
		return false;
	} else if (col == COLOR_TREASURE) {
		*win = true;
		*err = A_MSG_WIN;
		return false;
	} else {
		*new_x = d->x + sx;
		*new_y = d->y + sy;
		*err = NULL;
		return false;
	}
}

char *simple_move(void *data, char c, bool *win)
{
	char *err;
	int new_x, new_y;

	if (simple_try_move(data, c, win, &err, &new_x, &new_y))
		return NULL;
	if (!err)
		err = A_MSG_WALL_HIT;
	return err;
}

static int find_max(int *data, int len)
{
	int max = INT_MIN, res = 0;

	for (int i = 0; i < len; i++) {
		if (data[i] > max) {
			max = data[i];
			res = i;
		}
	}
	return res;
}

static void update_viewport(void)
{
	int shifts[4]; /* up down left right */
	int dir;
	struct simple_data *d;

	/* see what happens to number of visible players when we move in
	 * each direction */
	memset(shifts, 0, sizeof(shifts));
	for (d = first; d; d = d->next) {
		if (d->y == origin_y + DRAW_MOD_HEIGHT - 1)
			shifts[0]--;
		else if (d->y == origin_y - 1)
			shifts[0]++;
		else if (d->y == origin_y)
			shifts[1]--;
		else if (d->y == origin_y + DRAW_MOD_HEIGHT)
			shifts[1]++;
		if (d->x == origin_x + DRAW_MOD_WIDTH - 1)
			shifts[2]--;
		else if (d->x == origin_x - 1)
			shifts[2]++;
		else if (d->x == origin_x)
			shifts[3]--;
		else if (d->x == origin_x + DRAW_MOD_WIDTH)
			shifts[3]++;
	}

	dir = find_max(shifts, 4);

	if (shifts[dir] <= 0) {
		/* the number of visible players stay the same; look whether
		 * we can improve the viewport location by keeping a border */
		memset(shifts, 0, sizeof(shifts));
		for (d = first; d; d = d->next) {
			if (d->x < origin_x || d->x >= origin_x + DRAW_MOD_WIDTH ||
			    d->y < origin_y || d->y >= origin_y + DRAW_MOD_HEIGHT)
				continue;
			if (d->y < origin_y + BORDER)
				shifts[0]++;
			if (d->y <= origin_y + BORDER)
				shifts[1]--;
			if (d->y > origin_y + DRAW_MOD_HEIGHT - 1 - BORDER)
				shifts[1]++;
			if (d->y >= origin_y + DRAW_MOD_HEIGHT - 1 - BORDER)
				shifts[0]--;
			if (d->x < origin_x + BORDER)
				shifts[2]++;
			if (d->x <= origin_x + BORDER)
				shifts[3]--;
			if (d->x > origin_x + DRAW_MOD_WIDTH - 1 - BORDER)
				shifts[3]++;
			if (d->x >= origin_x + DRAW_MOD_WIDTH - 1 - BORDER)
				shifts[2]--;
		}

		dir = find_max(shifts, 4);
		if (shifts[dir] <= 0)
			dir = -1;
	}

	switch (dir) {
	case 0:
		if (origin_y > 0)
			origin_y--;
		break;
	case 1:
		if (origin_y < height - DRAW_MOD_HEIGHT)
			origin_y++;
		break;
	case 2:
		if (origin_x > 0)
			origin_x--;
		break;
	case 3:
		if (origin_x < width - DRAW_MOD_WIDTH)
			origin_x++;
		break;
	}
}

void simple_redraw(void)
{
	int x_min, y_min, x_max, y_max;
	int x, y;
	struct simple_data *d;

	draw_clear();
	draw_set_origin(origin_x * DRAW_MOD, origin_y * DRAW_MOD);

	x_min = origin_x;
	if (x_min < 0)
		x_min = 0;
	y_min = origin_y;
	if (y_min < 0)
		y_min = 0;
	x_max = origin_x + DRAW_MOD_WIDTH;
	if (x_max > width)
		x_max = width;
	y_max = origin_y + DRAW_MOD_HEIGHT;
	if (y_max > height)
		y_max = height;

	for (y = y_min; y < y_max; y++)
		for (x = x_min; x < x_max; x++)
			draw_item(x * DRAW_MOD, y * DRAW_MOD, 0,
				  level_data[y * width + x]);
	for (d = first; d; d = d->next)
		draw_item(d->x * DRAW_MOD, d->y * DRAW_MOD, d->angle, COLOR_PLAYER);
}
