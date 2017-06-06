#ifndef APP_H
#define APP_H
#include "level.h"
#include "socket.h"

const struct level_ops *app_get_level(char *code);
void app_redraw(const struct level_ops *level);
void app_remote_command(struct socket *s, void *buf, size_t len);

#endif
