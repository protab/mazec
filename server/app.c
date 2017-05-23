#include "app.h"
#include <dlfcn.h>
#include <string.h>
#include "config.h"
#include "common.h"
#include "log.h"

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
	return ops;
}
