/*global exports, require*/
'use strict';
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRoomRegistry = require('./../mdb/serviceRoomRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('RoomResource');

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (req, callback) {
    serviceRoomRegistry.getRoomForService(req.params.room, req.service, function (room) {
        callback(room);
    });
};

/*
 * Get Room. Represents a determined room.
 */
exports.represent = function (req, res) {
    doInit(req, function (currentRoom) {
        if (req.service === undefined) {
            res.status(401).send('Client unathorized');
        } else if (currentRoom === undefined) {
            log.info('message: representRoom - room does not exits, roomId: ' + req.params.room);
            res.status(404).send('Room does not exist');
        } else {
            log.info('message: representRoom success, roomId: ' + currentRoom._id +
                ', serviceId: ' + req.service._id);
            res.send(currentRoom);
        }
    });
};

/*
 * Update Room.
 */
exports.updateRoom = function (req, res) {
    doInit(req, function (currentRoom) {
        if (req.service === undefined) {
            res.status(401).send('Client unathorized');
        } else if (currentRoom === undefined) {
            log.info('message: updateRoom - room does not exist + roomId: ' + req.params.room);
            res.status(404).send('Room does not exist');
        } else if (req.body.name === undefined) {
            log.info('message: updateRoom - Invalid room');
            res.status(400).send('Invalid room');
        } else {
            var id = '' + currentRoom._id;
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
        }
    });
};

/*
 * Patch Room.
 */
exports.patchRoom = function (req, res) {
    doInit(req, function (currentRoom) {
        if (req.service === undefined) {
            res.status(401).send('Client unathorized');
        } else if (currentRoom === undefined) {
            log.info('message: pachRoom - room does not exist, roomId : ' + req.params.room);
            res.status(404).send('Room does not exist');
        } else {
            var id = '' + currentRoom._id;
            var room = currentRoom;

            if (req.body.name) room.name = req.body.name;
            if (req.body.options) {
              if (req.body.options.p2p) room.p2p = req.body.options.p2p;
              if (req.body.options.data) {
                  for (var d in req.body.options.data) {
                      room.data[d] = req.body.options.data[d];
                  }
              }
            }

            roomRegistry.updateRoom(id, room);
        }
    });
};


/*
 * Delete Room. Removes a determined room from the data base
 * and asks cloudHandler to remove it from erizoController.
 */
exports.deleteRoom = function (req, res) {
    doInit(req, function (currentRoom) {
        if (req.service === undefined) {
            res.status(401).send('Client unathorized');
        } else if (currentRoom === undefined) {
            log.info('message: deleteRoom - room does not exist, roomId: ' + req.params.room);
            res.status(404).send('Room does not exist');
        } else {
            var id = '' + currentRoom._id;
            roomRegistry.removeRoom(id);
            cloudHandler.deleteRoom(id, function () {});
            serviceRoomRegistry.removeServiceRoom(req.service, currentRoom, function () {});
            res.send('Room deleted');
        }
    });
};

exports.findOrCreateRoom = function (req, res) {
    if (req.service === undefined) {
        return res.status(401).send('Client unathorized');
    }

    var roomName = req.body.roomName;

    roomRegistry.findOrCreateRoom(roomName, function(err, result) {
        if (err) {
            log.warn('message: findOrCreateRoom error, ' + logger.objectToLog(err));
            return res.status(500).send('Failed to find room by name');
        }

        var { room, isNew } = result;

        if (isNew) {
            serviceRoomRegistry.addServiceRoom(req.service, room, function() {});
        }

        res.json(room);
    });
};
