#ifndef LEVEL_H
#define LEVEL_H
#include <stdbool.h>

/* The definition of the level. Each level must define a non-static symbol
 * named 'level_ops' of the 'struct level_ops' type. The fields are set to
 * appropriate level data and level callbacks. */

struct level_ops {
	/* Maximum number of concurrent connections to this level. */
	int max_conn;

	/* Maximum number of seconds that are available to solve the level.
	 * After this time, all connections are terminated. */
	int max_time;

	/* Callback called right after the level is loaded. */
	void (*init)(void);

	/* Called once for each new connection. Returns newly allocated
	 * private data that are passed to all other callbacks. */
	void *(*get_data)(void);

	/* Frees private data allocated by the 'get_data' callback. */
	void (*free_data)(void *data);

	/* The callbacks below return NULL for success or error message for
	 * failure. Failure does not terminate the level. */

	/* Called for the MOVE command. The passed character is in 'c'.
	 * The return semantics is valid only when '*win' is false. If
	 * '*win' is set to true, the returned message is the winning
	 * message and the level is terminated. */
	char *(*move)(void *data, char c, bool *win);

	/* Returns the color at position 'x', 'y'. The meaning of 'x' and
	 * 'y' is up to the level. The returned color is stored into
	 * '*res'. */
	char *(*what)(void *data, int x, int y, int *res);

	/* Returns the whole level. This function can be NULL. The returned
	 * level data are in '*res', the length of the returned data is in
	 * '*len'. No lifecycle management of the '*res' data is provided,
	 * the function should return its own statically allocated buffer. */
	char *(*maze)(void *data, unsigned char **res, unsigned *len);

	/* Stores the actual x position to '*res'. */
	char *(*get_x)(void *data, int *res);

	/* Stores the actual y position to '*res'. */
	char *(*get_y)(void *data, int *res);

	/* Stores the width to '*res'. The width must be constant across
	 * calls to the function. */
	char *(*get_w)(void *data, int *res);

	/* Stores the height to '*res'. The height must be constant across
	 * calls to the function. */
	char *(*get_h)(void *data, int *res);

	/* Force redraw of the remote screen. Called whenever a new remote
	 * display is connected. */
	void (*redraw)(void);
};

/* Colors (objects). */
enum {
	COLOR_NONE,
	COLOR_PLAYER,
	COLOR_WALL,
};

#endif
