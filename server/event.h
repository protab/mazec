#ifndef EVENT_H
#define EVENT_H
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/types.h>

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
void event_ignore_pid(pid_t pid);
int event_loop(void);
void event_quit(void);

/* Timers */

/* For return values, see event_callback_t. The "count" parameter contains
 * a value indicating how many times the alarm was fired since the last
 * call. This may be greater than one e.g. in case it was paused. */
typedef int (*timer_callback_t)(int fd, int count, void *data);

/* Adds a new timer. "cb" is the callback that is called whenever the timer
 * fires, "cb_data" is user data passed to the callback, "cb_destructor" is
 * used to release "cb_data" when the timer is deleted. The return value is
 * the timer identifier or < 0 in case of an error. */
int timer_new(timer_callback_t cb, void *cb_data,
	      cb_data_destructor_t cb_destructor);

/* Arms (sets) the given timer. It will fire after the given number of
 * miliseconds. If "repeat" is false, it will be a one shot (but it's of
 * course possible to call "timer_arm" again), if it's true, the timer will
 * fire repeatedly every "milisecs". */
int timer_arm(int fd, int milisecs, bool repeat);

/* Disarms the given timer. It's guaranteed that the callback won't be
 * called after this call. */
int timer_disarm(int fd);

/* Deletes the given timer. If it's armed, it will be automatically
 * disarmed. */
int timer_del(int fd);

#endif
