#ifndef TIMEMATH_H
#define TIMEMATH_H

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>

#ifdef HAVE_CLOCK_GETTIME
#define clocktime_t struct timespec
#else
#define clocktime_t struct timeval
#endif

#ifdef HAVE_CLOCK_GETTIME
#define CT_FRACTION 1000000000
#else
#define CT_FRACTION 1000000
#endif

#ifdef HAVE_CLOCK_GETTIME
#define TIME_S(t)  ((t).tv_sec)
#define TIME_SS(t) ((t).tv_nsec)
#else
#define TIME_S(t)  ((t).tv_sec)
#define TIME_SS(t) ((t).tv_usec)
#endif

#define PTS_TO_CLOCKTIME(xtime, PTS) {            \
  TIME_S(xtime) = PTS/90000;                      \
  TIME_SS(xtime) = (PTS%90000)*CT_FRACTION/90000; \
}

void clocktime_get(clocktime_t *d);

void timesub(clocktime_t *d, const clocktime_t *s1, const clocktime_t *s2);

void timeadd(clocktime_t *d, const clocktime_t *s1, const clocktime_t *s2);

int timecompare(const clocktime_t *s1, const clocktime_t *s2); 

#endif /* TIMEMATH_H */
