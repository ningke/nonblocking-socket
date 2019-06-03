#include <iostream>
#include <algorithm>

#include "nsock.h"
#include "npoll.h"
#include "util.h"
#include "echoServer.h"

using namespace std;
using namespace nsock;


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("%s: host port\n", argv[0]);
    }

    string host = argv[1];
    unsigned short port = stoi(argv[2]);


}
