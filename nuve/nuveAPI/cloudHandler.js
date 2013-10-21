/*global require, console, setInterval, clearInterval, exports*/
var rpc = require('./rpc/rpc');
var config = require('./../../licode_config');
var ec2;

var INTERVAL_TIME_EC_READY = 100;
var INTERVAL_TIME_CHECK_KA = 1000;
var MAX_KA_COUNT = 10;

var erizoControllers = {};
var rooms = {}; // roomId: erizoControllerId
var ecQueue = [];
var idIndex = 0;

var recalculatePriority = function () {
    "use strict";

    //*******************************************************************
    // States: 
    //  0: Not available
    //  1: Warning
    //  2: Available 
    //*******************************************************************

    var newEcQueue = [],
        available = 0,
        warnings = 0,
        ec;

    for (ec in erizoControllers) {
        if (erizoControllers.hasOwnProperty(ec)) {
            if (erizoControllers[ec].state === 2) {
                newEcQueue.push(ec);
                available += 1;
            }
        }
    }

    for (ec in erizoControllers) {
        if (erizoControllers.hasOwnProperty(ec)) {
            if (erizoControllers[ec].state === 1) {
                newEcQueue.push(ec);
                warnings += 1;
            }
        }
    }

    ecQueue = newEcQueue;

    if (ecQueue.length === 0 || (available === 0 && warnings < 2)) {
        console.log('[CLOUD HANDLER]: Warning! No erizoController is available.');
    }

},

    checkKA = function () {
        "use strict";
        var ec, room;

        for (ec in erizoControllers) {
            if (erizoControllers.hasOwnProperty(ec)) {
                erizoControllers[ec].keepAlive += 1;
                if (erizoControllers[ec].keepAlive > MAX_KA_COUNT) {
                    console.log('ErizoController', ec, ' in ', erizoControllers[ec].ip, 'does not respond. Deleting it.');
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
    },

    checkKAInterval = setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

exports.addNewErizoController = function (msg, callback) {
    "use strict";

    if (msg.cloudProvider === '') {
        addNewPrivateErizoController(msg.ip, callback);
    } else if (msg.cloudProvider === 'amazon') {
        addNewAmazonErizoController(msg.ip, callback);
    }
    
};

var addNewAmazonErizoController = function(privateIP, callback) {
    
    var publicIP;
    var instaceId;

    if (ec2 === undefined) {
        var opt = {version: '2012-12-01'};
        if (config.cloudProvider.host !== '') {
            opt.host = config.cloudProvider.host;
        }
        ec2 = require('aws-lib').createEC2Client(config.cloudProvider.accessKey, config.cloudProvider.secretAccessKey, opt);
    }
    console.log('private ip ', privateIP);

    ec2.call('DescribeInstances', {'Filter.1.Name':'private-ip-address', 'Filter.1.Value':privateIP}, function (err, response) {

        if (err) {
            console.log('Error: ', err);
            callback('error');
        } else if (response) {
            publicIP = response.reservationSet.item.instancesSet.item.ipAddress;
            console.log('public IP: ', publicIP);
            addNewPrivateErizoController(publicIP, callback);
        }
    });
}

var addNewPrivateErizoController = function (ip, callback) {
    "use strict";
    idIndex += 1;
    var id = idIndex,
        rpcID = 'erizoController_' + id;
    erizoControllers[id] = {ip: ip, rpcID: rpcID, state: 2, keepAlive: 0};
    console.log('New erizocontroller (', id, ') in: ', erizoControllers[id].ip);
    recalculatePriority();
    callback({id: id, publicIP: ip});
};

exports.keepAlive = function (id, callback) {
    "use strict";
    var result;

    if (erizoControllers[id] === undefined) {
        result = 'whoareyou';
        console.log('I received a keepAlive mess from a removed erizoController');
    } else {
        erizoControllers[id].keepAlive = 0;
        result = 'ok';
        //console.log('KA: ', id);
    }
    callback(result);
};

exports.setInfo = function (params) {
    "use strict";

    console.log('Received info ', params,    '.Recalculating erizoControllers priority');
    erizoControllers[params.id].state = params.state;
    recalculatePriority();
};

exports.killMe = function (ip) {
    "use strict";

    console.log('[CLOUD HANDLER]: ErizoController in host ', ip, 'does not respond.');

};

exports.getErizoControllerForRoom = function (roomId, callback) {
    "use strict";

    if (rooms[roomId] !== undefined) {
        callback(erizoControllers[rooms[roomId]]);
        return;
    }

    var id,
        intervarId = setInterval(function () {

            id = ecQueue[0];

            if (id !== undefined) {

                rooms[roomId] = id;
                callback(erizoControllers[id]);

                recalculatePriority();
                clearInterval(intervarId);
            }

        }, INTERVAL_TIME_EC_READY);

};

exports.getUsersInRoom = function (roomId, callback) {
    "use strict";

    if (rooms[roomId] === undefined) {
        callback([]);
        return;
    }

    var rpcID = erizoControllers[rooms[roomId]].rpcID;
    rpc.callRpc(rpcID, 'getUsersInRoom', roomId, function (users) {
        if (users === 'timeout') {
            users = '?';
        }
        callback(users);
    });
};

exports.deleteRoom = function (roomId, callback) {
    "use strict";

    if (rooms[roomId] === undefined) {
        callback('Success');
        return;
    }

    var rpcID = erizoControllers[rooms[roomId]].rpcID;
    rpc.callRpc(rpcID, 'deleteRoom', roomId, function (result) {
        callback(result);
    });
};
