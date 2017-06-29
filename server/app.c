#include "app.h"
#include <dlfcn.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "common.h"
#include "draw.h"
#include "event.h"
#include "log.h"
#include "proto.h"
#include "pybindings.h"
#include "websocket_data.h"

static bool dirty;

#define MAX_PATH_LEN	(sizeof(LEVELS_DIR) + LOGIN_LEN + 3 + 1)
#define MAX_PYPATH_LEN	(sizeof(PYLEVELS_DIR) + LOGIN_LEN + 5 + 1)

static bool file_exists(const char *path, bool symlink)
{
	struct stat sb;

	if (lstat(path, &sb) < 0)
		return false;
	if (symlink)
		return S_ISLNK(sb.st_mode);
	return S_ISREG(sb.st_mode);
}

static const struct level_ops *so_get_level(char *code)
{
	char path[MAX_PATH_LEN];
	size_t pos;
	void *handle;
	char *err;
	struct level_ops *ops;

	pos = strlcpy(path, LEVELS_DIR, MAX_PATH_LEN);
	pos += strlcpy(path + pos, code, MAX_PATH_LEN - pos);
	pos += strlcpy(path + pos, ".so", MAX_PATH_LEN - pos);
	if (!file_exists(path, false))
		return NULL;

	handle = dlopen(path, RTLD_NOW);
	err = dlerror();
	if (!handle) {
		log_info("library load error: %s", err);
		return NULL;
	}
	/* Need to call dlerror() before calling dlsym() to clear any
	 * possible previous error condition. We do that above. */
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

static const struct level_ops *py_get_level(char *code)
{
	char path[MAX_PYPATH_LEN];
	char sl[LOGIN_LEN + 3 + 1];
	size_t pos;
	ssize_t len;

	pos = strlcpy(path, PYLEVELS_DIR, MAX_PYPATH_LEN);
	pos += strlcpy(path + pos, "code_", MAX_PYPATH_LEN - pos);
	pos += strlcpy(path + pos, code, MAX_PYPATH_LEN - pos);
	if (!file_exists(path, true))
		return NULL;
	len = readlink(path, sl, sizeof(sl));
	if (len < 0) {
		log_err("cannot read symlink %s", path);
		return NULL;
	}
	if (len == sizeof(sl))
		len--;
	sl[len] = '\0';

	pos = strlcpy(path, PYLEVELS_DIR, MAX_PYPATH_LEN);
	pos += strlcpy(path + pos, sl, MAX_PYPATH_LEN - pos);
	if (!file_exists(path, false)) {
		log_err("dangling symlink to %s", path);
		return NULL;
	}

	return pyb_load(path);
}

const struct level_ops *app_get_level(char *code)
{
	const struct level_ops *res;

	res = so_get_level(code);
	if (res)
		return res;
	return py_get_level(code);
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
