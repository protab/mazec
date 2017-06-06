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
static unsigned x_ofs;
static unsigned y_ofs;
static unsigned char fixed[(DRAW_MOD_WIDTH - 1) * (DRAW_MOD_HEIGHT - 1)];
static struct sprite *floating;
static struct sprite **floating_last;

#define fixed_coords(x, y)	((y) * (DRAW_MOD_WIDTH - 1) + (x))

#define MSG_MAX_LEN	(3 + sizeof(fixed) + 4 * DRAW_MOD_WIDTH * DRAW_MOD_HEIGHT)
static unsigned char *msg;
static unsigned int msg_len;
static bool was_changed;

void draw_init(void)
{
	seconds = -1;
	buttons = 0;
	x_ofs = y_ofs = 15;
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

	msg[0] = (y_ofs - 15) << 4 | (x_ofs - 15);
	msg[1] = (rsec & 0x300 >> 2) | buttons;
	msg[2] = rsec & 0xff;
	msg_len = 3;

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

/* -1 seconds means infinity */
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

void draw_item(unsigned x, unsigned y, unsigned angle, unsigned color)
{
	struct sprite *p;

	was_changed = true;

	if (!(x % 15) && !(y % 15) && !angle) {
		if (x == 0 || y == 0)
			/* the first row and the first column are not transfered */
			return;
		fixed[fixed_coords(x / 15 - 1, y / 15 - 1)] = color;
		return;
	}
	p = salloc(sizeof(*p));
	p->x = x;
	p->y = y;
	p->angle = angle / 3;
	p->color = color;
	p->next = NULL;
	*floating_last = p;
	floating_last = &p->next;
}

/* Both x and y must be >= 15. */
void draw_viewport(unsigned x, unsigned y)
{
	if (x_ofs == x && y_ofs == y)
		return;

	x_ofs = x;
	y_ofs = y;

	was_changed = true;
}
