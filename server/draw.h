#ifndef DRAW_H
#define DRAW_H
#include <stdbool.h>

#define DRAW_MOD		15
#define DRAW_WIDTH		495
#define DRAW_HEIGHT		495

#define DRAW_MOD_WIDTH		(DRAW_WIDTH / DRAW_MOD)
#define DRAW_MOD_HEIGHT		(DRAW_HEIGHT / DRAW_MOD)

#define BUTTON_WAIT		1
#define BUTTON_KILL		2

void draw_init(void);

void draw_commit(void);
void draw_force_commit(void);

void draw_seconds(int seconds);
void draw_button(unsigned int button, bool on);

/* Clears the whole screen. */
void draw_clear(void);

/* Sets the upper left corner of the visible area to [x, y]. Note that if
 * you move the origin over a block boundary, you'll have to redraw all
 * items. If in doubt, always redraw after changing the origin. */
void draw_set_origin(int x, int y);

/* Draw an item to the [x, y] coordinates.
 * It's okay to pass items that are off screen to this function. However, if
 * it's cheap not to do that, it's of course more efficient. If you need to
 * check coords of each item individually to determine whether it's off
 * screen, do not bother and just call this function, it will do that for
 * you. */
void draw_item(int x, int y, unsigned angle, unsigned color);

#endif
