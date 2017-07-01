#include "simple.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "../common.h"
#include "../draw.h"
#include "../level.h"
#include "../proto_msg.h"

static int width, height;
static int origin_x, origin_y;
static const unsigned char *level_data;
static unsigned char *level_copy;

#define BORDER	5
static void update_viewport(void);

void simple_init(int width_, int height_, const unsigned char *level,
		 int start_x, int start_y, unsigned priv_size)
{
	grid_init(width_, height_, level, start_x, start_y, priv_size);
	grid_set_move_commit(simple_move_commit);

	width = width_;
	height = height_;
	level_data = level;

	level_copy = salloc(width * height);

	origin_x = start_x - (DRAW_MOD_WIDTH + 1) / 2;
	origin_y = start_y - (DRAW_MOD_HEIGHT + 1) / 2;
}

void *simple_get_data(void)
{
	struct simple_data *d;

	d = grid_get_data();
	update_viewport();
	level_dirty();
	return d;
}

void simple_free_data(void *data)
{
	grid_free_data(data);
}

char *simple_what(void *data __unused, int x, int y, int *res)
{
	struct simple_data *d;

	if (x < 0 || x >= width || y < 0 || y >= height)
		return A_MSG_OUT_OF_MAZE;

	for_each_player(d) {
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
	for_each_player(d) {
		level_copy[d->y * width + d->x] = COLOR_PLAYER;
	}

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

void simple_move_commit(void *data __unused)
{
	update_viewport();
	level_dirty();
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
	for_each_player(d) {
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
		for_each_player(d) {
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

void simple_ensure_visible(int x, int y)
{
	int new_x = origin_x, new_y = origin_y;

	if (x < origin_x)
		new_x = x;
	else if (x >= origin_x + DRAW_MOD_WIDTH)
		new_x = x - DRAW_MOD_WIDTH + 1;
	if (y < origin_y)
		new_y = y;
	else if (y >= origin_y + DRAW_MOD_HEIGHT)
		new_y = y - DRAW_MOD_HEIGHT + 1;
	if (new_x != origin_x || new_y != origin_y) {
		origin_x = new_x;
		origin_y = new_y;
		update_viewport();
		level_dirty();
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
	for_each_player(d) {
		draw_item(d->x * DRAW_MOD, d->y * DRAW_MOD, d->angle, COLOR_PLAYER);
	}
}
