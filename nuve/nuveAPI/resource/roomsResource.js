/*global exports, require*/
'use strict';
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var serviceRoomRegistry = require('./../mdb/serviceRoomRegistry');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('RoomsResource');

/*
 * Post Room. Creates a new room for a determined service.
 */
exports.createRoom = function (req, res) {
    var room;

    var currentService = req.service;

    if (currentService === undefined) {
        res.status(404).send('Service not found');
        return;
    }
    if (req.body.name === undefined) {
        log.info('message: createRoom - invalid room name');
        res.status(400).send('Invalid room');
        return;
    }

    req.body.options = req.body.options || {};

    room = { name: req.body.name };

    if (req.body.options.p2p) {
        room.p2p = true;
    }
    if (req.body.options.data) {
        room.data = req.body.options.data;
    }
    roomRegistry.addRoom(room, function (result) {
        serviceRoomRegistry.addServiceRoom(req.service, room, function() {});
        log.info('message: createRoom success, roomName:' + req.body.name + ', serviceId: ' +
                 currentService.name + ', p2p: ' + room.p2p);
        res.send(result);
    });
};

exports.represent = function (req, res) {
    // I think we don't need this
    res.send([]);
};
