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

int print_command_line(FILE *fp, int argc, char *argv[])
{
    fprintf(fp, "#");

    for (int i = 0; i < argc; ++i) {
        if (strchr(argv[i], ' ') == NULL) {
            fprintf(fp, " %s", argv[i]);
        }
        else {
            fprintf(fp, " \"%s\"", argv[i]);
        }
    }

    fprintf(fp, "\n");
    
    return 0;
}
