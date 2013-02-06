/*global exports, require, console, Buffer*/
var roomRegistry = require('./../mdb/roomRegistry');
var tokenRegistry = require('./../mdb/tokenRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var dataBase = require('./../mdb/dataBase');
var crypto = require('crypto');
var cloudHandler = require('../cloudHandler');

var currentService;
var currentRoom;

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (roomId, callback) {
    "use strict";

    currentService = require('./../auth/nuveAuthenticator').service;

    serviceRegistry.getRoomForService(roomId, currentService, function (room) {
        //console.log(room);
        currentRoom = room;
        callback();
    });
};

var getTokenString = function (id, token) {
    "use strict";

    var toSign = id + ',' + token.host,
        hex = crypto.createHmac('sha1', dataBase.nuveKey).update(toSign).digest('hex'),
        signed = (new Buffer(hex)).toString('base64'),

        tokenJ = {tokenId: id, host: token.host, signature: signed},
        tokenS = (new Buffer(JSON.stringify(tokenJ))).toString('base64');

    return tokenS;
};

/*
 * Generates new token. 
 * The format of a token is:
 * {tokenId: id, host: erizoController host, signature: signature of the token};
 */
var generateToken = function (callback) {
    "use strict";

    var user = require('./../auth/nuveAuthenticator').user,
        role = require('./../auth/nuveAuthenticator').role,
        r,
        tr,
        token,
        tokenS;

    if (user === undefined || user === '') {
        callback(undefined);
        return;
    }

    token = {};
    token.userName = user;
    token.room = currentRoom._id;
    token.role = role;
    token.service = currentService._id;
    token.creationDate = new Date();

    r = currentRoom._id;
    tr = undefined;

    if (currentService.testRoom !== undefined) {
        tr = currentService.testRoom._id;
    }

    if (tr === r) {

        if (currentService.testToken === undefined) {
            token.use = 0;
            token.host = dataBase.testErizoController;

            console.log('Creating testToken');

            tokenRegistry.addToken(token, function (id) {

                token._id = id;
                currentService.testToken = token;
                serviceRegistry.updateService(currentService);

                tokenS = getTokenString(id, token);
                callback(tokenS);
                return;
            });

        } else {

            token = currentService.testToken;

            console.log('TestToken already exists, sending it', token);

            tokenS = getTokenString(token._id, token);
            callback(tokenS);
            return;

        }
    } else {

        cloudHandler.getErizoControllerForRoom (currentRoom._id, function (ec) {

            if (ec === 'timeout') {
                callback('error');
                return;
            }

            token.host = ec.ip + ':8080';

            tokenRegistry.addToken(token, function (id) {

                var tokenS = getTokenString(id, token);
                callback(tokenS);
            });
        });
    }
};

/*
 * Post Token. Creates a new token for a determined room of a service.
 */
exports.create = function (req, res) {
    "use strict";

    doInit(req.params.room, function () {

        if (currentService === undefined) {
            console.log('Service not found');
            res.send('Service not found', 404);
            return;
        } else if (currentRoom === undefined) {
            console.log('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
            return;
        }

        generateToken(function (tokenS) {

            if (tokenS === undefined) {
                res.send('Name and role?', 401);
                return;
            }
            if (tokenS === 'error') {
                res.send('CloudHandler does not respond', 401);
                return;
            }
            console.log('Created token for room ', currentRoom._id, 'and service ', currentService._id);
            res.send(tokenS);
        });
    });
};