#ifndef EVENT_H
#define EVENT_H
#include <stdbool.h>
#include <sys/epoll.h>

#define EQUIT	256

#define EV_READ		EPOLLIN
#define EV_WRITE	EPOLLOUT

/* Return values: 0 to keep, 1 to disable (but not remove!), 2 to terminate
 * the event loop. */
typedef int (*event_callback_t)(int fd, unsigned events, void *data);

typedef void (*cb_data_destructor_t)(void *data);

int event_init(void);
int event_add_fd(int fd, unsigned events, event_callback_t cb, void *cb_data,
		 cb_data_destructor_t cb_destructor);
int event_del_fd(int fd);
int event_enable_fd(int fd, bool enable);
int event_change_fd(int fd, unsigned events);
int event_loop(void);

int timer_new(event_callback_t cb, void *cb_data,
	      cb_data_destructor_t cb_destructor);
int timer_arm(int fd, int milisecs);

#endif
