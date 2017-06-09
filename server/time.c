#include "time.h"
#include "common.h"

struct timespec *time_now(struct timespec *tp)
{
	check(clock_gettime(CLOCK_MONOTONIC, tp));
	return tp;
}

void time_add(struct timespec *tp, long milisecs)
{
	time_t secs;
	long nsecs;

	secs = milisecs / 1000;
	nsecs = (milisecs % 1000) * 1000000;
	tp->tv_nsec += nsecs;
	if (tp->tv_nsec >= 1000000000) {
		tp->tv_nsec -= 1000000000;
		secs++;
	}
	tp->tv_sec += secs;
}

bool time_after(struct timespec *tp)
{
	struct timespec now;

	time_now(&now);
	if (now.tv_sec > tp->tv_sec)
		return true;
	if (now.tv_sec < tp->tv_sec)
		return false;
	return now.tv_nsec > tp->tv_nsec;
}

long time_left(struct timespec *tp)
{
	struct timespec now;
	long res;

	time_now(&now);
	res = (tp->tv_sec - now.tv_sec) * 1000 +
	      (tp->tv_nsec - now.tv_nsec) / 1000000;
	if (res < 0)
		res = 0;
	return res;
}
