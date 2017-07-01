#ifndef LEVEL_H
#define LEVEL_H
#include <stdbool.h>
#include <stdlib.h>

/* The access code for the level. Each level must call this macro. */
#define LEVEL_CODE(code)

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
	 * Returns one of the MOVE_ constants (see below). In cases other
	 * than MOVE_OKAY, 'msg' must be set to a message that will be sent
	 * to the client. */
	int (*move)(void *data, char c, char **msg);

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

	/* Redraw the remote screen. May be called even when the level did
	 * not indicate the screen is dirty, for example when a new client
	 * connects. May not be called at all, for example when there's no
	 * client connected. */
	void (*redraw)(void);
};

/* 'move' callback return values. */
enum {
	MOVE_OKAY,	/* Moved successfully. */
	MOVE_BAD,	/* Cannot move that way. */
	MOVE_WIN,	/* End of the level, winning. */
	MOVE_LOSE,	/* End of the level, losing. */
};

/* Colors (objects). */
enum {
	COLOR_NONE,
	COLOR_PLAYER,
	COLOR_WALL,
	COLOR_TREASURE,
	COLOR_KEY,
	COLOR_DOOR,
	COLOR_CROWN,
	COLOR_GOLD,
	COLOR_MONEY,
	COLOR_CHALICE,
};

/* Mark the remote screen as dirty. Call whenever there's a change to the
 * display. If the level is large and there are off screen changes, it is
 * advisable not to call this function when the change is not visible. */
void level_dirty(void);

#endif
