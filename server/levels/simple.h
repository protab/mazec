#ifndef SIMPLE_H
#define SIMPLE_H

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

#endif
