/*global exports, require, console, Buffer*/
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var rpc = require('./../rpc/rpc');

var currentService;
var currentRoom;

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (roomId, callback) {
    "use strict";

    currentService = require('./../auth/nuveAuthenticator').service;

    serviceRegistry.getRoomForService(roomId, currentService, function (room) {
        currentRoom = room;
        callback();
    });

};

/*
 * Get Users. Represent a list of users of a determined room. This is consulted to cloudHandler using RabbitMQ RPC call.
 */
exports.getList = function (req, res) {
    "use strict";

    doInit(req.params.room, function () {

        if (currentService === undefined) {
            res.send('Service not found', 404);
            return;
        } else if (currentRoom === undefined) {
            console.log('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
            return;
        }

        console.log('Representing users for room ', currentRoom._id, 'and service', currentService._id);
        rpc.callRpc('cloudHandler', 'getUsersInRoom', currentRoom._id, function (users) {
            if (users === 'error') {
                res.send('CloudHandler does not respond', 401);
                return;
            }
            res.send(users);
        });

    });

};