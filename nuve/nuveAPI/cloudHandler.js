/*global require, setInterval, clearInterval, exports*/
'use strict';
var rpc = require('./rpc/rpc');
var config = require('config');
var logger = require('./logger').logger;
var erizoControllerRegistry = require('./mdb/erizoControllerRegistry');
var roomRegistry = require('./mdb/roomRegistry');

// Logger
var log = logger.getLogger('CloudHandler');

var AWS;

var INTERVAL_TIME_EC_READY = 500;
var TOTAL_ATTEMPTS_EC_READY = 20;
var INTERVAL_TIME_CHECK_KA = 1000;
var KA_TIMEOUT_MS = 10*1000;

var getErizoController;

var getEcQueue = function (callback) {
    //*******************************************************************
    // States:
    //  0: Not available
    //  1: Warning
    //  2: Available
    //*******************************************************************

    erizoControllerRegistry.getErizoControllers(function(erizoControllers) {
        var ecQueue = [],
            noAvailable = [],
            warning = [],
            ec;

        for (ec in erizoControllers) {
            if (erizoControllers.hasOwnProperty(ec)) {
                var erizoController = erizoControllers[ec];
                if (erizoController.state === 2) {
                    ecQueue.push(erizoController);
                }
                if (erizoController.state === 1) {
                    warning.push(erizoController);
                }
                if (erizoController.state === 0) {
                    noAvailable.push(erizoController);
                }
            }
        }

        ecQueue = ecQueue.concat(warning);

        if (ecQueue.length === 0) {
            log.error('No erizoController is available.');
        }
        ecQueue = ecQueue.concat(noAvailable);
        for (var w in warning) {
            log.warn('Erizo Controller in ', erizoControllers[w].ip,
                     'has reached the warning number of rooms');
        }

        for (var n in noAvailable) {
            log.warn('Erizo Controller in ', erizoControllers[n].ip,
                     'has reached the limit number of rooms');
        }
        callback(ecQueue);
    });
};

var assignErizoController = function (erizoControllerId, room, callback) {
    roomRegistry.assignErizoControllerToRoom(room, erizoControllerId, callback);
};

var unassignErizoController = function (erizoControllerId) {
    roomRegistry.getRooms(function(rooms) {
        for (var room in rooms) {
            if (rooms.hasOwnProperty(room)) {
                if (rooms[room].erizoControllerId &&
                    rooms[room].erizoControllerId.equals(erizoControllerId)) {
                    rooms[room].erizoControllerId = undefined;
                    roomRegistry.updateRoom(rooms[room]._id, rooms[room]);
                }
            }
        }
    });
};

var checkKA = function () {
    var ec;

    erizoControllerRegistry.getTimedOutErizoControllers(KA_TIMEOUT_MS, function(erizoControllers) {

        for (ec in erizoControllers) {
            if (erizoControllers.hasOwnProperty(ec)) {
                var id = erizoControllers[ec]._id;
                log.warn('ErizoController', ec, ' in ', erizoControllers[ec].ip,
                         'does not respond. Deleting it.');
                erizoControllerRegistry.removeErizoController(id);
                unassignErizoController(id);
                return;
            }
        }
    });
};

setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

var CLOUD_HANDLER_POLICY = config.get('nuve.cloudHandlerPolicy');
if (CLOUD_HANDLER_POLICY) {
    getErizoController = require('./ch_policies/' + CLOUD_HANDLER_POLICY).getErizoController;
}

var addNewPrivateErizoController = function (ip, hostname, port, ssl, callback) {

    var erizoController = {
        ip: ip,
        state: 2,
        keepAliveTs: new Date(),
        hostname: hostname,
        port: port,
        ssl: ssl
    };
    erizoControllerRegistry.addErizoController(erizoController, function (erizoController) {
        var id = erizoController._id;
        log.info('New erizocontroller (', id, ') in: ', erizoController.ip);
        callback({id: id, publicIP: ip, hostname: hostname, port: port, ssl: ssl});
    });
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

    erizoControllerRegistry.getErizoController(id, function (erizoController) {

        if (erizoController) {
          erizoControllerRegistry.updateErizoController(id, {keepAliveTs: new Date()});
          result = 'ok';
          //log.info('KA: ', id);
        } else {
          result = 'whoareyou';
          log.warn('I received a keepAlive message from an unknown erizoController');
        }
        callback(result);
    });
};

exports.setInfo = function (params) {
    log.info('Received info ', params, '. Recalculating erizoControllers priority');
    erizoControllerRegistry.updateErizoController(params.id, {state: params.state});
};

exports.killMe = function (ip) {
    log.info('[CLOUD HANDLER]: ErizoController in host ', ip, 'wants to be killed.');
};

var getErizoControllerForRoom = exports.getErizoControllerForRoom = function (room, callback) {
    var roomId = room._id;

    roomRegistry.getRoom(roomId, function (room) {
        var id = room.erizoControllerId;
        if (id) {
            erizoControllerRegistry.getErizoController(id, function (erizoController) {
                if (erizoController) {
                    callback(erizoController);
                } else {
                    room.erizoControllerId = undefined;
                    roomRegistry.updateRoom(room._id, room);
                    getErizoControllerForRoom(room, callback);
                }
            });
            return;
        }

        var attempts = 0;
        var intervalId;

        getEcQueue(function(ecQueue) {
            intervalId = setInterval(function () {
                var erizoController;
                if (getErizoController) {
                    erizoController = getErizoController(room, ecQueue);
                } else {
                    erizoController = ecQueue[0];
                }
                var id = erizoController ? erizoController._id : undefined;

                if (id !== undefined) {

                    assignErizoController(id, room, function (erizoController) {
                        callback(erizoController);
                        clearInterval(intervalId);
                    });

                }

                if (attempts > TOTAL_ATTEMPTS_EC_READY) {
                    clearInterval(intervalId);
                    callback('timeout');
                }
                attempts++;

            }, INTERVAL_TIME_EC_READY);
        });
    });
};

exports.getUsersInRoom = function (roomId, callback) {
    roomRegistry.getRoom(roomId, function (room) {
        if (room && room.erizoControllerId) {
            var rpcID = 'erizoController_' + room.erizoControllerId;
            rpc.callRpc(rpcID, 'getUsersInRoom', [roomId], {'callback': function (users) {
                callback(users);
            }});

        } else {
            callback([]);
        }
    });

};

exports.deleteRoom = function (roomId, callback) {
    roomRegistry.getRoom(roomId, function (room) {
        if (room && room.erizoControllerId) {
            var rpcID = 'erizoController_' + room.erizoControllerId;
            rpc.callRpc(rpcID, 'deleteRoom', [roomId], {'callback': function (result) {
                callback(result);
            }});
        } else {
            callback('Success');
        }
    });
};

exports.deleteUser = function (user, roomId, callback) {
    roomRegistry.getRoom(roomId, function (room) {
        if (room && room.erizoControllerId) {
            var rpcID = 'erizoController_' + room.erizoControllerId;
            rpc.callRpc(rpcID,
                        'deleteUser',
                        [{user: user, roomId:roomId}],
                        {'callback': function (result) {
                            callback(result);
                        }});
        } else {
            callback('Room does not exist or the user is not connected');
        }
    });
};
