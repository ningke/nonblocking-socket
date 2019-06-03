#include <string>

#include "assert.h"
#include "util.h"
#include "stdio.h"
#include "stdarg.h"


namespace nsock {


/*
 * Set a socket to non-blocking.
 */
int setnonblocking(int fd) {
    int flags;
    int err;

    // Get existing file status flags and add O_NONBLOCK if needed.
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -errno;
    }

    // return if already set
    if (flags & O_NONBLOCK) {
        return 0;
    }

    flags |= O_NONBLOCK;
    err = fcntl(fd, F_SETFL, flags);
    if (err) {
        return -errno;
    }

    return 0;
}


static const char *sLogFilePath = "/tmp/nsock.log";
static FILE *sLogSink = nullptr;


void log(const char *fmt, ...) {
    if (!sLogSink) {
        sLogSink = fopen(sLogFilePath, "w");
        assert(sLogSink);
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(sLogSink, fmt, ap);
    fflush(sLogSink);
    va_end(ap);
}

}
