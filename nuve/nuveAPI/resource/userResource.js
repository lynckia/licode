/*global exports, require, console, Buffer*/
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('../cloudHandler');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger("UserResource");

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
 * Get User. Represent a determined user of a room. This is consulted to erizoController using RabbitMQ RPC call.
 */
exports.getUser = function (req, res) {
    "use strict";

    doInit(req.params.room, function () {

        if (currentService === undefined) {
            res.send('Service not found', 404);
            return;
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
            return;
        }

        var user = req.params.user;

        
        cloudHandler.getUsersInRoom (currentRoom._id, function (users) {
            if (users === 'error') {
                res.send('CloudHandler does not respond', 401);
                return;
            }
            for (var index in users){
                
                if (users[index].name === user){
                    log.info('Found user', user);
                    res.send(users[index]);
                    return;
                }

            }
            log.error('User', req.params.user, 'does not exist')
            res.send('User does not exist', 404);
            return;
            

        });

    });
};

/*
 * Delete User. Removes a determined user from a room. This order is sent to erizoController using RabbitMQ RPC call.
 */
exports.deleteUser = function (req, res) {
    "use strict";

    doInit(req.params.room, function () {

        if (currentService === undefined) {
            res.send('Service not found', 404);
            return;
        } else if (currentRoom === undefined) {
            log.info('Room ', req.params.room, ' does not exist');
            res.send('Room does not exist', 404);
            return;
        }

        var user = req.params.user;
        


        cloudHandler.deleteUser (user, currentRoom._id, function(result){
            if(result === 'User does not exist'){
                res.send(result, 404);
            }
            else {
                res.send(result);
                return;
            }
        });
        

        //Consultar RabbitMQ
    });
};