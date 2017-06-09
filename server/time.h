#ifndef TIME_H
#define TIME_H
#include <stdbool.h>
#include <time.h>

/* Time calculations */
struct timespec *time_now(struct timespec *tp);
void time_add(struct timespec *tp, long milisecs);
bool time_after(struct timespec *tp);
long time_left(struct timespec *tp);

#endif
