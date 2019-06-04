#ifndef _COMMAND_SERVER_H
#define _COMMAND_SERVER_H

#include <vector>
#include <map>
#include <string>
#include <functional>

#include <inttypes.h>
#include <unistd.h>

#include "npoll.h"
#include "util.h"


namespace nsock {

/*
 * Non-blocking command line (STDIN) processing. Use a request-response model:
 * read a command (request) from STDIN and write the response to STDOUT.
 */

typedef uint64_t REQID;

class NRequest {
public:
    static std::pair<bool, NRequest> parseInput(bool raw=false);

    REQID id = 0;
    const std::string cmd;
    const std::map<std::string, std::string> params;

    const std::string rawInput;

private:
    NRequest() :
        id(getNextReqId()) {
    }

    NRequest(const std::string &inp) :
        id(getNextReqId()), rawInput(inp) {
    }

    NRequest(const std::string &cmd, const std::map<std::string, std::string> &params) :
        id(getNextReqId()), cmd(cmd), params(params) {
    }

    static REQID sNextRequestId;
    static REQID getNextReqId() {
        return ++sNextRequestId;
    }
};

/*
 * Register commands to be notified of.
 */

typedef std::function<std::string (const NRequest &req)> NRequestEventFunc;
typedef std::function<void ()> EofFunc;
typedef std::function<void (const std::string &inp)> RawInputFunc;

class CommandServer {
public:
    CommandServer(EofFunc eofFn=nullptr, RawInputFunc rawInpFn=nullptr) :
        mOnEof(eofFn), mOnRawInput(rawInpFn) {
    }

    bool monitorStdin();
    void stopMonitoring();

    bool addCommand(const std::string cmdStr, NRequestEventFunc cmdFn);
    std::vector<std::string> getCommands() const;

private:
    void onStdinReadable(int fd, uint32_t revents);

    std::map<std::string, NRequestEventFunc> mDispatchTable;
    EofFunc mOnEof;
    RawInputFunc mOnRawInput;

    bool mIsEof = false;
    bool mMonitoring = false;
    int mStdin = STDIN_FILENO;
};

}

#endif
