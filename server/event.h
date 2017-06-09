#ifndef EVENT_H
#define EVENT_H
#include <stdbool.h>
#include <sys/epoll.h>

#define EV_READ		EPOLLIN
#define EV_WRITE	EPOLLOUT
#define EV_SOCK		EPOLLRDHUP
#define EV_ERROR	(EPOLLERR | EPOLLHUP | EPOLLRDHUP)

/* Return values: 0 to keep, 1 to disable (but not remove!), < 0 to
 * signalize error and terminate the event loop. To cleanly shutdown, call
 * event_quit. */
typedef int (*event_callback_t)(int fd, unsigned events, void *data);

typedef void (*cb_data_destructor_t)(void *data);

int event_init(void);
int event_add_fd(int fd, unsigned events, event_callback_t cb, void *cb_data,
		 cb_data_destructor_t cb_destructor);
int event_del_fd(int fd);
int event_enable_fd(int fd, bool enable);
int event_pause_fd(int fd, bool pause);
int event_change_fd(int fd, unsigned events);
int event_change_fd_add(int fd, unsigned events);
int event_change_fd_remove(int fd, unsigned events);
int event_loop(void);
void event_quit(void);

int timer_new(event_callback_t cb, void *cb_data,
	      cb_data_destructor_t cb_destructor);

/* pass 0 in milisecs to disarm */
int timer_arm(int fd, int milisecs, bool repeat);

int timer_snooze(int fd);
int timer_del(int fd);

#endif
