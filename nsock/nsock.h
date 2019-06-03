#ifndef _NSOCK_H
#define _NSOCK_H

#include <string>
#include <memory>
#include <functional>
#include <algorithm>
#include <queue>

#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>


namespace nsock {

enum NSockState {
    NSockInit = 0,
    NSockConnecting,
    NSockConnected,
    NSockClosing,
    NSockClosed
};

class NSock;
typedef std::shared_ptr<NSock> NSockPtr;
typedef std::function<void (NSockPtr sock, const uint8_t *buf, int recvLen)> NSockOnRecvFunc;
typedef std::function<void (NSockPtr sock)> NSockOnDrainFunc;
typedef std::function<void (NSockPtr sock, int error)> NSockOnErrorFunc;
typedef std::function<void (NSockPtr sock)> NSockOnConnectFunc;

struct SockStat {
    uint64_t acceptNr = 0;

    // errors
    uint64_t sysErrorNr = 0;
    uint64_t listenErrorNr = 0;
    uint64_t acceptErrorNr = 0;

    uint64_t recvErrorNr = 0;
    uint64_t sendErrorNr = 0;
};

/*
 * A circular buffer.
 */
struct CircularBuffer {
    CircularBuffer() {
    }

    bool full() const {
        return dataLen == dataBufSize;
    }

    bool empty() const {
        return dataLen == 0;
    }

    size_t put(const uint8_t *buf, size_t bufLen) {

        size_t freeLen = dataBufSize - dataLen;
        size_t wlen = std::min(freeLen, bufLen);
        size_t offset = 0;

        while (wlen) {
            assert(dataLen <= dataBufSize);

            size_t freeStart = (dataStart + dataLen) % dataBufSize;
            size_t len = std::min(wlen, dataBufSize - freeStart);
            memcpy(dataBuf + freeStart, buf + offset, len);
            dataLen += len;
            offset += len;
            wlen -= len;
        }

        return wlen;
    }

    size_t get(uint8_t const **bufPtr) {
        const size_t dataBufSize = sizeof(dataBuf);

        if (dataLen == 0) {
            *bufPtr = nullptr;
            return 0;
        }

        *bufPtr = dataBuf + dataStart;
        size_t rlen = std::min(dataLen, dataBufSize - dataStart);
        dataStart = (dataStart + rlen) % dataBufSize;
        dataLen -= rlen;
        return rlen;
    }

    uint8_t dataBuf[8192];
    const size_t dataBufSize = sizeof(dataBuf);
    size_t dataStart = 0;
    size_t dataLen = 0;
};


typedef uint64_t NSOCKID;

class NSock : public std::enable_shared_from_this<NSock> {
public:
    /*
     * Create a client socket by connecting to a server.
     */
    static NSockPtr connect(std::string const &host, unsigned short port,
                            NSockOnRecvFunc recvFn,
                            NSockOnErrorFunc errorFn);

    /*
     * Create a server socket by listening on a network interface.
     */
    static NSockPtr listen(const std::string &host, unsigned short port,
                           NSockOnConnectFunc connectFn);

    /* Get socket state */
    enum NSockState getState() const {
        return state;
    }

    /* Get socket ID */
    NSOCKID getId() const {
        return id;
    }

    /* Set the OnRecv callback */
    void setRecvFn(NSockOnRecvFunc recvFn) {
        onRecv = recvFn;
        recvFromSocket();
    }

    /* Set the OnError callback */
    void setErrorFn(NSockOnErrorFunc errorFn) {
        onError = errorFn;
    }

    /* Set onDrain callback */
    void setDrainFn(NSockOnDrainFunc drainFn) {
        onDrain = drainFn;
    }

    /* Shutdown and close the socket gracefully */
    void end();

    /* Send some data */
    int send(const uint8_t *buf, int bufLen);

    /* Constructor - don't call directly, use listen(), or connect() */
    NSock(int sfd=-1);

    /* Destructor */
    ~NSock();

private:
    /* Server socket only: the callback on new connection */
    void onAcceptCb(uint32_t revents);

    /* Low level socket read|write */
    void recvFromSocket();
    void writeToSocket();

    /* Handle errors */
    void handleError();

    /* Monitor socket for read|write events */
    void monitorSocket();

    static NSOCKID sNSockId;
    static NSOCKID getNextNSockId() {
        return ++sNSockId;
    }

    NSOCKID id = 0;
    bool isServer = false;
    int sockfd = -1;
    enum NSockState state = NSockInit;
    struct sockaddr_storage localAddr = {0};
    struct sockaddr_storage remoteAddr = {0};

    // Callbacks
    NSockOnConnectFunc onConnect; // server socket only
    NSockOnErrorFunc onError;
    NSockOnRecvFunc onRecv;
    NSockOnDrainFunc onDrain;

    // Recv buffer
    uint8_t recvBuf[8192];

    // Send buffer
    CircularBuffer sendBuf;

    // Stats
    SockStat stat;
};

}

#endif
