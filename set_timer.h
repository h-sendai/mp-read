#ifndef _SET_TIMER
#define _SET_TIMER 1

#include <sys/time.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

extern int set_timer(long sec, long usec, long sec_interval, long usec_interval);
extern struct timeval float2timeval(double x);
extern struct timeval str2timeval(char *str);
extern int conv_str2timeval(char *str, struct timeval *result);
extern useconds_t str2useconds(char *str);
extern int wait_alarm();
#endif

