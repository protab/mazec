#ifndef GRID_H
#define GRID_H
#include "../level.h"

/* This helps with implementation of levels where all objects are in the
 * basic grid (i.e. coordinates are multiple of 15). The grid may be of any
 * size. This module does not do any drawing, it only keeps track of
 * players. Multiple connections (players) are supported. Each player starts
 * at the same position (but this can be overriden in get_data). */

/* Private data must start with this struct embdedded: */
struct grid_data {
	int x;
	int y;
	int angle;
	struct grid_data *next;
};

extern struct grid_data *grid_players;

typedef void (*grid_move_commit_t)(void *data);

/* Initialize the level. This must be called from the level's 'init'
 * callback.
 * The 'level' array contains the level data, 'width' and 'height' is the
 * size of the 'level' array. 'start_x' and 'start_y' is the initial
 * position of the player(s) in the array. 'priv_size' is the size of the
 * private data to allocate for each player, this must be either zero or
 * include struct grid_data size. */
void grid_init(int width, int height, const unsigned char *level,
	       int start_x, int start_y, unsigned priv_size);

/* Sets the callback called after successful move. */
void grid_set_move_commit(grid_move_commit_t move_commit);

/* These must be called from the level's 'get_data' and 'free_data'
 * callbacks. */
void *grid_get_data(void);
void grid_free_data(void *data);

/* Try to move. Returns -1 or one of the MOVE_ constants (see ../level.h).
 * The meaning of the return values is:
 * MOVE_OKAY: the player was moved, there's nothing to do.
 * MOVE_BAD: the player would hit a wall or would move out of borders. The
 *           'msg' is set to an error message.
 * MOVE_WIN: the player reached a treasure. The 'msg' is set.
 * MOVE_LOST: this is never returned.
 * -1: the move was in bounds but would move to something that is not a wall
 *     or empty space. Further processing is required. The new coordinates
 *     are stored in 'new_x' and 'new_y' (without the player being moved).
 */
int grid_try_move(void *data, char c, char **msg, int *new_x, int *new_y);

/* Try to move, rotating player on left/right keys. The parameters are
 * identical to the grid_try_move helper. */
int grid_try_o_move(void *data, char c, char **msg, int *new_x, int *new_y);

/* The simplest levels may use this as the 'move' callback. It handles only
 * walls and treasure. If you need anything more, you need to implement your
 * own 'move' handler (see the grid_try_move helper). */
int grid_move(void *data, char c, char **msg);

/* Usable as the 'move' callback with the same limitations as grid_move. The
 * player changes its orientation on left/right keys. */
int grid_o_move(void *data, char c, char **msg);

/* Macro to iterate through all players. */
#define for_each_player(player)						\
		for (player = (void *)grid_players;			\
		     player;						\
		     player = (void *)((struct grid_data *)player->next))

#endif
