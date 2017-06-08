#include "app.h"
#include <dlfcn.h>
#include <string.h>
#include "config.h"
#include "common.h"
#include "draw.h"
#include "event.h"
#include "log.h"
#include "proto.h"
#include "websocket_data.h"

static bool dirty;

#define MAX_PATH_LEN	(sizeof(LEVELS_DIR) + LOGIN_LEN + 3)

const struct level_ops *app_get_level(char *code)
{
	char path[MAX_PATH_LEN];
	size_t pos;
	void *handle;
	char *err;
	struct level_ops *ops;

	pos = strlcpy(path, LEVELS_DIR, MAX_PATH_LEN);
	pos += strlcpy(path + pos, code, MAX_PATH_LEN - pos);
	pos += strlcpy(path + pos, ".so", MAX_PATH_LEN - pos);
	handle = dlopen(path, RTLD_NOW);
	if (!handle)
		return NULL;
	dlerror();
	ops = dlsym(handle, "level_ops");
	err = dlerror();
	if (err || !ops) {
		if (!err)
			err = "the symbol is NULL";
		log_err("import error from library %s: %s", path, err);
		dlclose(handle);
		return NULL;
	}
	if (ops->init)
		ops->init();
	dirty = true;
	return ops;
}

void app_redraw(const struct level_ops *level)
{
	if (!websocket_connected())
		return;

	if (dirty)
		level->redraw();
	draw_commit();
	dirty = false;
}

/* This function is defined in level.h but implemented here. This is to keep
 * the levels source code contained. */
void level_dirty(void)
{
	dirty = true;
}

void app_remote_command(struct socket *s __unused, void *buf, size_t len)
{
	unsigned char cmd;

	if (len > 1) {
		log_warn("got remote websocket command with len %zu", len);
		return;
	}
	cmd = ((unsigned char *)buf)[0];
	log_info("got remote command 0x%x", cmd);
	if (cmd & 0x80)
		return;
	switch (cmd & 0x7f) {
	case BUTTON_WAIT:
		proto_resume();
		break;
	case BUTTON_KILL:
		event_quit();
		break;
	}
}
