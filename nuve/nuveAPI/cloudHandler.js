/*global require, setInterval, clearInterval, exports*/
'use strict';
var rpc = require('./rpc/rpc');
var config = require('./../../licode_config');
var logger = require('./logger').logger;

// Logger
var log = logger.getLogger('CloudHandler');

var AWS;

var INTERVAL_TIME_EC_READY = 500;
var TOTAL_ATTEMPTS_EC_READY = 20;
var INTERVAL_TIME_CHECK_KA = 1000;
var MAX_KA_COUNT = 10;

var erizoControllers = {};
var rooms = {}; // roomId: erizoControllerId
var ecQueue = [];
var idIndex = 0;
var getErizoController;

var recalculatePriority = function () {
    //*******************************************************************
    // States:
    //  0: Not available
    //  1: Warning
    //  2: Available
    //*******************************************************************

    var newEcQueue = [],
        noAvailable = [],
        warning = [],
        ec;

    for (ec in erizoControllers) {
        if (erizoControllers.hasOwnProperty(ec)) {
            if (erizoControllers[ec].state === 2) {
                newEcQueue.push(ec);
            }
        }
    }

    for (ec in erizoControllers) {
        if (erizoControllers.hasOwnProperty(ec)) {
            if (erizoControllers[ec].state === 1) {
                newEcQueue.push(ec);
                warning.push(ec);
            }
            if (erizoControllers[ec].state === 0) {
                noAvailable.push(ec);
            }
        }
    }

    ecQueue = newEcQueue;

    if (ecQueue.length === 0) {
        log.error('No erizoController is available.');
    }
    for (var w in warning) {
        log.warn('Erizo Controller in ', erizoControllers[w].ip,
                 'has reached the warning number of rooms');
    }

    for (var n in noAvailable) {
        log.warn('Erizo Controller in ', erizoControllers[n].ip,
                 'has reached the limit number of rooms');
    }
},

checkKA = function () {
    var ec, room;

    for (ec in erizoControllers) {
        if (erizoControllers.hasOwnProperty(ec)) {
            erizoControllers[ec].keepAlive += 1;
            if (erizoControllers[ec].keepAlive > MAX_KA_COUNT) {
                log.warn('ErizoController', ec, ' in ', erizoControllers[ec].ip,
                         'does not respond. Deleting it.');
                delete erizoControllers[ec];
                for (room in rooms) {
                    if (rooms.hasOwnProperty(room)) {
                        if (rooms[room] === ec) {
                            delete rooms[room];
                        }
                    }
                }
                recalculatePriority();
            }
        }
    }
};

setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

if (config.nuve.cloudHandlerPolicy) {
    getErizoController = require('./ch_policies/' +
                          config.nuve.cloudHandlerPolicy).getErizoController;
}

var addNewPrivateErizoController = function (ip, hostname, port, ssl, callback) {
    idIndex += 1;
    var id = idIndex,
        rpcID = 'erizoController_' + id;
    erizoControllers[id] = {
        ip: ip,
        rpcID: rpcID,
        state: 2,
        keepAlive: 0,
        hostname: hostname,
        port: port,
        ssl: ssl
    };
    log.info('New erizocontroller (', id, ') in: ', erizoControllers[id].ip);
    recalculatePriority();
    callback({id: id, publicIP: ip, hostname: hostname, port: port, ssl: ssl});
};

var addNewAmazonErizoController = function(privateIP, hostname, port, ssl, callback) {
    var publicIP;

    if (AWS === undefined) {
        AWS = require('aws-sdk');
    }
    log.info('private ip ', privateIP);
    new AWS.MetadataService({
        httpOptions: {
            timeout: 5000
        }
    }).request('/latest/meta-data/public-ipv4', function(err, data) {
        if (err) {
            log.info('Error: ', err);
            callback('error');
        } else {
            publicIP = data;
            log.info('public IP: ', publicIP);
            addNewPrivateErizoController(publicIP, hostname, port, ssl, callback);
        }
    });
};

exports.addNewErizoController = function (msg, callback) {
    if (msg.cloudProvider === '') {
        addNewPrivateErizoController(msg.ip, msg.hostname, msg.port, msg.ssl, callback);
    } else if (msg.cloudProvider === 'amazon') {
        addNewAmazonErizoController(msg.ip, msg.hostname, msg.port, msg.ssl, callback);
    }
};

exports.keepAlive = function (id, callback) {
    var result;

    if (erizoControllers[id] === undefined) {
        result = 'whoareyou';
        log.warn('I received a keepAlive message from an unknown erizoController');
    } else {
        erizoControllers[id].keepAlive = 0;
        result = 'ok';
        //log.info('KA: ', id);
    }
    callback(result);
};

exports.setInfo = function (params) {
    log.info('Received info ', params, '. Recalculating erizoControllers priority');
    erizoControllers[params.id].state = params.state;
    recalculatePriority();
};

exports.killMe = function (ip) {
    log.info('[CLOUD HANDLER]: ErizoController in host ', ip, 'wants to be killed.');
};

exports.getErizoControllerForRoom = function (room, callback) {
    var roomId = room._id;

    if (rooms[roomId] !== undefined) {
        callback(erizoControllers[rooms[roomId]]);
        return;
    }

    var id,
        attempts = 0,
        intervarId = setInterval(function () {

        if (getErizoController) {
            id = getErizoController(room, erizoControllers, ecQueue);
        } else {
            id = ecQueue[0];
        }

        if (id !== undefined) {

            rooms[roomId] = id;
            callback(erizoControllers[id]);

            recalculatePriority();
            clearInterval(intervarId);
        }

        if (attempts > TOTAL_ATTEMPTS_EC_READY) {
            clearInterval(intervarId);
            callback('timeout');
        }
        attempts++;

    }, INTERVAL_TIME_EC_READY);

};

exports.getUsersInRoom = function (roomId, callback) {
    if (rooms[roomId] === undefined) {
        callback([]);
        return;
    }

    var rpcID = erizoControllers[rooms[roomId]].rpcID;
    rpc.callRpc(rpcID, 'getUsersInRoom', [roomId], {'callback': function (users) {
        if (users === 'timeout') {
            users = '?';
        }
        callback(users);
    }});
};

exports.deleteRoom = function (roomId, callback) {
    if (rooms[roomId] === undefined) {
        callback('Success');
        return;
    }

    var rpcID = erizoControllers[rooms[roomId]].rpcID;
    rpc.callRpc(rpcID, 'deleteRoom', [roomId], {'callback': function (result) {
        callback(result);
    }});
};

exports.deleteUser = function (user, roomId, callback) {
    var rpcID = erizoControllers[rooms[roomId]].rpcID;
    rpc.callRpc(rpcID, 'deleteUser', [{user: user, roomId:roomId}], {'callback': function (result) {
        callback(result);
    }});
};
