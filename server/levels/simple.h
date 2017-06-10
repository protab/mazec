#ifndef SIMPLE_H
#define SIMPLE_H
#include "../level.h"

/* Easy implementation of simple levels. All objects must be in the basic
 * grid (i.e. coordinates are multiple of 15). The grid must be statically
 * generated. It may be of any size. Supports multiple connections
 * (players), the screen is positioned automatically to contain as much
 * players as possible. Each player starts at the same position. */

/* Private data must start with this struct embdedded: */
struct simple_data {
	int x;
	int y;
	int angle;
	struct simple_data *next;
};

/* Initialize the level. This must be called from the level's 'init'
 * callback.
 * The 'level' array contains the level data, 'width' and 'height' is the
 * size of the 'level' array. 'start_x' and 'start_y' is the initial
 * position of the player(s) in the array. 'priv_size' is the size of the
 * private data to allocate for each player, this must include struct
 * simple_data size. */
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

/* Try to move. Returns true if the player was moved; there's nothing to do
 * in such case. Returns false if the player was not moved. In that case,
 * 'err' contains either an error message or NULL. If it contains an error
 * message, move was not possible because of a wall or out of borders. It
 * can be also a winning message if the move reached a treasure ('win' is
 * set accordingly). If the move was in bounds but would move to something
 * that is not a wall or empty space, 'err' contains NULL and the new
 * coordinates are stored in 'new_x' and 'new_y' (without the player being
 * moved). */
bool simple_try_move(void *data, char c, bool *win, char **err,
		     int *new_x, int *new_y);

/* The simplest levels may use this as the 'move' callback. It handles only
 * walls and treasure. If you need anything more, you need to implement your
 * own 'move' handler (see the simple_try_move helper). */
char *simple_move(void *data, char c, bool *win);

/* Macros for easy definition of levels. */

#define SIMPLE_INIT(name, width, height, level_data, start_x, start_y)	\
	static void name(void)						\
	{								\
		simple_init(width, height, level_data,			\
			    start_x, start_y,				\
			    sizeof(struct simple_data));		\
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
