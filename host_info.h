#ifndef _HOST_INFO
#define _HOST_INFO

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

struct host_info_tag {
	char *ip_address;
	int   port;
	char *buf;
	int	  bufsize;
	int   sockfd;
	int   read_bytes;
	int   read_count;
    int   pipe_fd[2];
    pid_t pid;
    int   cpu_affinity;
	struct host_info_tag *next;
    long   read_bytes_interval;
    long   read_count_interval;
    int    so_rcvbuf;
    int    cpu_on;
};
typedef struct host_info_tag host_info;
typedef struct sockaddr SA;

/* SiTCP port */
#define DEFAULT_PORT    24
#define DEFAULT_BUFSIZE 1024

/* Taken from "The Practice of Programming, Kernighan and Pike" */
extern host_info *new_host(char *host_and_port);
extern host_info *addfront(host_info *host_list, host_info *newp);
extern host_info *addend(host_info *host_list, host_info *newp);
extern int        connect_to_server(host_info *host_info, int timeout);
extern int        dump_host_info(host_info *host_list);

extern int debug;
#endif
