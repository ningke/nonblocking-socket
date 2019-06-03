#include <iostream>
#include <sstream>

#include <stdio.h>
#include <fcntl.h>

namespace nsock {

int setnonblocking(int fd);

void log(const char *fmt, ...);

}
