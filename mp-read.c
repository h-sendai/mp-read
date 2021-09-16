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
#include "print_command_line.h"

int debug   = 0;
int bufsize = 2*1024*1024; // default 2 MB
host_info *host_list = NULL;
volatile sig_atomic_t has_sigusr1 = 0;
int enable_quickack = 0;
int disable_quickack = 0;

int usage()
{
    char msg[] = "Usage: mp-read [-i interval_sec] [-b bufsize] [-d] [-q] [-c cpu_num -c cpu_num ...] ip_address:port [ip_address:port ...]\n"
                 "-i: interval_sec (default: 1 second.  decimal value allowed)\n"
                 "-b: bufsize for reading socket (default: 2 MB). k for kilo, m for mega\n"
                 "-q: enable quickack once\n"
                 "-qq: enable quickack before every read()\n"
                 "-c: cpu_num.  may specify multiple times\n"
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
    pid_t pgid = getpgid(0);
    if (debug) {
        fprintf(stderr, "child_proc: %d %d ip_address: %s\n", pid, pgid, p->ip_address);
        fprintf(stderr, "cpu_affinity: %d\n", p->cpu_affinity);
    }

    if (p->cpu_affinity != -1) {
        if (set_cpu(p->cpu_affinity) < 0) {
            fprintf(stderr, "pid: %d set_cpu() failed", pid);
            exit(1);
        }
    }

    int pipe_rd_end = p->pipe_fd[0];
    int pipe_wr_end = p->pipe_fd[1];
    close(pipe_rd_end);

    long read_bytes_interval = 0;
    long read_count_interval = 0;

    int sockfd = tcp_socket();
    if (connect_tcp(sockfd, p->ip_address, p->port) < 0) {
        warnx("connect_tcp fail: %s", p->ip_address);
        killpg(0, SIGTERM); /* send SIGTERM to all child and parent using process group id */
    }

    if (enable_quickack) {
        if (set_so_quickack(sockfd) < 0) {
            errx(1, "set_so_quickack");
        }
    }
    if (disable_quickack) {
        int qack = 0;
        setsockopt(sockfd, IPPROTO_TCP, TCP_QUICKACK, &qack, sizeof(qack));
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

        if (enable_quickack > 1) {
            if (set_so_quickack(sockfd) < 0) {
                errx(1, "set_so_quickack");
            }
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
    char *interval_sec_str = "1.0";
    int __attribute__((unused)) total_sec = 10;
    int c;

    print_command_line(stdout, argc, argv);

    int max_n_cpu = 1024;
    int cpu_affinity[max_n_cpu];
    for (int i = 0; i < max_n_cpu; ++i) {
        cpu_affinity[i] = -1;
    }
    int cpu_affinity_index = 0;

    while ( (c = getopt(argc, argv, "b:c:dhi:qQt:")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'c':
                cpu_affinity[cpu_affinity_index] = strtol(optarg, NULL, 0);
                cpu_affinity_index ++;
                break;
            case 'd':
                debug = 1;
                break;
            case 'i':
                interval_sec_str = optarg;
                break;
            case 'h':
                usage();
                exit(0);
            case 'q':
                enable_quickack += 1;
                break;
            case 'Q':
                disable_quickack = 1;
                break;
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
        fprintf(stderr, "enable_quickack: %d\n", enable_quickack);
    }

    for (int i = 0; i < argc; ++i) {
        host_list = addend(host_list, new_host(argv[i]));
    }

    {
        // assign cpu affinity in host_info structure
        int i;
        host_info *p;
        for (p = host_list, i = 0; p != NULL; p = p->next, ++i) {
            p->cpu_affinity = cpu_affinity[i];
        }
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
    struct timeval interval;
    conv_str2timeval(interval_sec_str, &interval);
    set_timer(interval.tv_sec, interval.tv_usec, interval.tv_sec, interval.tv_usec);
    my_signal(SIGALRM, sig_alrm);

    struct timeval tv_start, elapsed, tv_prev, tv_interval, now;
    gettimeofday(&tv_start, NULL);
    tv_prev = tv_start;
    for ( ; ; ) {
        pause();
        gettimeofday(&now, NULL);
        timersub(&now, &tv_start, &elapsed);
        timersub(&now, &tv_prev,  &tv_interval);
        double interval_sec = tv_interval.tv_sec + 0.000001*tv_interval.tv_usec;
        printf("%ld.%06ld", elapsed.tv_sec, elapsed.tv_usec);
        for (host_info *p = host_list; p != NULL; p = p->next) {
            kill(p->pid, SIGUSR1);
        }
        long total_bytes = 0;
        for (host_info *p = host_list; p != NULL; p = p->next) {
            //long bytes;
            //long count;
            int n;
            n = read(p->pipe_fd[0], &p->read_bytes_interval, sizeof(p->read_bytes_interval));
            if (n < 0) {
                err(1, "read pipe from child fail");
            }
            n = read(p->pipe_fd[0], &p->read_count_interval, sizeof(p->read_count_interval));
            if (n < 0) {
                err(1, "read pipe from child fail");
            }
            total_bytes += p->read_bytes_interval;
        }
        printf(" Gbps:");
        for (host_info *p = host_list; p != NULL; p = p->next) {
            double transfer_rate_MB_s = p->read_bytes_interval / 1024.0 / 1024.0 / interval_sec;
            double transfer_rate_Gbps = MiB2Gb(transfer_rate_MB_s);
            //printf(" %.3f Gbps", transfer_rate_Gbps);
            printf(" %.3f", transfer_rate_Gbps);
        }
        printf(" MB/s:");
        for (host_info *p = host_list; p != NULL; p = p->next) {
            double transfer_rate_MB_s = p->read_bytes_interval / 1024.0 / 1024.0 / interval_sec;
            printf(" %.3f", transfer_rate_MB_s);
        }
        printf(" read_count:");
        for (host_info *p = host_list; p != NULL; p = p->next) {
            printf(" %5ld", p->read_count_interval);
        }
        double total_transfer_rate_MB_s = total_bytes / 1024.0 / 1024.0 / interval_sec;
        double total_transfer_rate_Gbps = MiB2Gb(total_transfer_rate_MB_s);
        //printf(" %.3f MB/s %.3f Gbps\n", total_bytes/1024.0/1024.0);
        printf(" total: %.3f Gbps %.3f MB/s\n", total_transfer_rate_Gbps, total_transfer_rate_MB_s);
        fflush(stdout);
        tv_prev = now;
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
