#ifndef EVENT_H
#define EVENT_H

#include <stdbool.h>

typedef void (*event_callback_t)(int fd, void *data);

int event_init(void);
int event_add_fd(int fd, bool poll_write, event_callback_t cb, void *cb_data);
int event_del_fd(int fd);
int event_enable_fd(int fd, bool enable);
void event_loop(void);

int timer_new(event_callback_t cb, void *cb_data);
int timer_arm(int fd, int milisecs);

#endif
