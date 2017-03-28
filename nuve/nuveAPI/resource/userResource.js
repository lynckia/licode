/*global exports, require*/
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('UserResource');

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
 * Get User. Represent a determined user of a room.
 * This is consulted to erizoController using RabbitMQ RPC call.
 */
exports.getUser = function (req, res) {
    doInit(req, function (currentService, currentRoom) {

        if (currentService === undefined) {
            res.status(404).send('Service not found');
            return;
        } else if (currentRoom === undefined) {
            log.info('message: getUser - room not found, roomId: ' + req.params.room);
            res.status(404).send('Room does not exist');
            return;
        }

        var user = req.params.user;

        cloudHandler.getUsersInRoom(currentRoom._id, function (users) {
            if (users === 'error') {
                res.status(503).send('CloudHandler does not respond');
                return;
            }
            for (var index in users){

                if (users[index].name === user){
                    log.info('message: getUser success, ' + logger.objectToLog(user));
                    res.send(users[index]);
                    return;
                }
            }
            log.error('message: getUser user not found, userId: ' + req.params.user);
            res.status(404).send('User does not exist');
            return;
        });
    });
};

/*
 * Delete User. Removes a determined user from a room.
 * This order is sent to erizoController using RabbitMQ RPC call.
 */
exports.deleteUser = function (req, res) {
    doInit(req, function (currentService, currentRoom) {

        if (currentService === undefined) {
            res.status(404).send('Service not found');
            return;
        } else if (currentRoom === undefined) {
            log.info('message: deleteUser - room not found, roomId: ' + req.params.room);
            res.status(404).send('Room does not exist');
            return;
        }

        var user = req.params.user;

        cloudHandler.deleteUser(user, currentRoom._id, function(result){
            if (result !== 'Success'){
                res.status(404).send(result);
            } else {
                res.send(result);
                return;
            }
        });
        //Consultar RabbitMQ
    });
};