#include "time.h"
#include "common.h"

static struct timespec cache;
static bool cache_valid;

void time_flush_cache(void)
{
	cache_valid = false;
}

struct timespec *time_now(void)
{
	if (!cache_valid) {
		check(clock_gettime(CLOCK_MONOTONIC, &cache));
		cache_valid = true;
	}
	return &cache;
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

void time_from_now(struct timespec *tp, long milisecs)
{
	time_now();
	*tp = cache;
	time_add(tp, milisecs);
}

bool time_after(struct timespec *tp)
{
	time_now();
	if (cache.tv_sec > tp->tv_sec)
		return true;
	if (cache.tv_sec < tp->tv_sec)
		return false;
	return cache.tv_nsec > tp->tv_nsec;
}

static long time_diff(struct timespec *tp1, struct timespec *tp2)
{
	return (tp1->tv_sec - tp2->tv_sec) * 1000 +
	       (tp1->tv_nsec - tp2->tv_nsec) / 1000000;
}

long time_left(struct timespec *tp)
{
	long res;

	res = time_diff(tp, time_now());
	if (res < 0)
		res = 0;
	return res;
}

long time_elapsed(struct timespec *tp)
{
	return time_diff(time_now(), tp);
}
