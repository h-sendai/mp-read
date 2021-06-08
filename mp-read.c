#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "get_num.h"
#include "host_info.h"
#include "my_signal.h"
#include "my_socket.h"
#include "set_timer.h"
#include "set_cpu.h"  // not used.  may be used in reading 10GbE x 4 experiment (?)

int debug   = 0;
int bufsize = 128*1024; // default 128kB
host_info *host_list = NULL;
volatile sig_atomic_t has_sigusr1 = 0;

int usage()
{
    char msg[] = "Usage: mp-read [-i interval_sec] [-b bufsize] [-d] ip_address:port [ip_address:port ...]\n"
                 "-i: interval_sec (default: 1 second)\n"
                 "-b: bufsize for reading socket (default: 128kB). k for kilo, m for mega\n"
                 "-d: debug\n";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

void sig_usr1(int signo)
{
    has_sigusr1 = 1;
#if 0
    struct timeval tv;
    gettimeofday(&tv, NULL);
    pid_t pid = getpid();
    fprintf(stderr, "%d %ld.%06ld\n", pid, tv.tv_sec, tv.tv_usec);
#endif
    return;
}

void sig_alrm(int signo)
{
    return;
}

int child_proc(host_info *p)
{
    my_signal(SIGUSR1, sig_usr1);

    pid_t pid = getpid();
    if (debug) {
        fprintf(stderr, "child_proc: %d ip_address: %s\n", pid, p->ip_address);
    }

    int pipe_rd_end = p->pipe_fd[0];
    int pipe_wr_end = p->pipe_fd[1];
    close(pipe_rd_end);

    long read_bytes_interval = 0;
    long read_count_interval = 0;

    int sockfd = tcp_socket();
    if (connect_tcp(sockfd, p->ip_address, p->port) < 0) {
        errx(1, "connect_tcp fail");
    }

    char *buf = malloc(bufsize);
    if (buf == NULL) {
        fprintf(stderr, "malloc for socket read buf");
    }

    for ( ; ; ) {
        if (has_sigusr1) {
            int n;
            n = write(pipe_wr_end, &read_bytes_interval, sizeof(read_bytes_interval));
            if (n < 0) {
                err(1, "write pipe error");
            }
            n = write(pipe_wr_end, &read_count_interval, sizeof(read_count_interval));
            if (n < 0) {
                err(1, "write pipe error");
            }
            read_bytes_interval = 0;
            read_count_interval = 0;
            has_sigusr1         = 0;
        }
        int n = read(sockfd, buf, bufsize);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                err(1, "read");
            }
        }
        read_bytes_interval += n;
        read_count_interval += 1;
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    int interval_sec = 1;
    int __attribute__((unused)) total_sec    = 10;
    int c;
    while ( (c = getopt(argc, argv, "b:dhi:t:")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'd':
                debug = 1;
                break;
            case 'i':
                interval_sec = strtol(optarg, NULL, 0);
                break;
            case 'h':
                usage();
                exit(0);
            case 't':
                total_sec = strtol(optarg, NULL, 0);
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc  == 0) {
        usage();
        exit(1);
    }
    if (debug) {
        fprintf(stderr, "bufsize: %d bytes\n", bufsize);
    }

    for (int i = 0; i < argc; ++i) {
        host_list = addend(host_list, new_host(argv[i]));
    }
    if (debug) {
        dump_host_info(host_list);
    }

    for (host_info *p = host_list; p != NULL; p = p->next) {
        if (pipe(p->pipe_fd) < 0) {
            err(1, "pipe");
        }
        pid_t child_pid = fork();
        if (child_pid == 0) { /* child */
            child_proc(p);
            exit(0);
        }
        else { /* parent */
            // save child_pid to send SIGUSR1
            p->pid = child_pid;
            close(p->pipe_fd[1]);
        }
    }
 
    // parent.  data gatherer
    set_timer(interval_sec, 0, interval_sec, 0);
    my_signal(SIGALRM, sig_alrm);

    struct timeval tv_start, elapsed, tv_prev, tv_interval, now;
    gettimeofday(&tv_start, NULL);
    tv_prev = tv_start;
    for ( ; ; ) {
        pause();
        gettimeofday(&now, NULL);
        timersub(&now, &tv_start, &elapsed);
        timersub(&now, &tv_prev,  &tv_interval);
        printf("%ld.%06ld", tv_interval.tv_sec, tv_interval.tv_usec);
        for (host_info *p = host_list; p != NULL; p = p->next) {
            kill(p->pid, SIGUSR1);
        }
        long total_bytes = 0;
        for (host_info *p = host_list; p != NULL; p = p->next) {
            long bytes;
            long count;
            int n;
            n = read(p->pipe_fd[0], &bytes, sizeof(bytes));
            if (n < 0) {
                err(1, "read pipe from child fail");
            }
            n = read(p->pipe_fd[0], &count, sizeof(count));
            if (n < 0) {
                err(1, "read pipe from child fail");
            }
            printf(" %.3f ( %ld )", bytes/1024.0/1024.0/(double)interval_sec, count/interval_sec);
            total_bytes += bytes;
        }
        printf(" %.3f\n", total_bytes/1024.0/1024.0);
    }

    // SIG_INT will be sent to parent and child processes
    // We don't need following lines 
    //usleep(10000);
    //for (host_info *p = host_list; p != NULL; p = p->next) {
    //    kill(p->pid, SIGTERM);
    //}

    //wait all child procs
    //pid_t pid;
    //int   stat;
    //while ( (pid = waitpid(-1, &stat, 0)) > 0) {
    //    fprintf(stderr, "pid:%d done\n", pid);
    //}

    return 0;
}
