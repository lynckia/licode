/*global exports, require, console*/
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger("RoomResource");

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
 * Get Room. Represents a determined room.
 */
exports.represent = function (req, res) {
    "use strict";

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
 * Delete Room. Removes a determined room from the data base and asks cloudHandler to remove ir from erizoController.
 */
exports.deleteRoom = function (req, res) {
    "use strict";

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
