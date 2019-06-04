#include <iostream>
#include <string>
#include <sstream>

#include "unistd.h"

#include "commandServer.h"


using namespace std;

namespace nsock {


REQID NRequest::sNextRequestId = 0;

/*
 * Read a line from stdin, parse the input and return an NRequest object.
 */
pair<bool, NRequest> NRequest::parseInput(bool raw) {
    string cmdLine;
    const NRequest emptyReq;

    if (!getline(cin, cmdLine)) {
        return {false, emptyReq};
    }

    if (raw) {
        return {cin.good(), NRequest(cmdLine)};
    }

    stringstream ss(cmdLine);

    string cmd;
    if (!getline(ss, cmd, ' ')) {
        return {false, emptyReq};
    }

    string kvpair;
    map<string, string> params;
    while (getline(ss, kvpair, ' ')) {
        stringstream kvss(kvpair);
        string key, val;
        if (!getline(kvss, key, '=')) {
            break;
        }
        getline(kvss, val, '=');
        if (kvss.bad()) {
            break;
        }

        params.insert({key, val});
    }

    return {true, NRequest(cmd, params)};
}


bool CommandServer::monitorStdin() {
    if (mIsEof) {
        return false;
    } else if (mMonitoring) {
        return true;
    }

    mMonitoring = true;

    npoll::PollFunc cb = [=] (int fd, uint32_t revents) {
        return this->onStdinReadable(fd, revents);
    };

    return (0 == npoll::npollAddFd(mStdin, EPOLLIN, cb));
}


bool CommandServer::addCommand(const std::string cmdStr, NRequestEventFunc cmdFn) {
    auto it_res = mDispatchTable.insert({cmdStr, cmdFn});

    return it_res.second;
}

vector<string> CommandServer::getCommands() const {
    vector<string> cmds;

    transform(begin(mDispatchTable), end(mDispatchTable),
              back_inserter(cmds),
              [](const auto &kvp) {
                  return kvp.first;
              });

    return cmds;
}

void CommandServer::stopMonitoring() {
    mIsEof = true;
    npoll::npollRemoveFd(mStdin);

    if (mOnEof) {
        mOnEof();
    }
}

void CommandServer::onStdinReadable(int fd, uint32_t revents) {
    log("%s: fd=%d, revents=%08x\n", __FUNCTION__, fd, revents);
    if (revents & EPOLLERR) {
        stopMonitoring();
        return;
    }

    /* Empty command table - call back with the raw input */
    bool getRawInput = mDispatchTable.empty();
    auto [res, req] = NRequest::parseInput(getRawInput);
    if (!res) {
        stopMonitoring();
        return;
    }

    if (getRawInput && mOnRawInput) {
        mOnRawInput(req.rawInput);
        return;
    }

    auto it = mDispatchTable.find(req.cmd);
    if (it == mDispatchTable.end()) {
        log("%s: Unknown comamnd: [%s]\n", __FUNCTION__, req.cmd.c_str());
        return;
    }

    // Process command and output the response.
    NRequestEventFunc cmdFn = it->second;
    auto resp = cmdFn(req);
    if (!resp.empty()) {
        cout << resp << endl;
    }
}

}
