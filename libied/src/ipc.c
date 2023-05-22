/*
 * Here we deal with interprocess communication
 *
 * This is done via named pipes, to keep things simple.
 */

#include <libied/cfg.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

FILE *open_ipc_pipe(const char *path, int rw) {
    FILE *fp = NULL;
    const char *fm = NULL;
    static const char opn_r[] = "r";
    static const char opn_a[] = "a+";

    if (rw == 1) {
       fm = opn_a;
    } else {
       fm = opn_r;
    }

    if ((fp = fopen(path, fm)) == NULL) {
       fprintf(stderr, "open_ipc_pipe<%s,%d>: failed to open: %d:%s\n", path, rw, errno, strerror(errno));
       return NULL;
    }

    // enforce whole writes on the pipe
    if (rw == 1) {
       setvbuf(fp, NULL, _IOLBF, PIPE_BUF);
    }

    return fp;
}
