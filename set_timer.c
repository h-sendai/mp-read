#include "set_timer.h"

int set_timer(long sec, long usec, long sec_interval, long usec_interval)
{
	struct itimerval interval;

	interval.it_interval.tv_sec  = sec_interval;
	interval.it_interval.tv_usec = usec_interval;
	interval.it_value.tv_sec  = sec;
	interval.it_value.tv_usec = usec;

	if (setitimer(ITIMER_REAL, &interval, NULL) < 0) {
		//perror("setimer");
		return -1;
	}

    return 0;
}

struct timeval float2timeval(double x)
{
    struct timeval tv;

    tv.tv_sec = (time_t) x;

    double d = x - tv.tv_sec;
    d = d*1000000.0;

    tv.tv_sec = (time_t) x;
    tv.tv_usec = (unsigned int)d;

    return tv;
}

struct timeval str2timeval(char *str)
{
    struct timeval tv;
    double x = strtod(str, NULL);
    
    tv = float2timeval(x);

    return tv;
}

useconds_t str2useconds(char *str)
{
    struct timeval tv;
    tv = str2timeval(str);

    return 1000000*tv.tv_sec + tv.tv_usec;
}
