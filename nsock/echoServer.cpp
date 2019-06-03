#include <iostream>
#include <algorithm>

#include "nsock.h"
#include "npoll.h"
#include "util.h"
#include "echoServer.h"

using namespace std;
using namespace nsock;


ConnServerPtr ConnServer::createConnServer(std::string host, unsigned short port) {
    ConnServerPtr server = make_shared<ConnServer>(host, port);

    NSockOnConnectFunc connCb = [=] (NSockPtr sock) {
        server->onConnect(sock);
    };
    server->mListenSock = NSock::listen(host, port, connCb);

    server->serverLoop();

    return server;
}

ConnServer::ConnServer(std::string host, unsigned short port) :
    mHost(host), mPort(port) {
}

void ConnServer::serverLoop() {
    bool exitPollLoop = false;
    npollLoop(exitPollLoop);

    mConnections.clear();

    if (mListenSock) {
        mListenSock->end();
        mListenSock = nullptr;
    }
}


void ConnServer::onSocketRecv(NSockPtr sock, const uint8_t *buf, int recvLen) {
    log("%s: nsock recv: nsockId=%lu, buf=%p, recvLen=%d\n", __FUNCTION__,
        sock->getId(), buf, recvLen);

    sock->send(buf, recvLen);
}

void ConnServer::onSocketDrain(NSockPtr sock) {
    log("%s: nsock drained: nsockId=%lu\n", __FUNCTION__,
        sock->getId());
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
        self->onSocketRecv(sock, buf, bufLen);
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

    ConnServerPtr server = ConnServer::createConnServer();

    server->serverLoop();

    cout << "Good bye\n";
}
