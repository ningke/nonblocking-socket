#ifndef _ECHO_SERVER_H
#define _ECHO_SERVER_H

#include <vector>
#include <memory>
#include <map>
#include <set>
#include <string>

#include "nsock.h"
#include "npoll.h"
#include "util.h"


class ConnServer;
typedef std::shared_ptr<ConnServer> ConnServerPtr;

class ConnServer : public std::enable_shared_from_this<ConnServer> {
public:
    ConnServer(std::string host, unsigned short port);
    static ConnServerPtr createConnServer(std::string host="localhost",
                                          unsigned short port=12121);
    void serverLoop();
    std::string getConnStats() const;

private:

    /* The socket callbacks */
    void onConnect(nsock::NSockPtr sock);
    void onSocketError(nsock::NSockPtr sock, int error);
    void onSocketDrain(nsock::NSockPtr sock);
    void onSocketRecv(nsock::NSockPtr sock, const uint8_t *buf, int recvLen);

    std::string mHost;
    unsigned short mPort = 0;
    nsock::NSockPtr mListenSock;
    std::set<nsock::NSockPtr> mConnections;
};


#endif
