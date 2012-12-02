/*global exports, require, console*/
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');

var service;

/*
 * Gets the service for the proccess of the request.
 */
var doInit = function () {
    "use strict";
    service = require('./../auth/nuveAuthenticator').service;
};

/*
 * Post Room. Creates a new room for a determined service.
 */
exports.createRoom = function (req, res) {
    "use strict";

    var room;

    doInit();

    if (service === undefined) {
        res.send('Service not found', 404);
        return;
    }
    if (req.body.name === undefined) {
        console.log('Invalid room');
        res.send('Invalid room', 404);
        return;
    }

    if (req.body.options === 'test') {
        if (service.testRoom !== undefined) {
            console.log('TestRoom already exists for service', service.name);
            res.send(service.testRoom);
        } else {
            room = {name: 'testRoom'};
            roomRegistry.addRoom(room, function (result) {
                service.testRoom = result;
                service.rooms.push(result);
                serviceRegistry.updateService(service);
                console.log('TestRoom created for service', service.name);
                res.send(result);
            });
        }
    } else {
        room = {name: req.body.name};
        roomRegistry.addRoom(room, function (result) {
            service.rooms.push(result);
            serviceRegistry.updateService(service);
            console.log('Room created:', req.body.name, 'for service', service.name);
            res.send(result);
        });
    }
};

/*
 * Get Rooms. Represent a list of rooms for a determined service.
 */
exports.represent = function (req, res) {
    "use strict";

    doInit();
    if (service === undefined) {
        res.send('Service not found', 404);
        return;
    }
    console.log('Representing rooms for service ', service._id);

    res.send(service.rooms);
};
