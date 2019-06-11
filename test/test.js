const net = require('net');
const _ = require('underscore');

"use strict";

const echoServerHost = 'localhost';
const echoServerPort = 12121;

function startConnections(opts) {
    const calcStats = function () {
        let sum = 0, noRespNr = 0;
        console.log(`Connection       respTimeMs`);
        connStats.forEach ((obj, idx) => {
            if (obj.respTimeMs === -1) {
                ++noRespNr;
            } else {
                sum += obj.respTimeMs;
            }
            console.log(`${idx+1}      ${obj.respTimeMs}`);
        });
        const avg = sum / connStats.length;

        console.log(`Connections: ${opts.connNr}, Average response time: ${avg}, No response conns: ${noRespNr}`);
    };

    const statObj = {
        respTimeMs: -1,
    };

    let connStats = Array.from({length: opts.connNr}, (v) => {
        return JSON.parse(JSON.stringify(statObj));
    });

    let doneNr = 0;
    _.times(opts.connNr, (idx) => {
        connectAndSend(idx + 1, (respTime) => {
            connStats[idx].respTimeMs = respTime;
            ++doneNr;
            if (doneNr === opts.connNr) {
                calcStats();
            }
        });
    });
}


function connectAndSend(clientIdx, doneCb) {
    const msgLen = 32;
    const message = '0'.repeat(msgLen) + `${clientIdx}`.slice(-msgLen);

    const before = Date.now();
    let sock = net.createConnection(echoServerPort, echoServerHost, () => {
        console.log(`Client ${clientIdx} connected.`);

        sock.write(message);
    });

    let recvMsg = '';
    sock.on('data', (data) => {
        recvMsg += data;
        if (!message.startsWith(recvMsg)) {
            console.error(`Connection ${clientIdx}: Mismatched echo response!!!!!`);
        } else if (message.length === recvMsg.length) {
            sock.end();
            doneCb(Date.now() - before);
        }
    });

    sock.on('error', (err) => {
        console.log(`Connection ${clientIdx} got error ${err}`);
    });

    sock.on('close', () => {
        console.log(`Connection ${clientIdx} closed`);
    });
}


(function () {
    var argv = require('minimist')(process.argv.slice(2));

    const opts = {
        connNr: 10
    };

    if (argv.connNr) {
        opts.connNr = argv.connNr;
    }

    startConnections(opts);

})();
