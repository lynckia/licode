/*global exports, require*/
'use strict';
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('RoomResource');

var currentService;
var currentRoom;

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (roomId, callback) {
    currentService = require('./../auth/nuveAuthenticator').service;

    serviceRegistry.getRoomForService(roomId, currentService, function (room) {
        currentRoom = room;
        callback();
    });
};

/*
 * Get Room. Represents a determined room.
 */
exports.represent = function (req, res) {
    doInit(req.params.room, function () {
        if (currentService === undefined) {
            res.send('Client unathorized', 401);
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
        } else {
            log.info('Representing room ', currentRoom._id, 'of service ', currentService._id);
            res.send(currentRoom);
        }
    });
};

/*
 * Update Room.
 */
exports.updateRoom = function (req, res) {
    doInit(req.params.room, function () {
        if (currentService === undefined) {
            res.send('Client unathorized', 401);
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
        } else if (req.body.name === undefined) {
            log.info('Invalid room');
            res.send('Invalid room', 400);
        } else {
            var id = '',
                array = currentService.rooms,
                index = -1,
                i;

            id += currentRoom._id;
            var room = currentRoom;

            room.name = req.body.name;
            var options = req.body.options || {};


            if (options.p2p) {
                room.p2p = true;
            }
            if (options.data) {
                room.data = options.data;
            }

            roomRegistry.updateRoom(id, room);

            for (i = 0; i < array.length; i += 1) {
                if (array[i]._id === currentRoom._id) {
                    index = i;
                }
            }
            if (index !== -1) {

                currentService.rooms[index] = room;
                serviceRegistry.updateService(currentService);
                log.info('Room ', id, ' updated for service ', currentService._id);

                res.send('Room Updated');
            }
        }
    });
};

/*
 * Patch Room.
 */
exports.patchRoom = function (req, res) {
    doInit(req.params.room, function () {
        if (currentService === undefined) {
            res.send('Client unathorized', 401);
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
        } else {
            var id = '',
                array = currentService.rooms,
                index = -1,
                i;

            id += currentRoom._id;
            var room = currentRoom;

            if (req.body.name) room.name = req.body.name;
            if (req.body.options.p2p) room.p2p = req.body.options.p2p;
            if (req.body.options.data) {
                for (var d in req.body.options.data) {
                    room.data[d] = req.body.options.data[d];
                }
            }

            roomRegistry.updateRoom(id, room);

            for (i = 0; i < array.length; i += 1) {
                if (array[i]._id === currentRoom._id) {
                    index = i;
                }
            }
            if (index !== -1) {

                currentService.rooms[index] = room;
                serviceRegistry.updateService(currentService);
                log.info('Room ', id, ' updated for service ', currentService._id);

                res.send('Room Updated');
            }
        }
    });
};


/*
 * Delete Room. Removes a determined room from the data base
 * and asks cloudHandler to remove it from erizoController.
 */
exports.deleteRoom = function (req, res) {
    doInit(req.params.room, function () {
        if (currentService === undefined) {
            res.send('Client unathorized', 401);
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
        } else {
            var id = '',
                array = currentService.rooms,
                index = -1,
                i;

            id += currentRoom._id;
            roomRegistry.removeRoom(id);

            for (i = 0; i < array.length; i += 1) {
                if (array[i]._id === currentRoom._id) {
                    index = i;
                }
            }
            if (index !== -1) {
                currentService.rooms.splice(index, 1);
                serviceRegistry.updateService(currentService);
                log.info('Room ', id, ' deleted for service ', currentService._id);
                cloudHandler.deleteRoom(id, function () {});
                res.send('Room deleted');
            }
        }
    });
};
