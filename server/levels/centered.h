#ifndef CENTERED_H
#define CENTERED_H
#include "grid.h"
#include "../level.h"

/* Easy implementation of levels where the player stays in the center of the
 * screen and seemingly the surroundings change. Only a single player
 * supported. */

/* Private data must start with struct centered_data embedded. Currently,
 * this is equivalent to struct grid_data but don't rely on that. */
#define centered_data grid_data

/* Initialize the level. This must be called from the level's 'init'
 * callback.
 * See grid_init for description of the parameters. */
void centered_init(int width, int height, const unsigned char *level,
		   int start_x, int start_y, unsigned priv_size);

/* These must be set as the level's 'get_data' and 'free_data' callbacks or
 * called from within those callbacks. */
#define centered_get_data	grid_get_data
#define centered_free_data	grid_free_data

/* These should be set as the level's 'what', 'maze', 'get_x', 'get_y',
 * 'get_w' and 'get_h' callbacks, respectively. */
char *centered_what(void *data, int x, int y, int *res);
char *centered_maze(void *data, unsigned char **res, unsigned *len);
char *centered_get_x(void *data, int *res);
char *centered_get_y(void *data, int *res);
char *centered_get_w(void *data, int *res);
char *centered_get_h(void *data, int *res);

/* This must be set as the level's 'redraw' callback. */
void centered_redraw(void);

/* Has to be called after player is moved, i.e. after any of 'x', 'y' or
 * 'angle' field in 'data' is modified. */
void centered_move_commit(void *data);

/* Move helpers and simple callbacks. See grid.h for details. */
#define centered_try_move	grid_try_move
#define centered_try_o_move	grid_try_o_move
#define centered_move		grid_move
#define centered_o_move		grid_o_move

/* Macros for easy definition of levels. */

#define CENTERED_INIT(name, width, height, level_data, start_x, start_y)\
	static void name(void)						\
	{								\
		centered_init(width, height, level_data,		\
			    start_x, start_y, 0);			\
	}

#define CENTERED_DEFINE(max_conn_, max_time_, init_, move_)		\
	const struct level_ops level_ops = {				\
		.max_conn = max_conn_,					\
		.max_time = max_time_,					\
		.init = init_,						\
		.get_data = centered_get_data,				\
		.free_data = centered_free_data,			\
		.move = move_,						\
		.what = centered_what,					\
		.maze = centered_maze,					\
		.get_x = centered_get_x,				\
		.get_y = centered_get_y,				\
		.get_w = centered_get_w,				\
		.get_h = centered_get_h,				\
		.redraw = centered_redraw,				\
	}

#endif
