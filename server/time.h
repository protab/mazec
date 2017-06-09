#ifndef TIME_H
#define TIME_H
#include <stdbool.h>
#include <time.h>

/* Flush time cache for time_now. */
void time_flush_cache(void);

/* Returns the current time. The result is cached and subsequent calls will
 * return the same time until time_flush_cache is called. The returned value
 * is internal static buffer. */
struct timespec *time_now(void);

/* Adds the given amount of miliseconds to 'tp'. 'tp' must be
 * preinitialized. */
void time_add(struct timespec *tp, long milisecs);

/* Stores the time 'milisec' miliseconds in the future to 'tp'. */
void time_from_now(struct timespec *tp, long milisecs);

/* Checks whether 'tp' is in the past (i.e. the current time is after 'tp'). */
bool time_after(struct timespec *tp);

/* Returns the number of miliseconds left until 'tp'. */
long time_left(struct timespec *tp);

/* Returns the number of miliseconds elapsed since 'tp'. */
long time_elapsed(struct timespec *tp);

#endif
