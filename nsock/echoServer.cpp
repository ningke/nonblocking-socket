#include <iostream>
#include <algorithm>

#include "nsock.h"
#include "npoll.h"
#include "util.h"
#include "echoServer.h"
#include "commandServer.h"


using namespace std;
using namespace nsock;
using namespace npoll;


ConnServer::ConnServer(std::string host, unsigned short port) :
    mHost(host), mPort(port) {
}

ConnServer::~ConnServer() {
}


ConnServerPtr ConnServer::createConnServer(std::string host, unsigned short port) {
    ConnServerPtr server = make_shared<ConnServer>(host, port);

    NSockOnConnectFunc connCb = [=] (NSockPtr sock) {
        server->onConnect(sock);
    };
    server->mListenSock = NSock::listen(host, port, connCb);
    printf("ConnServer now listening on %s:%d\n", host.c_str(), port);

    server->serverLoop();

    return server;
}

string ConnServer::getConnStats() const {
    stringstream ss;

    auto stat = mListenSock->getStats();
    ss << "{\n";
    ss << "listenSocket: " << stat.toString() << ",\n";
    ss << "connections: [";
    bool first = true;
    for (auto &conn : mConnections) {
        auto connStat = conn->getStats();
        if (!first) {
            ss << ",\n";
        } else {
            first = false;
        }
        ss << connStat.toString();
    }
    ss << "]\n";
    ss << "}\n";

    return ss.str();
}

void ConnServer::serverLoop() {
    bool exit = false;

    EofFunc eofCb = [&]() {
        log("%s: Got EoF, exiting...\n", __FUNCTION__);
        exit = true;
    };

    CommandServer cmdServer(eofCb);
    auto self = shared_from_this();
    auto getStatsCb = [=] (const NRequest &req) {
        return self->getConnStats();
    };

    cmdServer.addCommand("get-stats", getStatsCb);

    cmdServer.monitorStdin();

    npollLoop(exit);
    mConnections.clear();

    if (mListenSock) {
        mListenSock->end();
        mListenSock = nullptr;
    }
}


size_t ConnServer::onSocketRecv(NSockPtr sock, const uint8_t *buf, int recvLen) {
    log("%s: nsock recv: nsockId=%lu, buf=%p, recvLen=%d\n", __FUNCTION__,
        sock->getId(), buf, recvLen);

    int n = sock->send(buf, recvLen);
    if (n < 0) {
        log("%s: socket send error: %d\n", __FUNCTION__, n);
        sock->end();
        return 0;
    }

    if (n < recvLen) {
        log("%s: socket send buffer full, pausing receive\n", __FUNCTION__, n);
        sock->setRecvFn(nullptr);

        assert(!mRecvPaused);
        mRecvPaused = true;
    }

    return n;
}

void ConnServer::onSocketDrain(NSockPtr sock) {
    log("%s: nsock drained: nsockId=%lu\n", __FUNCTION__,
        sock->getId());

    // Rearm receive if needed
    if (mRecvPaused) {
        auto self = shared_from_this();
        NSockOnRecvFunc recvCb = [=] (NSockPtr sock, const uint8_t *buf, int bufLen) {
            return self->onSocketRecv(sock, buf, bufLen);
        };

        mRecvPaused = false;
        sock->setRecvFn(recvCb);
    }
}

void ConnServer::onSocketError(NSockPtr sock, int error) {
    log("%s: nsock error: nsockId=%lu, error=%d\n", __FUNCTION__,
        sock->getId(), error);
    sock->end();
}

void ConnServer::onConnect(NSockPtr sock) {
    log("%s: client nsock %lu connected\n", __FUNCTION__, sock->getId());
    mConnections.insert(sock);

    auto self = shared_from_this();
    NSockOnRecvFunc recvCb = [=] (NSockPtr sock, const uint8_t *buf, int bufLen) {
        return self->onSocketRecv(sock, buf, bufLen);
    };
    sock->setRecvFn(recvCb);

    NSockOnErrorFunc errorCb = [=] (NSockPtr sock, int error) {
        self->mConnections.erase(sock);
        self->onSocketError(sock, error);
    };
    sock->setErrorFn(errorCb);

    NSockOnDrainFunc drainCb = [=] (NSockPtr sock) {
        self->onSocketDrain(sock);
    };
    sock->setDrainFn(drainCb);
}


int main(int argc, char *argv[]) {
    // TODO getopt

    logSetPath("/tmp/nsock/echoServer");

    log("%s: starting...\n", argv[0]);

    ConnServerPtr server = ConnServer::createConnServer();

    server->serverLoop();

    cout << "Good bye\n";
}
