#include <iostream>
#include <algorithm>

#include "nsock.h"
#include "npoll.h"
#include "util.h"
#include "commandServer.h"

using namespace std;
using namespace nsock;


void onClientSocketRecv(NSockPtr sock, const uint8_t *buf, int recvLen) {
    log("%s: nsock recv: nsockId=%lu, buf=%p, recvLen=%d\n", __FUNCTION__,
        sock->getId(), buf, recvLen);

    /* Write whatever we receive to stdout. To avoid messing up the
       terminal, only write ascii */
    write(STDOUT_FILENO, buf, recvLen);
}

void onClientSocketDrain(NSockPtr sock) {
    log("%s: nsock drained: nsockId=%lu\n", __FUNCTION__, sock->getId());
}

void onClientSocketError(NSockPtr sock, int error) {
    log("%s: nsock error: nsockId=%lu, error=%d\n", __FUNCTION__,
        sock->getId(), error);
    sock->end();
}

/* Send whatever we get from stdin to the echo server */
void onStdin(NSockPtr sock, const string &inp) {
    size_t n = sock->send((uint8_t *)inp.c_str(), inp.size());
    if (n < inp.size()) {
        log("%s: socket write buffer full. Message truncated.\n", __FUNCTION__);
    }
}

void echoLoop(const string &host, unsigned short port) {
    log("%s: connecting to %s:%d\n", __FUNCTION__, host.c_str(), port);

    bool exit = false;

    auto sock = NSock::connect(host, port,
                               onClientSocketRecv,
                               onClientSocketError);
    assert(sock);
    sock->setDrainFn(onClientSocketDrain);

    EofFunc eofCb = [&]() {
        log("%s: Got EoF, exiting...\n", __FUNCTION__);
        sock->end();
        exit = true;
    };

    RawInputFunc rawInpCb = [&] (const string &inp) {
        onStdin(sock, inp + '\n');
    };
    CommandServer cmdServer(eofCb, rawInpCb);
    cmdServer.monitorStdin();

    npoll::npollLoop(exit);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("%s: host port\n", argv[0]);
        return -1;
    }

    logSetPath("/tmp/nsock/echoClient");
    log("%s: starting...\n", argv[0]);

    string host = argv[1];
    unsigned short port = stoi(argv[2]);

    echoLoop(host, port);
}
