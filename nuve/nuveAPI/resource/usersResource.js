/*global exports, require*/
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('UsersResource');

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (req, callback) {
    var currentService = req.service;

    serviceRegistry.getRoomForService(req.params.room, currentService, function (room) {
        callback(currentService, room);
    });

};

/*
 * Get Users. Represent a list of users of a determined room. This is consulted to cloudHandler.
 */
exports.getList = function (req, res) {
    doInit(req, function (currentService, currentRoom) {

        if (currentService === undefined) {
            res.send('Service not found', 404);
            return;
        } else if (currentRoom === undefined) {
            log.info('message: getUserList - room not found, roomId: ' + req.params.room);
            res.send('Room does not exist', 404);
            return;
        }

        log.info('message: getUsersList sucess, roomId: ' + currentRoom._id +
                 ', serviceId: ' + currentService._id);
        cloudHandler.getUsersInRoom(currentRoom._id, function (users) {
            if (users === 'error') {
                res.send('CloudHandler does not respond', 401);
                return;
            }
            res.send(users);
        });

    });

};
