#include "centered.h"
#include "../common.h"
#include "../draw.h"
#include "../proto_msg.h"

static int width, height;
static const unsigned char *level_data;
static unsigned char window[DRAW_MOD_WIDTH * DRAW_MOD_HEIGHT];

void centered_init(int width_, int height_, const unsigned char *level,
		   int start_x, int start_y, unsigned priv_size)
{
	grid_init(width_, height_, level, start_x, start_y, priv_size);

	width = width_;
	height = height_;
	level_data = level;
}

static unsigned char get_color(struct centered_data *d, int x, int y, bool with_player)
{
	if (with_player && x == DRAW_MOD_WIDTH / 2 && y == DRAW_MOD_HEIGHT / 2)
		return COLOR_PLAYER;

	int src_x = d->x - DRAW_MOD_WIDTH / 2 + x;
	int src_y = d->y - DRAW_MOD_HEIGHT / 2 + y;
	unsigned char col = COLOR_NONE;

	if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height)
		col = level_data[src_y * width + src_x];
	return col;
}

char *centered_what(void *data, int x, int y, int *res)
{
	if (x < 0 || x >= DRAW_MOD_WIDTH || y < 0 || y >= DRAW_MOD_HEIGHT)
		return A_MSG_OUT_OF_MAZE;

	*res = get_color(data, x, y, true);
	return NULL;
}

char *centered_maze(void *data, unsigned char **res, unsigned *len)
{
	for (int y = 0; y < DRAW_MOD_HEIGHT; y++)
		for (int x = 0; x < DRAW_MOD_WIDTH; x++)
			window[y * DRAW_MOD_WIDTH + x] = get_color(data, x, y, true);

	*res = window;
	*len = DRAW_MOD_WIDTH * DRAW_MOD_HEIGHT;
	return NULL;
}

char *centered_get_x(void *data __unused, int *res)
{
	*res = DRAW_MOD_WIDTH / 2;
	return NULL;
}

char *centered_get_y(void *data __unused, int *res)
{
	*res = DRAW_MOD_HEIGHT / 2;
	return NULL;
}

char *centered_get_w(void *data __unused, int *res)
{
	*res = DRAW_MOD_WIDTH;
	return NULL;
}

char *centered_get_h(void *data __unused, int *res)
{
	*res = DRAW_MOD_HEIGHT;
	return NULL;
}

void centered_set_xy(void *data, int x, int y, int angle)
{
	struct centered_data *d = data;

	d->x = x;
	d->y = y;
	d->angle = angle;
	level_dirty();
}

bool centered_try_move(void *data, char c, bool *win, char **err,
		       int *new_x, int *new_y)
{
	struct centered_data *d = data;
	bool res;

	res = grid_try_move(data, c, win, err, new_x, new_y);
	if (res)
		centered_set_xy(data, d->x, d->y, d->angle);
	return res;
}

char *centered_move(void *data, char c, bool *win)
{
	char *err;
	int new_x, new_y;

	if (centered_try_move(data, c, win, &err, &new_x, &new_y))
		return NULL;
	if (!err)
		err = A_MSG_WALL_HIT;
	return err;
}

bool centered_try_o_move(void *data, char c, bool *win, char **err,
			 int *new_x, int *new_y)
{
	struct centered_data *d = data;
	bool res;

	res = grid_try_o_move(data, c, win, err, new_x, new_y);
	if (res)
		centered_set_xy(data, d->x, d->y, d->angle);
	return res;
}

char *centered_o_move(void *data, char c, bool *win)
{
	char *err;
	int new_x, new_y;

	if (centered_try_o_move(data, c, win, &err, &new_x, &new_y))
		return NULL;
	if (!err)
		err = A_MSG_WALL_HIT;
	return err;
}

void centered_redraw(void)
{
	struct centered_data *d = (void *)grid_players;

	draw_clear();
	for (int y = 0; y < DRAW_MOD_HEIGHT; y++)
		for (int x = 0; x < DRAW_MOD_WIDTH; x++)
			draw_item(x * DRAW_MOD, y * DRAW_MOD, 0,
				  get_color(d, x, y, false));
	draw_item((DRAW_MOD_WIDTH / 2) * DRAW_MOD,
		  (DRAW_MOD_HEIGHT / 2) * DRAW_MOD,
		  d->angle, COLOR_PLAYER);
}
