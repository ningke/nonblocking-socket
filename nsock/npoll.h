#ifndef _NPOLL_H
#define _NPOLL_H

#include <functional>
#include <string>

#include <inttypes.h>
#include <sys/epoll.h>

namespace npoll {

typedef std::function<void (int fd, uint32_t revents)> PollFunc;

int npollAddFd(int fd, uint32_t events, PollFunc callback);
int npollRemoveFd(int fd);
void npollLoop(bool &exitLoop);

}

#endif
