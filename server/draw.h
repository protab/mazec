#ifndef DRAW_H
#define DRAW_H
#include <stdbool.h>

#define DRAW_MOD		15
#define DRAW_WIDTH		525
#define DRAW_HEIGHT		525
#define DRAW_VIEWPORT_WIDTH	495
#define DRAW_VIEWPORT_HEIGHT	495

#define DRAW_MOD_WIDTH		(DRAW_WIDTH / DRAW_MOD)
#define DRAW_MOD_HEIGHT		(DRAW_HEIGHT / DRAW_MOD)
#define DRAW_MOD_VPORT_WIDTH	(DRAW_VIEWPORT_WIDTH / DRAW_MOD)
#define DRAW_MOD_VPORT_HEIGHT	(DRAW_VIEWPORT_HEIGHT / DRAW_MOD)

#define BUTTON_WAIT		1
#define BUTTON_KILL		2

void draw_init(void);

void draw_commit(void);
void draw_force_commit(void);

void draw_seconds(int seconds);
void draw_button(unsigned int button, bool on);

void draw_clear(void);
void draw_item(unsigned x, unsigned y, unsigned angle, unsigned color);
void draw_viewport(unsigned x, unsigned y);

#endif
