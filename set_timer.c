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

int wait_alarm()
{
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigwait(&set, &sig);

    return 0;
}

/*    $OpenBSD: sleep.c,v 1.29 2020/02/25 15:46:15 cheloha Exp $    */
/*    $NetBSD: sleep.c,v 1.8 1995/03/21 09:11:11 cgd Exp $    */

/*
 * Copyright (c) 1988, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* From OpenBSD /usr/src/bin/sleep.c and mofdified */

/* 
 * conv_str2timeval(char * str, struct timeval *result)
 * Convert decimal string to struct timeval
 * Result will be filled in struct timeval result
 * On error, return -1.
 * valid string example: 1.23, .23
 * invalid string example: a.12, 1.2a
 */
int conv_str2timeval(char *str, struct timeval *result)
{
    struct timeval tv;
    time_t t;
    char *cp;
    int i;

    //timespecclear(&tv);
    tv.tv_sec        = 0;
    tv.tv_usec       = 0;

    /* Handle whole seconds. */
    for (cp = str; *cp != '\0' && *cp != '.'; cp++) {
        if (!isdigit((unsigned char)*cp)) {
            warnx("seconds is invalid: %s", str);
            return -1;
        }
        t = (tv.tv_sec * 10) + (*cp - '0');
        if (t / 10 != tv.tv_sec) {   /* overflow */
            warnx("seconds is too large: %s", str);
            return -1;
        }

        tv.tv_sec = t;
    }

    /*
     * Handle fractions of a second.  The multiplier divides to zero
     * after nine digits so anything more precise than a microsecond is
     * validated but not used.
     */
    if (*cp == '.') {
        i = 100000;
        for (cp++; *cp != '\0'; cp++) {
            if (!isdigit((unsigned char)*cp)) {
                warnx("seconds is invalid: %s", str);
                return -1;
            }
            tv.tv_usec += (*cp - '0') * i;
            i /= 10;
        }
    }

    result->tv_sec  = tv.tv_sec;
    result->tv_usec = tv.tv_usec;

    return 0;
}
