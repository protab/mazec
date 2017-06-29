#include "draw.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "websocket_data.h"

struct sprite {
	unsigned x;
	unsigned y;
	unsigned char angle;
	unsigned char color;
	struct sprite *next;
};

static int seconds;
static unsigned buttons;
static int bank;
static int x_orig, y_orig;
static int x_start, y_start;
static unsigned char fixed[(DRAW_MOD_WIDTH + 1) * (DRAW_MOD_HEIGHT + 1)];
static struct sprite *floating;
static struct sprite **floating_last;

#define fixed_coords(x, y)	((y) * (DRAW_MOD_WIDTH + 1) + (x))

#define MSG_MAX_LEN	(4 + sizeof(fixed) + 4 * (DRAW_MOD_WIDTH + 2) * (DRAW_MOD_HEIGHT + 2))
static unsigned char *msg;
static unsigned int msg_len;
static bool was_changed;

void draw_init(void)
{
	seconds = -1;
	buttons = 0;
	bank = 0;
	x_orig = y_orig = 0;
	x_start = y_start = 0;
	floating = NULL;
	draw_clear();

	msg = salloc(MSG_MAX_LEN);
	msg_len = 0;
}

static void emit_field(unsigned char color, unsigned cnt)
{
	while (cnt) {
		bool rle = cnt > 2;
		unsigned rle_cnt;

		msg[msg_len++] = color | (rle ? 0x80 : 0);
		if (!rle) {
			cnt--;
			continue;
		}
		rle_cnt = cnt;
		if (rle_cnt > 258)
			rle_cnt = 258;
		msg[msg_len++] = rle_cnt - 3;
		cnt -= rle_cnt;
	}
}

void draw_force_commit(void)
{
	unsigned rsec, cnt;
	int last_color;

	if (seconds < 0)
		rsec = 0x3ff;
	else
		rsec = seconds;

	msg[0] = (x_orig - x_start) << 4 | (y_orig - y_start);
	msg[1] = ((rsec & 0x300) >> 2) | buttons;
	msg[2] = rsec & 0xff;
	msg[3] = bank;
	msg_len = 4;

	cnt = 0;
	last_color = -1;
	for (unsigned i = 0; i < sizeof(fixed); i++) {
		if (fixed[i] == last_color) {
			cnt++;
			continue;
		}
		if (last_color >= 0)
			emit_field(last_color, cnt);
		last_color = fixed[i];
		cnt = 1;
	}
	emit_field(last_color, cnt);

	for (struct sprite *p = floating; p; p = p->next) {
		bool rot = !!p->angle;

		msg[msg_len++] = (p->x & 0x100) >> 2 |
				 (p->y & 0x100) >> 1 |
				 (rot ? 0x20 : 0) |
				 p->color;
		msg[msg_len++] = p->x & 0xff;
		msg[msg_len++] = p->y & 0xff;
		if (rot)
			msg[msg_len++] = p->angle & 0x7f;
	}
	websocket_broadcast(msg, msg_len);

	was_changed = false;
}

void draw_commit(void)
{
	if (was_changed)
		draw_force_commit();
}

void draw_seconds(int seconds_)
{
	if (seconds_ == seconds)
		return;

	seconds = seconds_;
	was_changed = true;
}

void draw_button(unsigned int button, bool on)
{
	unsigned new_buttons = buttons;

	button = 1 << (button - 1);
	if (on)
		new_buttons |= button;
	else
		new_buttons &= ~button;

	if (new_buttons == buttons)
		return;

	buttons = new_buttons;
	was_changed = true;
}

void draw_set_bank(int bank_)
{
	bank = bank_;
	was_changed = true;
}

void draw_clear(void)
{
	while (floating) {
		struct sprite *next = floating->next;

		sfree(floating);
		floating = next;
	}
	floating_last = &floating;
	memset(fixed, 0, sizeof(fixed));

	was_changed = true;
}

void draw_item(int x, int y, unsigned angle, unsigned color)
{
	struct sprite *p;

	if (x <= x_orig - DRAW_MOD || y <= y_orig - DRAW_MOD ||
	    x >= x_orig + DRAW_WIDTH || y >= y_orig + DRAW_HEIGHT)
		/* the item is off screen */
		return;

	was_changed = true;

	x -= x_start;
	y -= y_start;

	if (!(x % DRAW_MOD) && !(y % DRAW_MOD) && !angle) {
		fixed[fixed_coords(x / DRAW_MOD, y / DRAW_MOD)] = color;
		return;
	}
	p = salloc(sizeof(*p));
	p->x = x + DRAW_MOD;	/* this is never negative */
	p->y = y + DRAW_MOD;
	p->angle = angle / 3;
	p->color = color;
	p->next = NULL;
	*floating_last = p;
	floating_last = &p->next;
}

static int modulo(int dividend, int divisor)
{
	int res = dividend % divisor;
	if (dividend < 0 && res != 0)
		res += divisor;
	return res;
}

void draw_set_origin(int x, int y)
{
	if (x_orig == x && y_orig == y)
		return;

	x_orig = x;
	y_orig = y;
	x_start = x - modulo(x, DRAW_MOD);
	y_start = y - modulo(y, DRAW_MOD);

	was_changed = true;
}
