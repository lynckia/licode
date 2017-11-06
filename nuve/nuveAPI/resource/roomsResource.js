/*global exports, require*/
'use strict';
var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var async = require ('async');
var logger = require('./../logger').logger;
var log = logger.getLogger('RoomsResource');

var addRoom = async.queue((task, done) => {
  serviceRegistry.getService(task.currentService._id, function (serv) {
    serv.rooms.push(task.result);
    serviceRegistry.updateService(serv, function() {
      log.info('message: createRoom success, roomName: ' + task.req.body.name + ', serviceId: ' +
               serv.name + ', p2p: ' + !!task.req.body.options.p2p + ', roomId: ', task.result._id);
      task.callback(task.result);
      done();
    });
  });
});

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

    if (req.body.options.test) {
        if (currentService.testRoom !== undefined) {
            log.info('message: testRoom already exists, serviceId: ' + currentService.name);
            res.send(currentService.testRoom);
        } else {
            room = {name: 'testRoom'};
            roomRegistry.addRoom(room, function (result) {
                currentService.testRoom = result;
                currentService.rooms.push(result);
                serviceRegistry.updateService(currentService);
                log.info('message: testRoom created, serviceId: ' + currentService.name);
                res.send(result);
            });
        }
    } else {
        room = {name: req.body.name};
        room.p2p = !!req.body.options.p2p;
        if (req.body.options.data) {
            room.data = req.body.options.data;
        }
        if (typeof req.body.options.mediaConfiguration === 'string') {
            room.mediaConfiguration = req.body.options.mediaConfiguration;
        }
        roomRegistry.addRoom(room, function (result) {
          addRoom.push({result: result,
                        currentService: currentService,
                        req: req,
                        callback: function(result) {
                          res.send(result);
                        }
          });
        });
    }
};

/*
 * Get Rooms. Represent a list of rooms for a determined service.
 */
exports.represent = function (req, res) {
    var currentService = req.service;
    if (currentService === undefined) {
        res.status(404).send('Service not found');
        return;
    }
    log.info('message: representRooms, serviceId: ' + currentService._id);

    res.send(currentService.rooms);
};
