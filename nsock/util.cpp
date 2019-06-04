#include <string>
#include <sstream>
#include <chrono>

#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "assert.h"
#include "util.h"
#include "stdio.h"
#include "stdarg.h"


using namespace std;

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


static string sLogFilePath = "/tmp/nsock/nsock";
static FILE *sLogSink = nullptr;


void logSetPath(const string &logFilePath) {
    stringstream ss;
    ss << logFilePath << "-" << getpid() << ".log";

    sLogFilePath = ss.str();
}

void log(const char *fmt, ...) {
    if (!sLogSink) {
        sLogSink = fopen(sLogFilePath.c_str(), "w");
        assert(sLogSink);
    }

    va_list ap;
    va_start(ap, fmt);
    vfprintf(sLogSink, fmt, ap);
    fflush(sLogSink);
    va_end(ap);
}

}
