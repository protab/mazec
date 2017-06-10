#ifndef SIMPLE_H
#define SIMPLE_H
#include "grid.h"
#include "../level.h"

/* Easy implementation of simple levels. All objects must be in the basic
 * grid. Supports multiple players, the screen is positioned automatically
 * to contain as much players as possible. For other limitations, see
 * grid.h. */

/* Private data must start with struct simple_data embedded. Currently, this
 * is equivalent to struct grid_data but don't rely on that. */
#define simple_data grid_data

/* Initialize the level. This must be called from the level's 'init'
 * callback.
 * See grid_init for description of the parameters. */
void simple_init(int width, int height, const unsigned char *level,
		 int start_x, int start_y, unsigned priv_size);

/* These must be set as the level's 'get_data' and 'free_data' callbacks or
 * called from within those callbacks. */
void *simple_get_data(void);
void simple_free_data(void *data);

/* These should be set as the level's 'what', 'maze', 'get_x', 'get_y',
 * 'get_w' and 'get_h' callbacks, respectively. */
char *simple_what(void *data, int x, int y, int *res);
char *simple_maze(void *data, unsigned char **res, unsigned *len);
char *simple_get_x(void *data, int *res);
char *simple_get_y(void *data, int *res);
char *simple_get_w(void *data, int *res);
char *simple_get_h(void *data, int *res);

/* This must be set as the level's 'redraw' callback. */
void simple_redraw(void);

/* Move the player. This must be called instead of altering the 'x', 'y' and
 * 'angle' fields directly. */
void simple_set_xy(void *data, int x, int y, int angle);

/* Try to move. See grid_try_move for description of pamaters. */
bool simple_try_move(void *data, char c, bool *win, char **err,
		     int *new_x, int *new_y);

/* Try to move, rotating player on left/right keys. The parameters are
 * identical to the simple_try_move helper. */
bool simple_try_o_move(void *data, char c, bool *win, char **err,
		       int *new_x, int *new_y);

/* The simplest levels may use this as the 'move' callback. It handles only
 * walls and treasure. If you need anything more, you need to implement your
 * own 'move' handler (see the simple_try_move helper). */
char *simple_move(void *data, char c, bool *win);

/* Usable as the 'move' callback with the same limitations as simple_move.
 * The player changes its orientation on left/right keys. */
char *simple_o_move(void *data, char c, bool *win);

/* Macros for easy definition of levels. */

#define SIMPLE_INIT(name, width, height, level_data, start_x, start_y)	\
	static void name(void)						\
	{								\
		simple_init(width, height, level_data,			\
			    start_x, start_y, 0);			\
	}

#define SIMPLE_DEFINE(max_conn_, max_time_, init_, move_)		\
	const struct level_ops level_ops = {				\
		.max_conn = max_conn_,					\
		.max_time = max_time_,					\
		.init = init_,						\
		.get_data = simple_get_data,				\
		.free_data = simple_free_data,				\
		.move = move_,						\
		.what = simple_what,					\
		.maze = simple_maze,					\
		.get_x = simple_get_x,					\
		.get_y = simple_get_y,					\
		.get_w = simple_get_w,					\
		.get_h = simple_get_h,					\
		.redraw = simple_redraw,				\
	}

#endif
