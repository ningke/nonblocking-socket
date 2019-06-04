#include <sstream>
#include <map>

#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "npoll.h"
#include "util.h"

using namespace std;

namespace npoll {


class NPollStruct {
public:
    NPollStruct();
    ~NPollStruct();

    int addFd(int fd, uint32_t events, PollFunc callback);
    int removeFd(int fd);
    int waitForEvents(int timeoutMs=-1);

    int epollfd = -1;
    map<int, PollFunc> fdMap;
    struct epoll_event *epollEvents = nullptr;
    int epollEventsNr = 0;
};

static NPollStruct sNPollObj;


int npollAddFd(int fd, uint32_t events, PollFunc callback) {
    sNPollObj.addFd(fd, events, callback);
    return 0;
}


int npollRemoveFd(int fd) {
    sNPollObj.removeFd(fd);
    return 0;
}


void npollLoop(bool &exitLoop) {
    const int timerResMs = 1000;

    while (!exitLoop) {
        sNPollObj.waitForEvents(timerResMs);
    }

    nsock::log("%s: exiting...\n", __FUNCTION__);
}


NPollStruct::NPollStruct() {
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        stringstream ss;
        ss << "Failed epoll_create(): " << errno;
        throw runtime_error(ss.str());
    }

    epollEventsNr = 1024;
    epollEvents = (struct epoll_event *)calloc(epollEventsNr, sizeof(struct epoll_event));
}


NPollStruct::~NPollStruct() {
    if (!fdMap.empty()) {
        nsock::log("%s: when exiting, fdMap still has %d fds!\n", __FUNCTION__,
            fdMap.size());
    }

    if (epollfd != -1) {
        close(epollfd);
    }

    free(epollEvents);
}


int NPollStruct::waitForEvents(int timeoutMs) {
    int eventsNr = fdMap.size();

    if (eventsNr == 0) {
        return 0;
    }

    if (eventsNr > epollEventsNr) {
        epollEvents = (struct epoll_event *)reallocarray(epollEvents, eventsNr,
                                                         sizeof(struct epoll_event));
        assert(epollEvents);
        epollEventsNr = eventsNr;
    }

    int nfds = epoll_wait(epollfd, epollEvents, eventsNr, timeoutMs);
    if (nfds == -1) {
        nsock::log("%s: failed epoll_wait: %d\n", __FUNCTION__, errno);
        return -1;
    }

    int cnt = 0;
    for (int n = 0; n < nfds; n++) {
        auto evtFd = epollEvents[n].data.fd;
        auto it = fdMap.find(evtFd);
        if (it == fdMap.end()) {
            nsock::log("%s: epoll_wait returned unknown fd=%d!\n", __FUNCTION__, evtFd);
        }

        auto evtCb = it->second;
        evtCb(evtFd, epollEvents[n].events);

        ++cnt;
    }

    return cnt;
}


int NPollStruct::addFd(int fd, uint32_t events, PollFunc callback) {
    auto it = fdMap.find(fd);

    if (it != fdMap.end()) {
        // Already exists
        nsock::log("%s: fd %d is already being monitored\n", __FUNCTION__, fd);
        return 0;
    }

    fdMap.insert({fd, callback});

    struct epoll_event ev = {0};
    ev.events = events;
    ev.data.fd = fd;
    int err = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    if (err) {
        nsock::log("%s: failed epoll_ctl for fd %d: error=%d\n", __FUNCTION__, fd, errno);
        fdMap.erase(fd);
        return -1;
    }

    return 0;
}


int NPollStruct::removeFd(int fd) {
    auto it = fdMap.find(fd);

    if (it == fdMap.end()) {
        nsock::log("%s: fd %d is not being monitored\n", __FUNCTION__, fd);
        return 0;
    }

    int err = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    if (err) {
        nsock::log("%s: failed epoll_ctl for fd %d: error=%d\n", __FUNCTION__, fd, errno);
        return -1;
    }

    fdMap.erase(fd);

    return 0;
}


}
