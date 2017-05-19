#include "common.h"
#include <stdlib.h>
#include "log.h"

void emerg_exit(const char *path, int line, int code)
{
	log_err("fatal error %d at %s:%d", -code, path, line);
	exit(1);
}
