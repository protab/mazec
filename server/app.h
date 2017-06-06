#ifndef APP_H
#define APP_H
#include "level.h"

const struct level_ops *app_get_level(char *code);
void app_redraw(const struct level_ops *level);

#endif
