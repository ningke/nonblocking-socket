#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "nsock.h"
#include "npoll.h"
#include "util.h"


using namespace std;
using namespace npoll;

namespace nsock {

uint64_t NSock::sNSockId = 0;

NSock::NSock(int sfd) :
    id(getNextNSockId()), sockfd(sfd) {
}


NSock::~NSock() {
    log("%s: sockId=%lu\n", __FUNCTION__, getId());
    end();
}


/*
 * Create a server (listen) socket.
 */
NSockPtr NSock::listen(const string &host, unsigned short port,
                       NSockOnConnectFunc connectFn) {
    /*
     * Get socket address and do socket(), and bind().
     */
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // stream socket
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          // Any protocol

    struct addrinfo *result, *rp;
    int err = getaddrinfo(host.empty() ? NULL : host.c_str(),
                          to_string(port).c_str(),
                          &hints,
                          &result);
    if (err) {
        stringstream ss;
        ss << "Failed getaddrinfo(): " << gai_strerror(err) << endl;
        throw runtime_error(ss.str());
    }

    int sfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        err = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (!err)
            break;

        ::close(sfd);
        sfd = -1;
    }

    if (rp == NULL) {
        stringstream ss;
        ss << "Failed bind(): " << errno << endl;
        throw runtime_error(ss.str());
    }

    freeaddrinfo(result);

    struct sockaddr_storage localAddr;
    socklen_t slen = sizeof (localAddr);
    err = getsockname(sfd, reinterpret_cast<struct sockaddr *>(&localAddr), &slen);
    if (err) {
        log("Failed getsockname(): %d\n", __FUNCTION__, errno);
        ::close(sfd);
        return nullptr;
    }

    // Do listen
    err = ::listen(sfd, 512);
    if (err) {
        log("Failed listen(): %d\n", __FUNCTION__, errno);
        ::close(sfd);
        return nullptr;
    }

    err = setnonblocking(sfd);
    if (err) {
        log("Failed setnonblocking(): %d\n", __FUNCTION__, errno);
        ::close(sfd);
        return nullptr;
    }

    assert(sfd != -1);
    auto listenSocket = make_shared<NSock>(sfd);
    listenSocket->isServer = true;
    listenSocket->localAddr = localAddr;
    listenSocket->onConnect = connectFn;

    // Add to poll
    PollFunc cb = [=](int fd, uint32_t revents) -> void {
        assert(fd == sfd);
        listenSocket->onAcceptCb(revents);
    };

    err = npollAddFd(sfd, EPOLLIN, cb);
    if (err) {
        log("%s: failed to add listen socket to poll\n", __FUNCTION__);
        return nullptr;
    }

    return listenSocket;
}


/*
 * Create a connected (client) socket.
 */
NSockPtr NSock::connect(std::string const &host, unsigned short port,
                        NSockOnRecvFunc recvFn,
                        NSockOnErrorFunc errorFn) {
    /*
     * Get socket address and do socket() and connect.
     */
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // stream socket
    hints.ai_flags = 0;              // For wildcard IP address
    hints.ai_protocol = 0;           // Any protocol

    struct addrinfo *result, *rp;
    int err = getaddrinfo(host.empty() ? NULL : host.c_str(),
                          to_string(port).c_str(),
                          &hints,
                          &result);
    if (err) {
        stringstream ss;
        ss << "Failed getaddrinfo(): " << gai_strerror(err) << endl;
        throw runtime_error(ss.str());
    }

    int sfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        // Do connect
        err = ::connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (err) {
            log("Failed connect(): %d\n", __FUNCTION__, errno);
            ::close(sfd);
            sfd = -1;
        } else {
            break;
        }
    }

    if (sfd == -1) {
        log("%s: Failed socket()|connect(): %s\n", __FUNCTION__, strerror(errno));
        return nullptr;
    }

    freeaddrinfo(result);

    struct sockaddr_storage localAddr;
    socklen_t slen = sizeof (localAddr);
    err = getsockname(sfd, reinterpret_cast<struct sockaddr *>(&localAddr), &slen);
    if (err) {
        log("Failed getsockname(): %d\n", __FUNCTION__, errno);
        ::close(sfd);
        return nullptr;
    }

    // Set to non-blocking
    err = setnonblocking(sfd);
    if (err) {
        log("Failed setnonblocking(): %d\n", __FUNCTION__, errno);
        ::close(sfd);
        return nullptr;
    }

    assert(sfd != -1);
    auto sock = make_shared<NSock>(sfd);
    sock->localAddr = localAddr;
    sock->onRecv = recvFn;
    sock->onError = errorFn;

    sock->monitorSocket();

    return sock;
}


/*
 * Send some data. Return the number of bytes queued. If the returned bytes is
 * less than bufLen, then the socket buffer is full, and the caller should wait
 * a bit before trying again.
 */
int NSock::send(const uint8_t *buf, int bufLen) {
    size_t len = sendBuf.put(buf, bufLen);

    writeToSocket();

    return len;
}


/*
 * Shutdown and close the socket
 */
void NSock::end() {
    if (sockfd == -1) {
        return;
    }

    log("%s: closing socket %lu(%d)\n", __FUNCTION__,
        getId(), sockfd);

    // remove from poll
    int err = npollRemoveFd(sockfd);
    if (err) {
        log("%s: failed npollRemoveFd\n", __FUNCTION__);
    }

    if (!isServer) {
        ::shutdown(sockfd, SHUT_WR);
    }

    ::close(sockfd);
    sockfd = -1;
}


/*
 * Server socket: A new incoming connection is here, create a new socket from it.
 */
void NSock::onAcceptCb(uint32_t revents) {
    assert(isServer);

    if ((EPOLLERR & revents) || !(EPOLLIN & revents)) {
        ++stat.sysErrorNr;
        return;
    }

    struct sockaddr_storage remAddr = {0};
    socklen_t remAddrLen = sizeof remAddr;
    int connfd = ::accept(sockfd, (struct sockaddr *)(&remAddr), &remAddrLen);
    if (connfd == -1) {
        ++stat.acceptErrorNr;
        log("%s: failed accept: %d\n", __FUNCTION__, errno);
        return;
    }

    struct sockaddr_storage localAddr;
    socklen_t slen = sizeof (localAddr);
    int err = getsockname(connfd, reinterpret_cast<struct sockaddr *>(&localAddr), &slen);
    if (err) {
        ++stat.sysErrorNr;
        log("Failed getsockname(): %d", __FUNCTION__, errno);
        ::close(connfd);
        return;
    }

    err = setnonblocking(connfd);
    if (err) {
        log("Failed setnonblocking(): %d", __FUNCTION__, errno);
        ::close(connfd);
        return;
    }

    /* Create a new socket and hand over ownership to caller */
    ++stat.acceptNr;
    auto connSock = make_shared<NSock>(connfd);
    connSock->localAddr = localAddr;
    connSock->monitorSocket();

    onConnect(connSock);
}


/*
 * Recieve data from the socket and invoke onRecv.
 */
void NSock::recvFromSocket() {
    if (!onRecv) {
        return;
    }

    // We must drain the socket receive buffer by reading until we encounter
    // EAGAIN. Otherwise, epoll_wait will not notify us about any remaining data
    // in the socket.
    while (true) {
        int recvdLen = ::recv(sockfd, recvBuf, sizeof(recvBuf), 0);

        if (recvdLen == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                //log("%s: recv not available: %d\n", __FUNCTION__, errno);
                return;
            }

            log("%s: recv fatal error: %d\n", __FUNCTION__, errno);
            ++stat.recvErrorNr;
            handleError();
            return;
        } else if (recvdLen == 0) {
            log("%s: peer closed socket: %d\n", __FUNCTION__, errno);
            handleError();
            return;
        }

        stat.recvBytes += recvdLen;

        onRecv(shared_from_this(), recvBuf, recvdLen);
    }
}

/*
 * Write data to the socket.
 */
void NSock::writeToSocket() {
    // We must drain the socket send buffer by writing until we encounter
    // EAGAIN. Otherwise, epoll_wait will not notify us about availabe writes.
    bool drained = false;
    while (true) {
        const uint8_t *buf;
        size_t bufLen;

        bufLen = sendBuf.get(&buf);
        if (!buf) {
            drained = true;
            break;
        }

        int sentLen = ::send(sockfd, buf, bufLen, 0);
        if (sentLen == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                handleError();
                return;
            }
        } else if (sentLen == 0) {
            log("%s: sent %d bytes\n", __FUNCTION__, sentLen);
            return;
        }

        stat.sendBytes += sentLen;
    }

    if (drained && onDrain) {
        onDrain(shared_from_this());
    }
}


/*
 * Handle socket errors.
 */
void NSock::handleError() {
    if (onError) {
        onError(shared_from_this(), errno);
    }
}


/*
 * Poll socket descriptor for read|write events.
 */
void NSock::monitorSocket() {
    // Add to poll
    auto self = shared_from_this();
    PollFunc cb = [=](int fd, uint32_t revents) -> void {
        assert(fd == sockfd);

        if ((EPOLLERR & revents)) {
            ++stat.sysErrorNr;
            handleError();
            return;
        }

        if (EPOLLIN & revents) {
            self->recvFromSocket();
        }

        if (EPOLLOUT & revents) {
            self->writeToSocket();
        }
    };

    int err = npollAddFd(sockfd, EPOLLET|EPOLLIN|EPOLLOUT, cb);
    if (err) {
        ++stat.sysErrorNr;
        handleError();
        return;
    }
}


}
